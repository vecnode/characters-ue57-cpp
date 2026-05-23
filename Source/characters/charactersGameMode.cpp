// Copyright Epic Games, Inc. All Rights Reserved.

#include "charactersGameMode.h"
#include "charactersMHPlayer.h"
#include "charactersPlayerController.h"
#include "characters.h"
#include "EngineUtils.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/World.h"

namespace
{
	bool IsUsablePlacedPawn(const APawn* Pawn)
	{
		return IsValid(Pawn)
			&& Pawn->GetController() == nullptr
			&& !Pawn->IsPlayerControlled()
			&& !Pawn->IsA<ASpectatorPawn>();
	}

	bool MatchesPreferredName(const APawn* Pawn, const FString& PreferredName)
	{
		if (!Pawn || PreferredName.IsEmpty())
		{
			return false;
		}

		if (Pawn->GetName().Equals(PreferredName, ESearchCase::IgnoreCase)
			|| Pawn->GetFName().ToString().Equals(PreferredName, ESearchCase::IgnoreCase))
		{
			return true;
		}

#if WITH_EDITOR
		if (Pawn->GetActorLabel().Equals(PreferredName, ESearchCase::IgnoreCase))
		{
			return true;
		}
#endif

		return false;
	}

	void SetupProxyVisualsFromSourceActor(AActor* SourceActor, APawn* TargetPawn)
	{
		if (!SourceActor || !TargetPawn)
		{
			return;
		}

		ACharacter* TargetCharacter = Cast<ACharacter>(TargetPawn);
		USkeletalMeshComponent* DriverMesh = TargetCharacter ? TargetCharacter->GetMesh() : nullptr;

		// Keep the original actor as the visible character proxy and preserve its placed offset on attach.
		SourceActor->AttachToActor(TargetPawn, FAttachmentTransformRules::KeepWorldTransform);
		SourceActor->SetActorHiddenInGame(false);
		SourceActor->SetActorEnableCollision(false);
		SourceActor->SetActorTickEnabled(true);

		if (!DriverMesh)
		{
			return;
		}

		TInlineComponentArray<USkeletalMeshComponent*> SourceMeshes;
		SourceActor->GetComponents(SourceMeshes);

		// Configure the hidden pawn mesh as animation driver using a source mesh from MAIN_CHARACTER.
		USkeletalMeshComponent* SourceDriver = nullptr;
		for (USkeletalMeshComponent* SourceMesh : SourceMeshes)
		{
			if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset())
			{
				continue;
			}

			const bool bHasAnimClass = (SourceMesh->GetAnimClass() != nullptr);
			if (!SourceDriver || bHasAnimClass)
			{
				SourceDriver = SourceMesh;
			}

			if (bHasAnimClass)
			{
				break;
			}
		}

		if (SourceDriver)
		{
			DriverMesh->SetSkeletalMeshAsset(SourceDriver->GetSkeletalMeshAsset());
			DriverMesh->SetRelativeTransform(SourceDriver->GetRelativeTransform());
			DriverMesh->SetAnimationMode(SourceDriver->GetAnimationMode());
			if (SourceDriver->GetAnimClass())
			{
				DriverMesh->SetAnimInstanceClass(SourceDriver->GetAnimClass());
			}
		}

		DriverMesh->SetVisibility(false, true);
		DriverMesh->SetHiddenInGame(true, true);
		DriverMesh->SetOwnerNoSee(true);
		DriverMesh->SetOnlyOwnerSee(false);
		DriverMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

		// DEBUG: log every source mesh component found and whether it receives leader-pose.
		UE_LOG(Logcharacters, Log,
			TEXT("SetupProxyVisuals: DriverMesh mesh='%s' anim='%s' skeleton='%s'"),
			*GetNameSafe(DriverMesh->GetSkeletalMeshAsset()),
			*GetNameSafe(DriverMesh->GetAnimClass()),
			DriverMesh->GetSkeletalMeshAsset() ? *GetNameSafe(DriverMesh->GetSkeletalMeshAsset()->GetSkeleton()) : TEXT("none"));

		// Make compatible source meshes follow the pawn animation driver.
		if (DriverMesh->GetSkeletalMeshAsset() && DriverMesh->GetSkeletalMeshAsset()->GetSkeleton())
		{
			USkeleton* DriverSkeleton = DriverMesh->GetSkeletalMeshAsset()->GetSkeleton();
			for (USkeletalMeshComponent* SourceMesh : SourceMeshes)
			{
				if (!SourceMesh || !SourceMesh->GetSkeletalMeshAsset())
				{
					continue;
				}

				USkeleton* SourceSkeleton = SourceMesh->GetSkeletalMeshAsset()->GetSkeleton();
				const bool bSameSkelly = (SourceSkeleton == DriverSkeleton);
				if (bSameSkelly)
				{
					SourceMesh->SetLeaderPoseComponent(DriverMesh);
				}

				UE_LOG(Logcharacters, Log,
					TEXT("SetupProxyVisuals:   Mesh='%s' comp='%s' skeleton='%s' HasAnimClass=%d LeaderPoseSet=%d"),
					*GetNameSafe(SourceMesh->GetSkeletalMeshAsset()),
					*GetNameSafe(SourceMesh),
					*GetNameSafe(SourceSkeleton),
					SourceMesh->GetAnimClass() != nullptr ? 1 : 0,
					bSameSkelly ? 1 : 0);
			}
		}
	}
}

AcharactersGameMode::AcharactersGameMode()
{
	// Keep a native pawn as spawn fallback; placed actors are preferred (see RestartPlayer).
	DefaultPawnClass = AcharactersMHPlayer::StaticClass();
	PlayerControllerClass = AcharactersPlayerController::StaticClass();
}

void AcharactersGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Always attempt possession logic for players when PIE starts.
	RestartPlayer(NewPlayer);
}

APawn* AcharactersGameMode::FindCachedBridgePawn(AController* Controller)
{
	if (!Controller)
	{
		return nullptr;
	}

	TWeakObjectPtr<APawn>* FoundPawnPtr = CachedBridgePawns.Find(Controller);
	if (!FoundPawnPtr)
	{
		return nullptr;
	}

	APawn* CachedPawn = FoundPawnPtr->Get();
	if (!IsValid(CachedPawn) || CachedPawn->GetWorld() != GetWorld())
	{
		CachedBridgePawns.Remove(Controller);
		return nullptr;
	}

	return CachedPawn;
}

void AcharactersGameMode::CacheBridgePawn(AController* Controller, APawn* Pawn)
{
	if (!Controller || !Pawn)
	{
		return;
	}

	CachedBridgePawns.FindOrAdd(Controller) = Pawn;
	Pawn->OnDestroyed.AddUniqueDynamic(this, &AcharactersGameMode::HandleCachedBridgeDestroyed);
}

void AcharactersGameMode::HandleCachedBridgeDestroyed(AActor* DestroyedActor)
{
	for (auto It = CachedBridgePawns.CreateIterator(); It; ++It)
	{
		const bool bInvalidController = !It.Key().IsValid();
		const APawn* CachedPawn = It.Value().Get();
		const bool bInvalidPawn = (CachedPawn == nullptr);
		const bool bDestroyedMatch = (CachedPawn == DestroyedActor);

		if (bInvalidController || bInvalidPawn || bDestroyedMatch)
		{
			It.RemoveCurrent();
		}
	}
}

void AcharactersGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	AActor* PreferredNamedNonPawnActor = nullptr;

	// Resolve optional class filter for placed pawn selection.
	UClass* PreferredClass = nullptr;
	if (!PreferredPlacedPawnClass.IsNull())
	{
		PreferredClass = PreferredPlacedPawnClass.LoadSynchronous();
	}

	APawn* NamedPawn = nullptr;
	APawn* TaggedPawn = nullptr;
	APawn* PreferredClassPawn = nullptr;
	APawn* FirstUsablePawn = nullptr;

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsUsablePlacedPawn(Candidate))
		{
			continue;
		}

		if (!NamedPawn && MatchesPreferredName(Candidate, PreferredPlacedPawnName))
		{
			NamedPawn = Candidate;
			break;
		}

		if (Candidate->ActorHasTag(PreferredPlacedPawnTag))
		{
			TaggedPawn = Candidate;
		}

		if (!PreferredClassPawn && PreferredClass && Candidate->IsA(PreferredClass))
		{
			PreferredClassPawn = Candidate;
		}

		if (!FirstUsablePawn)
		{
			FirstUsablePawn = Candidate;
		}
	}

	APawn* SelectedPawn = NamedPawn
		? NamedPawn
		: (TaggedPawn ? TaggedPawn : (PreferredClassPawn ? PreferredClassPawn : FirstUsablePawn));

	if (SelectedPawn)
	{
		NewPlayer->Possess(SelectedPawn);
		UE_LOG(Logcharacters, Log,
			TEXT("charactersGameMode: Possessed placed pawn '%s' (%s). Name='%s' Tag='%s' ClassFilter=%s"),
			*SelectedPawn->GetName(),
			*SelectedPawn->GetClass()->GetName(),
			*PreferredPlacedPawnName,
			*PreferredPlacedPawnTag.ToString(),
			*GetNameSafe(PreferredClass));
		return;
	}

	if (!PreferredPlacedPawnName.IsEmpty())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			AActor* ActorCandidate = *It;
			if (!IsValid(ActorCandidate) || ActorCandidate->IsA<APawn>())
			{
				continue;
			}

			const bool bMatchesObjectName = ActorCandidate->GetName().Equals(PreferredPlacedPawnName, ESearchCase::IgnoreCase);
#if WITH_EDITOR
			const bool bMatchesLabel = ActorCandidate->GetActorLabel().Equals(PreferredPlacedPawnName, ESearchCase::IgnoreCase);
#else
			const bool bMatchesLabel = false;
#endif

			if (bMatchesObjectName || bMatchesLabel)
			{
				PreferredNamedNonPawnActor = ActorCandidate;
				break;
			}
		}
	}

	if (PreferredNamedNonPawnActor && bSpawnFromPreferredNamedActorIfNotPawn)
	{
		if (APawn* CachedBridgePawn = FindCachedBridgePawn(NewPlayer))
		{
			NewPlayer->Possess(CachedBridgePawn);
			UE_LOG(Logcharacters, Log,
				TEXT("charactersGameMode: Reused cached bridge pawn '%s' for controller '%s'."),
				*CachedBridgePawn->GetName(),
				*GetNameSafe(NewPlayer));
			return;
		}

		UClass* SpawnClass = DefaultPawnClass;
		if (!SpawnClass || !SpawnClass->IsChildOf(APawn::StaticClass()))
		{
			UE_LOG(Logcharacters, Error,
				TEXT("charactersGameMode: Cannot bridge non-pawn actor '%s' because DefaultPawnClass is invalid."),
				*PreferredNamedNonPawnActor->GetName());
		}
		else
		{
			TWeakObjectPtr<APawn> PreviousBridgePtr;
			if (TWeakObjectPtr<APawn>* ExistingBridgePtr = CachedBridgePawns.Find(NewPlayer))
			{
				PreviousBridgePtr = *ExistingBridgePtr;
				CachedBridgePawns.Remove(NewPlayer);
			}

			if (APawn* PreviousBridgePawn = PreviousBridgePtr.Get())
			{
				PreviousBridgePawn->Destroy();
			}

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.Owner = NewPlayer;

			const FTransform SpawnTransform = PreferredNamedNonPawnActor->GetActorTransform();
			if (UWorld* World = GetWorld())
			{
				if (APawn* SpawnedPawn = World->SpawnActor<APawn>(SpawnClass, SpawnTransform, SpawnParams))
				{
					SetupProxyVisualsFromSourceActor(PreferredNamedNonPawnActor, SpawnedPawn);
					CacheBridgePawn(NewPlayer, SpawnedPawn);

					NewPlayer->Possess(SpawnedPawn);
					UE_LOG(Logcharacters, Warning,
						TEXT("charactersGameMode: Preferred actor '%s' (%s) is not a Pawn. Spawned '%s', configured proxy visuals, and possessed it."),
						*PreferredNamedNonPawnActor->GetName(),
						*PreferredNamedNonPawnActor->GetClass()->GetName(),
						*SpawnedPawn->GetName());
					return;
				}
			}

			UE_LOG(Logcharacters, Error,
				TEXT("charactersGameMode: Failed to spawn bridge pawn from non-pawn actor '%s'."),
				*PreferredNamedNonPawnActor->GetName());
		}
	}

	if (bAllowSpawnFallback)
	{
		UE_LOG(Logcharacters, Warning,
			TEXT("charactersGameMode: No placed pawn found (Name='%s', Tag='%s'). Falling back to spawn."),
			*PreferredPlacedPawnName,
			*PreferredPlacedPawnTag.ToString());
		Super::RestartPlayer(NewPlayer);
		return;
	}

	if (PreferredNamedNonPawnActor)
	{
		UE_LOG(Logcharacters, Error,
			TEXT("charactersGameMode: Actor '%s' matched preferred name '%s' but is class '%s' (not a Pawn). Reparent to Pawn/Character to allow direct possession."),
			*PreferredNamedNonPawnActor->GetName(),
			*PreferredPlacedPawnName,
			*PreferredNamedNonPawnActor->GetClass()->GetName());
	}

	UE_LOG(Logcharacters, Error,
		TEXT("charactersGameMode: No placed pawn found (Name='%s', Tag='%s'). Spawn fallback is disabled; no pawn will be spawned."),
		*PreferredPlacedPawnName,
		*PreferredPlacedPawnTag.ToString());
}

void AcharactersGameMode::Logout(AController* Exiting)
{
	if (Exiting)
	{
		if (TWeakObjectPtr<APawn>* BridgePawnPtr = CachedBridgePawns.Find(Exiting))
		{
			if (APawn* BridgePawn = BridgePawnPtr->Get())
			{
				BridgePawn->Destroy();
			}
			CachedBridgePawns.Remove(Exiting);
		}
	}

	Super::Logout(Exiting);
}

void AcharactersGameMode::BeginPlay()
{
	Super::BeginPlay();
}

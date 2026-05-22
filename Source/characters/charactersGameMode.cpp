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

	void HideAllPawnSkeletalMeshes(APawn* Pawn)
	{
		if (!Pawn)
		{
			return;
		}

		TInlineComponentArray<USkeletalMeshComponent*> MeshComponents;
		Pawn->GetComponents(MeshComponents);
		for (USkeletalMeshComponent* MeshComp : MeshComponents)
		{
			if (!MeshComp)
			{
				continue;
			}

			MeshComp->SetVisibility(false, true);
			MeshComp->SetHiddenInGame(true, true);
			MeshComp->SetOwnerNoSee(true);
			MeshComp->SetOnlyOwnerSee(false);
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
		UClass* SpawnClass = DefaultPawnClass;
		if (!SpawnClass || !SpawnClass->IsChildOf(APawn::StaticClass()))
		{
			UE_LOG(Logcharacters, Error,
				TEXT("charactersGameMode: Cannot bridge non-pawn actor '%s' because DefaultPawnClass is invalid."),
				*PreferredNamedNonPawnActor->GetName());
		}
		else
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			SpawnParams.Owner = NewPlayer;

			const FTransform SpawnTransform = PreferredNamedNonPawnActor->GetActorTransform();
			if (UWorld* World = GetWorld())
			{
				if (APawn* SpawnedPawn = World->SpawnActor<APawn>(SpawnClass, SpawnTransform, SpawnParams))
				{
					// Keep the original non-pawn character visual in the world and make it follow the pawn.
					// This preserves full MetaHuman component setup (body/face/hair/clothes) without partial copy artifacts.
					PreferredNamedNonPawnActor->AttachToActor(SpawnedPawn, FAttachmentTransformRules::KeepWorldTransform);
					PreferredNamedNonPawnActor->SetActorHiddenInGame(false);
					PreferredNamedNonPawnActor->SetActorTickEnabled(true);

					// Avoid visual duplicates: hide skeletal meshes on the spawned control pawn.
					HideAllPawnSkeletalMeshes(SpawnedPawn);

					NewPlayer->Possess(SpawnedPawn);
					UE_LOG(Logcharacters, Warning,
						TEXT("charactersGameMode: Preferred actor '%s' (%s) is not a Pawn. Spawned '%s', attached preferred actor for visuals, and possessed the pawn."),
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

void AcharactersGameMode::BeginPlay()
{
	Super::BeginPlay();
}

// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.

#include "Gameplay/Modes/MetaAgentGameMode.h"
#include "UI/HUD/MetaAgentHUD.h"
#include "Gameplay/Characters/MetaAgentMHPlayer.h"
#include "Gameplay/Controllers/MetaAgentPlayerController.h"
#include "Core/MetaAgent.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"
#include "Engine/World.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshBoundsVolume.h"

namespace
{
	bool IsSelectablePlacedPawn(const APawn* Pawn)
	{
		return IsValid(Pawn)
			&& !Pawn->IsA<ASpectatorPawn>();
	}

	bool CanControllerPossessPlacedPawn(const APawn* Pawn, const AController* RequestingController)
	{
		if (!IsValid(Pawn))
		{
			return false;
		}

		const AController* ExistingController = Pawn->GetController();
		if (!ExistingController)
		{
			return true;
		}

		if (ExistingController == RequestingController)
		{
			return true;
		}

		// Allow reclaiming pawns controlled by AI/non-player controllers after BP reparenting.
		return !ExistingController->IsPlayerController();
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
}

AMetaAgentGameMode::AMetaAgentGameMode()
{
	// Keep a native pawn as spawn fallback; placed actors are preferred (see RestartPlayer).
	DefaultPawnClass = AMetaAgentMHPlayer::StaticClass();
	PlayerControllerClass = AMetaAgentPlayerController::StaticClass();
	HUDClass = AMetaAgentHUD::StaticClass();
}

void AMetaAgentGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!IsMetaAgentRuntimeActive())
	{
		return;
	}

	// Always attempt possession logic for players when PIE starts.
	RestartPlayer(NewPlayer);
}

void AMetaAgentGameMode::RestartPlayer(AController* NewPlayer)
{
	if (!IsMetaAgentRuntimeActive())
	{
		return;
	}

	if (!NewPlayer)
	{
		return;
	}

	if (APawn* ExistingPawn = NewPlayer->GetPawn())
	{
		if (!ExistingPawn->IsA<ASpectatorPawn>())
		{
			UE_LOG(LogMetaAgent, Log,
				TEXT("MetaAgentGameMode: Controller '%s' already has pawn '%s'. Keeping current possession."),
				*GetNameSafe(NewPlayer),
				*ExistingPawn->GetName());
			return;
		}
	}

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
		if (!IsSelectablePlacedPawn(Candidate))
		{
			continue;
		}

		const bool bCanPossessCandidate = CanControllerPossessPlacedPawn(Candidate, NewPlayer);

		if (!NamedPawn && bCanPossessCandidate && MatchesPreferredName(Candidate, PreferredPlacedPawnName))
		{
			NamedPawn = Candidate;
			break;
		}

		if (!TaggedPawn && bCanPossessCandidate && Candidate->ActorHasTag(PreferredPlacedPawnTag))
		{
			TaggedPawn = Candidate;
		}

		if (!PreferredClassPawn && bCanPossessCandidate && PreferredClass && Candidate->IsA(PreferredClass))
		{
			PreferredClassPawn = Candidate;
		}

		if (!FirstUsablePawn && bCanPossessCandidate)
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
		UE_LOG(LogMetaAgent, Log,
			TEXT("MetaAgentGameMode: Possessed placed pawn '%s' (%s). Name='%s' Tag='%s' ClassFilter=%s"),
			*SelectedPawn->GetName(),
			*SelectedPawn->GetClass()->GetName(),
			*PreferredPlacedPawnName,
			*PreferredPlacedPawnTag.ToString(),
			*GetNameSafe(PreferredClass));
		return;
	}

	if (bAllowSpawnFallback)
	{
		UE_LOG(LogMetaAgent, Log,
			TEXT("MetaAgentGameMode: No placed pawn found (Name='%s', Tag='%s'). Falling back to normal spawn."),
			*PreferredPlacedPawnName,
			*PreferredPlacedPawnTag.ToString());
		Super::RestartPlayer(NewPlayer);
		return;
	}

	UE_LOG(LogMetaAgent, Error,
		TEXT("MetaAgentGameMode: No placed pawn found (Name='%s', Tag='%s'). Place a Pawn/Character with that name/tag, or enable spawn fallback."),
		*PreferredPlacedPawnName,
		*PreferredPlacedPawnTag.ToString());
}

void AMetaAgentGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (!IsMetaAgentRuntimeActive())
	{
		return;
	}

	EnsureAutoNavMeshBounds();
}

void AMetaAgentGameMode::EnsureAutoNavMeshBounds()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ANavMeshBoundsVolume* PreferredVolume = nullptr;
	ANavMeshBoundsVolume* FirstVolume = nullptr;

	for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
	{
		ANavMeshBoundsVolume* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (!FirstVolume)
		{
			FirstVolume = Candidate;
		}

		if (!PreferredNavMeshBoundsName.IsEmpty())
		{
			const bool bNameMatch = Candidate->GetName().Equals(PreferredNavMeshBoundsName, ESearchCase::IgnoreCase)
				|| Candidate->GetFName().ToString().Equals(PreferredNavMeshBoundsName, ESearchCase::IgnoreCase);

			bool bLabelMatch = false;
#if WITH_EDITOR
			bLabelMatch = Candidate->GetActorLabel().Equals(PreferredNavMeshBoundsName, ESearchCase::IgnoreCase);
#endif

			if (bNameMatch || bLabelMatch)
			{
				PreferredVolume = Candidate;
				break;
			}
		}
	}

	ANavMeshBoundsVolume* SelectedVolume = PreferredVolume ? PreferredVolume : FirstVolume;
	if (SelectedVolume)
	{
		if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			NavigationSystem->OnNavigationBoundsUpdated(SelectedVolume);
			NavigationSystem->Build();

			UE_LOG(LogMetaAgent, Log,
				TEXT("MetaAgentGameMode: Using NavMeshBoundsVolume '%s' for navigation build.%s"),
				*GetNameSafe(SelectedVolume),
				PreferredVolume ? TEXT("") : TEXT(" (Preferred name not found, using first available volume.)"));
		}
		else
		{
			UE_LOG(LogMetaAgent, Error,
				TEXT("MetaAgentGameMode: NavigationSystemV1 unavailable while using existing NavMeshBoundsVolume '%s'."),
				*GetNameSafe(SelectedVolume));
		}

		return;
	}

	if (!bAutoCreateNavMeshBounds)
	{
		UE_LOG(LogMetaAgent, Warning,
			TEXT("MetaAgentGameMode: No NavMeshBoundsVolume found (preferred='%s') and auto-create is disabled."),
			*PreferredNavMeshBoundsName);
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	if (APlayerController* PC = World->GetFirstPlayerController())
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			SpawnLocation = Pawn->GetActorLocation();
		}
	}

	ANavMeshBoundsVolume* NavBoundsVolume = World->SpawnActor<ANavMeshBoundsVolume>(
		ANavMeshBoundsVolume::StaticClass(),
		SpawnLocation,
		FRotator::ZeroRotator);

	if (!NavBoundsVolume)
	{
		UE_LOG(LogMetaAgent, Error, TEXT("MetaAgentGameMode: Failed to spawn runtime NavMeshBoundsVolume."));
		return;
	}

	constexpr float DefaultBrushHalfExtent = 100.0f;
	const FVector SafeExtents(
		FMath::Max(AutoNavMeshExtentXY, 1000.0f),
		FMath::Max(AutoNavMeshExtentXY, 1000.0f),
		FMath::Max(AutoNavMeshExtentZ, 500.0f));

	NavBoundsVolume->SetActorScale3D(SafeExtents / DefaultBrushHalfExtent);

	if (UNavigationSystemV1* NavigationSystem = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		NavigationSystem->OnNavigationBoundsUpdated(NavBoundsVolume);
		NavigationSystem->Build();
		UE_LOG(LogMetaAgent, Log,
			TEXT("MetaAgentGameMode: Spawned runtime NavMeshBoundsVolume at %s with extents XY=%.0f Z=%.0f and triggered nav build."),
			*SpawnLocation.ToCompactString(),
			SafeExtents.X,
			SafeExtents.Z);
	}
	else
	{
		UE_LOG(LogMetaAgent, Error,
			TEXT("MetaAgentGameMode: NavigationSystemV1 unavailable after spawning NavMeshBoundsVolume."));
	}
}


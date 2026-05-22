// Copyright Epic Games, Inc. All Rights Reserved.

#include "charactersGameMode.h"
#include "charactersMHPlayer.h"
#include "charactersPlayerController.h"
#include "characters.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpectatorPawn.h"

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

	// Resolve optional class filter for placed pawn selection.
	UClass* PreferredClass = nullptr;
	if (!PreferredPlacedPawnClass.IsNull())
	{
		PreferredClass = PreferredPlacedPawnClass.LoadSynchronous();
	}

	APawn* TaggedPawn = nullptr;
	APawn* PreferredClassPawn = nullptr;
	APawn* FirstUsablePawn = nullptr;

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		APawn* Candidate = *It;
		if (!IsValid(Candidate))
		{
			continue;
		}

		if (Candidate->GetController() != nullptr || Candidate->IsPlayerControlled())
		{
			continue;
		}

		if (Candidate->IsA<ASpectatorPawn>())
		{
			continue;
		}

		if (Candidate->ActorHasTag(PreferredPlacedPawnTag))
		{
			TaggedPawn = Candidate;
			break;
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

	APawn* SelectedPawn = TaggedPawn ? TaggedPawn : (PreferredClassPawn ? PreferredClassPawn : FirstUsablePawn);

	if (SelectedPawn)
	{
		NewPlayer->Possess(SelectedPawn);
		UE_LOG(Logcharacters, Log,
			TEXT("charactersGameMode: Possessed existing level pawn '%s' (%s). Tag=%s ClassFilter=%s"),
			*SelectedPawn->GetName(),
			*SelectedPawn->GetClass()->GetName(),
			*PreferredPlacedPawnTag.ToString(),
			*GetNameSafe(PreferredClass));
		return;
	}

	// No placed character found — fall back to spawning.
	UE_LOG(Logcharacters, Warning,
		TEXT("charactersGameMode: No unpossessed placed APawn found in level. Falling back to spawn."));
	Super::RestartPlayer(NewPlayer);
}

void AcharactersGameMode::BeginPlay()
{
	Super::BeginPlay();
}

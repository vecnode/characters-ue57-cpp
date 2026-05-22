// Copyright Epic Games, Inc. All Rights Reserved.

#include "charactersGameMode.h"
#include "characters.h"
#include "charactersMHPlayer.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AcharactersGameMode::AcharactersGameMode()
{
	// Always keep a native fallback so PIE never drops to spectator/fly view
	// when a BP asset path is missing or fails to load.
	DefaultPawnClass = AcharactersMHPlayer::StaticClass();

	// Bulletproof default pawn: always try to spawn BP_character2 in gameplay.
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(
		TEXT("/Game/MetaHumans/character2/BP_character2"));

	if (PlayerPawnBPClass.Class != nullptr)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
	else
	{
		UE_LOG(Logcharacters, Warning,
			TEXT("charactersGameMode: Could not load /Game/MetaHumans/character2/BP_character2. Using native AcharactersMHPlayer fallback."));
	}

	// Use the existing third person controller BP so Enhanced Input mapping contexts
	// configured in the editor continue to apply.
	static ConstructorHelpers::FClassFinder<APlayerController> PlayerControllerBPClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonPlayerController"));

	if (PlayerControllerBPClass.Class != nullptr)
	{
		PlayerControllerClass = PlayerControllerBPClass.Class;
	}
}

void AcharactersGameMode::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(Logcharacters, Log,
		TEXT("charactersGameMode: DefaultPawnClass=%s, PlayerControllerClass=%s"),
		*GetNameSafe(DefaultPawnClass),
		*GetNameSafe(PlayerControllerClass));
}

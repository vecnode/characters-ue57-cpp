// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UObject/SoftObjectPtr.h"
#include "charactersGameMode.generated.h"

/**
 *  Runtime game mode that possesses an existing placed character in the level.
 *  Falls back to spawning a new pawn only when no placed character is found.
 */
UCLASS()
class AcharactersGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:

	AcharactersGameMode();

protected:

	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void RestartPlayer(AController* NewPlayer) override;
	virtual void BeginPlay() override;

	/** Optional class filter for placed pawns to possess first. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	TSoftClassPtr<APawn> PreferredPlacedPawnClass;

	/** Optional actor tag used to pick a specific placed pawn (for example: PlayerCharacter). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	FName PreferredPlacedPawnTag = TEXT("PlayerCharacter");
};




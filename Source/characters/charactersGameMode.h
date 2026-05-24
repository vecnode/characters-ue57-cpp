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
	void EnsureAutoNavMeshBounds();

	/** Optional class filter for placed pawns to possess first. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	TSoftClassPtr<APawn> PreferredPlacedPawnClass;

	/** Optional actor tag used to pick a specific placed pawn (for example: PlayerCharacter). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	FName PreferredPlacedPawnTag = TEXT("PlayerCharacter");

	/** Optional actor name/label from the World Outliner to pick first (for example: MAIN_CHARACTER). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	FString PreferredPlacedPawnName = TEXT("MAIN_CHARACTER");

	/** If false, no pawn is spawned when a placed pawn cannot be found. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Player")
	bool bAllowSpawnFallback = false;

	/** If true, spawn a large NavMeshBoundsVolume automatically at runtime. */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Navigation")
	bool bAutoCreateNavMeshBounds = false;

	/** Preferred NavMeshBoundsVolume actor name/label to use and rebuild (for example: MAIN_NAV_MESH). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Navigation")
	FString PreferredNavMeshBoundsName = TEXT("MAIN_NAV_MESH");

	/** Half-extent in X/Y for the auto-created nav bounds volume (in Unreal units). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Navigation", meta = (ClampMin = "1000.0"))
	float AutoNavMeshExtentXY = 500000.0f;

	/** Half-extent in Z for the auto-created nav bounds volume (in Unreal units). */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Navigation", meta = (ClampMin = "500.0"))
	float AutoNavMeshExtentZ = 100000.0f;
};




// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"
#include "charactersPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

/**
 *  Basic PlayerController class for a third person game
 *  Manages input mappings
 */
UCLASS()
class AcharactersPlayerController : public APlayerController
{
	GENERATED_BODY()
	
protected:

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category ="Input|Input Mappings")
	TArray<UInputMappingContext*> DefaultMappingContexts;

	/** Input Mapping Contexts */
	UPROPERTY(EditAnywhere, Category="Input|Input Mappings")
	TArray<UInputMappingContext*> MobileExcludedMappingContexts;

	/** Optional soft reference fallback for default input mapping context. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Input Mappings")
	TSoftObjectPtr<UInputMappingContext> DefaultMappingContextAsset;

	/** Optional soft reference fallback for mouse-look mapping context. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Input Mappings")
	TSoftObjectPtr<UInputMappingContext> MouseLookMappingContextAsset;

	/** Mobile controls widget to spawn */
	UPROPERTY(EditAnywhere, Category="Input|Touch Controls")
	TSubclassOf<UUserWidget> MobileControlsWidgetClass;

	/** Pointer to the mobile controls widget */
	UPROPERTY()
	TObjectPtr<UUserWidget> MobileControlsWidget;

	/** If true, the player will use UMG touch controls even if not playing on mobile platforms */
	UPROPERTY(EditAnywhere, Config, Category = "Input|Touch Controls")
	bool bForceTouchControls = false;

	/** Gameplay initialization */
	virtual void BeginPlay() override;

	/** Called when this controller possesses a new pawn. Sets up third-person camera. */
	virtual void OnPossess(APawn* InPawn) override;

	/** Per-frame camera distance update (mouse wheel zoom). */
	virtual void PlayerTick(float DeltaTime) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Minimum zoom-in camera distance. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MinCameraDistance = 60.0f;

	/** Maximum zoom-out camera distance. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MaxCameraDistance = 500.0f;

	/** Distance change per mouse wheel notch. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MouseWheelZoomStep = 10.0f;

	/** Smoothing speed for zoom interpolation. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float CameraZoomInterpSpeed = 10.0f;

	/** Desired camera boom distance, updated by mouse wheel. */
	float DesiredCameraDistance = -1.0f;

};

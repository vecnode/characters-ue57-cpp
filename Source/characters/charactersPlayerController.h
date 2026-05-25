// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"
#include "charactersPlayerController.generated.h"

class AAIController;
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

	/** Requests application quit when Escape is pressed. */
	void HandleEscapePressed();

	/** Shows a quick HUD debug message when Y is pressed. */
	void HandleYPressed();

	/** Toggles player possession between manual control and runtime AI autopilot. */
	void HandleToggleAutopilotPressed();

	/** Enables AI autopilot over the currently possessed pawn. */
	void EnableAutopilotForCurrentPawn();

	/** Disables AI autopilot and repossesses the last autopilot pawn. */
	void DisableAutopilotAndRepossess();

	/** Returns true if the player should use UMG touch controls */
	bool ShouldUseTouchControls() const;

	/** Minimum zoom-in camera distance. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MinCameraDistance = 40.0f;

	/** Maximum zoom-out camera distance. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MaxCameraDistance = 1500.0f;

	/** Distance change per mouse wheel notch. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float MouseWheelZoomStep = 10.0f;

	/** Smoothing speed for zoom interpolation. */
	UPROPERTY(EditAnywhere, Category = "Camera|Zoom")
	float CameraZoomInterpSpeed = 10.0f;

	/** Desired camera boom distance, updated by mouse wheel. */
	float DesiredCameraDistance = -1.0f;

	/** One-shot runtime diagnostic flag for movement/animation wiring. */
	bool bLoggedMovementAnimDiagnostics = false;

	/** Enables raw keyboard fallback movement in case Enhanced Input mappings fail to initialize. */
	UPROPERTY(EditAnywhere, Category = "Input|Fallback")
	bool bEnableKeyboardFallbackMovement = true;

	/** Enables raw mouse-delta look fallback in case Enhanced Input look mappings fail. */
	UPROPERTY(EditAnywhere, Category = "Input|Fallback")
	bool bEnableMouseFallbackLook = true;

	/** Sensitivity multiplier for raw mouse look fallback. */
	UPROPERTY(EditAnywhere, Category = "Input|Fallback", meta=(ClampMin="0.001", ClampMax="5.0"))
	float MouseFallbackSensitivity = 0.15f;

	/** Tracks whether any mapping context was successfully added this session. */
	bool bAddedAnyMappingContext = false;

	/** True while collecting a short movement telemetry window. */
	bool bMovementProbeActive = false;

	/** Elapsed time for the active movement telemetry window. */
	float MovementProbeElapsed = 0.0f;

	/** Highest sampled horizontal speed in the telemetry window. */
	float MovementProbePeakSpeed2D = 0.0f;

	/** Highest sampled acceleration magnitude in the telemetry window. */
	float MovementProbePeakAcceleration2D = 0.0f;

	/** Sample count collected in the telemetry window. */
	int32 MovementProbeSampleCount = 0;

	/** True after one-time raw key bindings (Escape/J) are installed. */
	bool bUtilityKeysBound = false;

	/** True while the pawn is currently controlled by runtime autopilot AI. */
	bool bAutopilotEnabled = false;

	/** World time (seconds) of the last processed autopilot toggle. */
	float LastAutopilotToggleTimeSeconds = -1000.0f;

	/** Small debounce window to avoid duplicate toggle calls from repeated key binds. */
	UPROPERTY(EditAnywhere, Category = "AI|Autopilot", meta=(ClampMin="0.0", ClampMax="1.0"))
	float AutopilotToggleDebounceSeconds = 0.2f;

	/** Last pawn handed over to autopilot. */
	TWeakObjectPtr<APawn> AutopilotPawn;

	/** Configurable controller class used for autopilot possession. */
	UPROPERTY(EditAnywhere, Category = "AI|Autopilot")
	TSubclassOf<AAIController> AutopilotAIControllerClass;

};

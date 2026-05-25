// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPtr.h"
#include "charactersPlayerController.generated.h"

class AAIController;
class UInputMappingContext;
class UUserWidget;

/**
 * Runtime player controller that owns input setup, camera zoom behavior,
 * possession diagnostics, and optional AI autopilot handoff.
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

	/** Per-frame input fallback, diagnostics, and camera zoom updates. */
	virtual void PlayerTick(float DeltaTime) override;

	/** Input mapping context setup */
	virtual void SetupInputComponent() override;

	/** Requests application quit when Escape is pressed. */
	void HandleEscapePressed();

	/** Shows a quick HUD debug message when Y is pressed. */
	void HandleYPressed();

	/** Toggles player possession between manual control and runtime AI autopilot. */
	void HandleToggleAutopilotPressed();

	/** Bound to V: toggles cinematic orbit camera mode on/off. */
	void HandleToggleCinematicCameraPressed();

	/** Enables cinematic orbit camera mode around the active character. */
	void EnableCinematicCameraMode();

	/** Disables cinematic orbit camera mode and restores normal camera behavior. */
	void DisableCinematicCameraMode();

	/** Returns the best character actor to orbit (player pawn, autopilot pawn, or current target). */
	AActor* ResolveCinematicTargetActor() const;

	/** Resolves the preferred cinematic look-at point (head/chest when available). */
	FVector ResolveCinematicFocusLocation(AActor* TargetActor) const;

	/** Updates runtime cinematic camera transform each tick while mode is active. */
	void UpdateCinematicCamera(float DeltaTime);

	/** Enables AI autopilot over the currently possessed pawn. */
	void EnableAutopilotForCurrentPawn();

	/** Disables AI autopilot and repossesses the last autopilot pawn. */
	void DisableAutopilotAndRepossess();

public:

	/** Blueprint entry point so a UI button can toggle the same cinematic camera mode as V. */
	UFUNCTION(BlueprintCallable, Category = "Camera|Cinematic")
	void ToggleCinematicCameraMode();

protected:

	/** Resets one-shot movement diagnostics after each possession change. */
	void ResetMovementDiagnosticsState();

	/**
	 * Configures spring arm/camera after possession.
	 * Keep camera defaults centralized here for easier camera behavior iteration.
	 */
	void ConfigureCameraForPawn(APawn* InPawn);

	/**
	 * Applies mouse-wheel camera zoom with clamping and interpolation.
	 * This is the primary extension point for camera distance behavior.
	 */
	void ApplyMouseWheelZoom(APawn* ControlledPawn, float DeltaTime);

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

	/** Desired camera boom distance, updated by mouse wheel and smoothed in tick. */
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

	/** Blend time when entering cinematic camera mode. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.0", ClampMax="5.0"))
	float CinematicBlendInSeconds = 0.35f;

	/** Blend time when returning to normal camera. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.0", ClampMax="5.0"))
	float CinematicBlendOutSeconds = 0.35f;

	/** Degrees to orbit around the target during one cinematic pan. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="1.0", ClampMax="360.0"))
	float CinematicPanDegrees = 180.0f;

	/** Time in seconds for a full cinematic pan from 0 to PanDegrees. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.1", ClampMax="30.0"))
	float CinematicPanDurationSeconds = 4.0f;

	/** Extra vertical offset for the look-at target (roughly upper torso). */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="-200.0", ClampMax="400.0"))
	float CinematicLookAtZOffset = 100.0f;

	/** Keep orbiting continuously after the first pan duration completes. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic")
	bool bCinematicContinuousOrbit = true;

	/** Orbit speed multiplier. 0.2 means five times slower than base pan speed. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.05", ClampMax="3.0"))
	float CinematicOrbitSpeedScale = 0.2f;

	/** Swap clockwise/counter-clockwise direction after this many full 360 turns. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="1", ClampMax="20"))
	int32 CinematicTurnsPerDirection = 1;

	/** Desired close-up orbit radius for movie-like framing. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="80.0", ClampMax="500.0"))
	float CinematicCloseOrbitRadius = 105.0f;

	/** How strongly to frame toward the head vs chest (0=chest, 1=head). */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.0", ClampMax="1.0"))
	float CinematicHeadFocusAlpha = 0.80f;

	/** Horizontal handheld sway amplitude in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.0", ClampMax="80.0"))
	float CinematicSwayHorizontalAmplitude = 8.0f;

	/** Vertical handheld sway amplitude in centimeters. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.0", ClampMax="80.0"))
	float CinematicSwayVerticalAmplitude = 4.0f;

	/** Base handheld sway frequency in Hz. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic", meta=(ClampMin="0.1", ClampMax="5.0"))
	float CinematicSwayFrequency = 0.85f;

	/** If true, movement/look input is ignored while cinematic mode is active. */
	UPROPERTY(EditAnywhere, Category = "Camera|Cinematic")
	bool bDisablePlayerInputInCinematic = true;

	/** Runtime cinematic mode state. */
	bool bCinematicCameraModeEnabled = false;

	/** Current pan elapsed time in seconds. */
	float CinematicPanElapsedSeconds = 0.0f;

	/** Orbit radius captured when cinematic mode starts. */
	float CinematicOrbitRadius = 400.0f;

	/** Orbit yaw at start of cinematic mode. */
	float CinematicStartOrbitYawDegrees = 0.0f;

	/** Signed accumulated yaw offset from orbit start. */
	float CinematicOrbitAccumulatedYawDegrees = 0.0f;

	/** Current orbit direction. +1 = counter-clockwise, -1 = clockwise. */
	int32 CinematicOrbitDirectionSign = 1;

	/** Yaw distance traveled in the current direction bucket (degrees). */
	float CinematicDirectionTravelDegrees = 0.0f;

	/** Count of full turns completed in current direction bucket. */
	int32 CinematicCompletedTurnsThisDirection = 0;

	/** Camera height offset relative to target when cinematic mode starts. */
	float CinematicCameraHeightOffset = 120.0f;

	/** Per-session phase offset to avoid overly synthetic periodic movement. */
	float CinematicSwayPhaseOffset = 0.0f;

	/** Actor that camera orbits around while cinematic mode is active. */
	TWeakObjectPtr<AActor> CinematicTargetActor;

	/** View target to restore when cinematic mode ends. */
	TWeakObjectPtr<AActor> PreCinematicViewTarget;

	/** Runtime camera actor used for cinematic orbit view. */
	UPROPERTY(Transient)
	TObjectPtr<ACameraActor> RuntimeCinematicCameraActor;

};

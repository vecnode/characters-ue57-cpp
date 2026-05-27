// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.


#include "Gameplay/Controllers/charactersPlayerController.h"
#include "AIController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "Core/characters.h"
#include "UI/HUD/charactersHUD.h"
#include "Systems/Runtime/charactersGameInstance.h"
#include "Gameplay/AI/charactersWanderAIController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "LevelSequence.h"
#include "MoviePipelineGameOverrideSetting.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "MoviePipelineOutputSetting.h"
#include "MoviePipelinePrimaryConfig.h"
#include "MoviePipelineQueueEngineSubsystem.h"
#include "MoviePipelineExecutor.h"
#include "MoviePipelineSetting.h"
#include "MoviePipelineDeferredPasses.h"
#include "MoviePipelineImageSequenceOutput.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SVirtualJoystick.h"

#if WITH_EDITOR
#include "Recorder/TakeRecorderSubsystem.h"
#include "TakeRecorderSettings.h"
#endif

namespace
{
	UInputMappingContext* ResolveMappingContextWithFallback(
		const TSoftObjectPtr<UInputMappingContext>& SoftReference,
		const TCHAR* LegacyPath,
		const TCHAR* ContextLabel,
		const UObject* Owner)
	{
		if (!SoftReference.IsNull())
		{
			if (UInputMappingContext* LoadedFromSoftRef = SoftReference.LoadSynchronous())
			{
				return LoadedFromSoftRef;
			}

			UE_LOG(Logcharacters, Warning,
				TEXT("'%s' failed to load soft mapping context '%s' for %s."),
				*GetNameSafe(Owner),
				*SoftReference.ToString(),
				ContextLabel);
		}

		if (LegacyPath)
		{
			if (UInputMappingContext* LoadedFromLegacyPath = Cast<UInputMappingContext>(StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, LegacyPath)))
			{
				return LoadedFromLegacyPath;
			}
		}

		UE_LOG(Logcharacters, Warning,
			TEXT("'%s' has no valid mapping context for %s. Configure asset references in Blueprint/Class Defaults."),
			*GetNameSafe(Owner),
			ContextLabel);

		return nullptr;
	}
}

void AcharactersPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	if (!InPawn)
	{
		return;
	}

	ResetMovementDiagnosticsState();
	ConfigureCameraForPawn(InPawn);

	if (ACharacter* PossessedCharacter = Cast<ACharacter>(InPawn))
	{
		TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
		PossessedCharacter->GetComponents(SkeletalMeshes);

		USkeletalMeshComponent* DrivingBodyMesh = nullptr;
		for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
		{
			if (!MeshComp || !MeshComp->GetSkeletalMeshAsset())
			{
				continue;
			}

			if (MeshComp->GetAnimInstance())
			{
				DrivingBodyMesh = MeshComp;
				if (MeshComp->GetFName() == TEXT("Body"))
				{
					break;
				}
			}
		}

		if (DrivingBodyMesh)
		{
			DrivingBodyMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			DrivingBodyMesh->bEnableUpdateRateOptimizations = false;
		}

		for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
		{
			if (!MeshComp)
			{
				continue;
			}

			if (MeshComp->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				UE_LOG(Logcharacters, Warning,
					TEXT("MovementGuard: '%s' mesh '%s' had collision enabled (%d); forcing NoCollision to avoid movement drag."),
					*GetNameSafe(PossessedCharacter),
					*GetNameSafe(MeshComp),
					static_cast<int32>(MeshComp->GetCollisionEnabled()));
				MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}

			if (MeshComp->GetGenerateOverlapEvents())
			{
				MeshComp->SetGenerateOverlapEvents(false);
			}

			if (DrivingBodyMesh && MeshComp != DrivingBodyMesh && MeshComp->GetSkeletalMeshAsset() && DrivingBodyMesh->GetSkeletalMeshAsset())
			{
				const USkeleton* MeshSkeleton = MeshComp->GetSkeletalMeshAsset()->GetSkeleton();
				const USkeleton* BodySkeleton = DrivingBodyMesh->GetSkeletalMeshAsset()->GetSkeleton();
				const bool bSharesBodySkeleton = (MeshSkeleton && BodySkeleton && MeshSkeleton == BodySkeleton);

				if (bSharesBodySkeleton && MeshComp->GetAnimInstance() == nullptr)
				{
					MeshComp->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
					MeshComp->bEnableUpdateRateOptimizations = false;
					MeshComp->SetLeaderPoseComponent(DrivingBodyMesh, true, true);
					UE_LOG(Logcharacters, Warning,
						TEXT("MovementGuard: '%s' mesh '%s' now follows '%s' via LeaderPose (same skeleton, no anim instance, follower ticks pose)."),
						*GetNameSafe(PossessedCharacter),
						*GetNameSafe(MeshComp),
						*GetNameSafe(DrivingBodyMesh));
				}
			}
		}

		if (UCharacterMovementComponent* MovementComp = PossessedCharacter->GetCharacterMovement())
		{
			if (MovementComp->MaxWalkSpeed < 150.0f)
			{
				UE_LOG(Logcharacters, Warning,
					TEXT("MovementGuard: '%s' MaxWalkSpeed was %.2f; clamping to 350.00 to keep locomotion out of idle band."),
					*GetNameSafe(PossessedCharacter),
					MovementComp->MaxWalkSpeed);
				MovementComp->MaxWalkSpeed = 350.0f;
			}

			if (MovementComp->MaxAcceleration < 500.0f)
			{
				UE_LOG(Logcharacters, Warning,
					TEXT("MovementGuard: '%s' MaxAcceleration was %.2f; clamping to 2048.00."),
					*GetNameSafe(PossessedCharacter),
					MovementComp->MaxAcceleration);
				MovementComp->MaxAcceleration = 2048.0f;
			}
		}
	}

	UE_LOG(Logcharacters, Log,
		TEXT("AcharactersPlayerController: Possessed '%s' (%s)."),
		*GetNameSafe(InPawn),
		*InPawn->GetClass()->GetName());
}

void AcharactersPlayerController::ResetMovementDiagnosticsState()
{
	MovementDiagnostics.bLoggedMovementAnimDiagnostics = false;
	MovementDiagnostics.bMovementProbeActive = false;
	MovementDiagnostics.ProbeElapsedSeconds = 0.0f;
	MovementDiagnostics.ProbePeakSpeed2D = 0.0f;
	MovementDiagnostics.ProbePeakAcceleration2D = 0.0f;
	MovementDiagnostics.ProbeSampleCount = 0;
}

void AcharactersPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (CinematicCamera.bModeEnabled)
	{
		UpdateCinematicCamera(DeltaTime);
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn && !CinematicCamera.bModeEnabled)
	{
		return;
	}

	ApplyFallbackMovementInput(ControlledPawn);
	ApplyFallbackLookInput();
	LogMovementAnimationDiagnostics(ControlledPawn);
	UpdateMovementProbe(ControlledPawn, DeltaTime);

	if (ControlledPawn && !CinematicCamera.bModeEnabled)
	{
		ApplyMouseWheelZoom(ControlledPawn, DeltaTime);
	}

	if (Recording.bRenderInProgress && GEngine)
	{
		if (UMoviePipelineQueueEngineSubsystem* RenderSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>())
		{
			if (UMoviePipelineExecutorBase* ActiveExecutor = RenderSubsystem->GetActiveExecutor())
			{
				const float RenderProgress = FMath::Clamp(ActiveExecutor->GetStatusProgress(), 0.0f, 1.0f);
				Recording.RenderStatusText = FString::Printf(TEXT("Rendering: %.0f%%"), RenderProgress * 100.0f);
				Recording.RenderStatusColor = FColor::Yellow;
				UpdateRecordingStatusHud();
			}
		}
	}
}

void AcharactersPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalPlayerController() && !ShouldUseTouchControls())
	{
		FInputModeGameOnly InputMode;
		SetInputMode(InputMode);
		bShowMouseCursor = false;
		SetIgnoreLookInput(false);
		SetIgnoreMoveInput(false);
	}

	// only spawn touch controls on local player controllers
	if (ShouldUseTouchControls() && IsLocalPlayerController())
	{
		// spawn the mobile controls widget
		MobileControlsWidget = CreateWidget<UUserWidget>(this, MobileControlsWidgetClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(Logcharacters, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}

	UpdateRecordingStatusHud();
}

void AcharactersPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		if (!InputFallback.bUtilityKeysBound)
		{
			InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AcharactersPlayerController::HandleEscapePressed);
			InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AcharactersPlayerController::HandleYPressed);
			InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AcharactersPlayerController::HandleToggleAutopilotPressed);
			InputComponent->BindKey(EKeys::U, IE_Pressed, this, &AcharactersPlayerController::HandleRenderRecordedTakePressed);
			InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AcharactersPlayerController::HandleToggleCinematicCameraPressed);
			InputFallback.bUtilityKeysBound = true;
		}
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		InputFallback.bAddedAnyMappingContext = false;

		// Add Input Mapping Contexts
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (DefaultMappingContexts.Num() == 0)
			{
				if (UInputMappingContext* DefaultContext = ResolveMappingContextWithFallback(
					DefaultMappingContextAsset,
					TEXT("/Game/Input/IMC_Default.IMC_Default"),
					TEXT("DefaultMappingContext"),
					this))
				{
					DefaultMappingContexts.Add(DefaultContext);
				}
			}

			if (MobileExcludedMappingContexts.Num() == 0)
			{
				if (UInputMappingContext* MouseContext = ResolveMappingContextWithFallback(
					MouseLookMappingContextAsset,
					TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"),
					TEXT("MouseLookMappingContext"),
					this))
				{
					MobileExcludedMappingContexts.Add(MouseContext);
				}
			}

			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				if (CurrentContext)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
					InputFallback.bAddedAnyMappingContext = true;
				}
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					if (CurrentContext)
					{
						Subsystem->AddMappingContext(CurrentContext, 0);
						InputFallback.bAddedAnyMappingContext = true;
					}
				}
			}

			if (!InputFallback.bAddedAnyMappingContext)
			{
				UE_LOG(Logcharacters, Warning,
					TEXT("AcharactersPlayerController: No input mapping contexts were added. Raw keyboard/mouse fallback remains active."));
			}
		}
		else
		{
			UE_LOG(Logcharacters, Warning,
				TEXT("AcharactersPlayerController: EnhancedInputLocalPlayerSubsystem unavailable. Raw keyboard/mouse fallback remains active."));
		}
	}
}

void AcharactersPlayerController::HandleEscapePressed()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AcharactersPlayerController::HandleYPressed()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	UE_LOG(Logcharacters, Log, TEXT("Y pressed: requesting platform agent toggle."));

	if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	{
		const FString SourceLabel = GIsEditor ? TEXT("unreal-editor") : TEXT("unreal-standalone");
		GI->SendEventToPlatform(TEXT("key_pressed"), TEXT("toggle_agent"), SourceLabel);
	}
}

bool AcharactersPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

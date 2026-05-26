// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.


#include "charactersPlayerController.h"
#include "AIController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "characters.h"
#include "charactersHUD.h"
#include "charactersGameInstance.h"
#include "charactersWanderAIController.h"
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
	bool TryGetFirstValidSocketLocation(
		const USkeletalMeshComponent* Mesh,
		const TArray<FName>& CandidateSockets,
		FVector& OutLocation)
	{
		if (!Mesh)
		{
			return false;
		}

		for (const FName& SocketName : CandidateSockets)
		{
			if (Mesh->DoesSocketExist(SocketName))
			{
				OutLocation = Mesh->GetSocketLocation(SocketName);
				return true;
			}
		}

		return false;
	}

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
	bLoggedMovementAnimDiagnostics = false;
	bMovementProbeActive = false;
	MovementProbeElapsed = 0.0f;
	MovementProbePeakSpeed2D = 0.0f;
	MovementProbePeakAcceleration2D = 0.0f;
	MovementProbeSampleCount = 0;
}

void AcharactersPlayerController::ConfigureCameraForPawn(APawn* InPawn)
{
	if (!InPawn)
	{
		return;
	}

	// Works for any pawn that exposes a spring arm + camera pair.
	USpringArmComponent* SpringArm = InPawn->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* FollowCam = InPawn->FindComponentByClass<UCameraComponent>();

	if (SpringArm)
	{
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bDoCollisionTest = false;
		DesiredCameraDistance = FMath::Clamp(SpringArm->TargetArmLength, MinCameraDistance, MaxCameraDistance);
	}

	if (FollowCam)
	{
		FollowCam->bUsePawnControlRotation = false;
		FollowCam->Activate();
	}

	// Disable auto camera management so UE does not fight our view target.
	bAutoManageActiveCameraTarget = false;
	SetViewTargetWithBlend(InPawn, 0.0f);

	UE_LOG(Logcharacters, Log,
		TEXT("CameraSetup: Pawn='%s' SpringArm=%s Camera=%s TargetDistance=%.2f"),
		*GetNameSafe(InPawn),
		SpringArm ? TEXT("found") : TEXT("MISSING"),
		FollowCam ? TEXT("found") : TEXT("MISSING"),
		DesiredCameraDistance);
}

void AcharactersPlayerController::ApplyMouseWheelZoom(APawn* ControlledPawn, float DeltaTime)
{
	if (!ControlledPawn)
	{
		return;
	}

	USpringArmComponent* SpringArm = ControlledPawn->FindComponentByClass<USpringArmComponent>();
	if (!SpringArm)
	{
		return;
	}

	if (DesiredCameraDistance < 0.0f)
	{
		DesiredCameraDistance = FMath::Clamp(SpringArm->TargetArmLength, MinCameraDistance, MaxCameraDistance);
	}

	bool bConsumedDiscreteWheelInput = false;
	if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		DesiredCameraDistance -= MouseWheelZoomStep;
		bConsumedDiscreteWheelInput = true;
	}
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		DesiredCameraDistance += MouseWheelZoomStep;
		bConsumedDiscreteWheelInput = true;
	}

	// Standalone/shipping builds often report wheel movement through axis input
	// instead of the discrete MouseScrollUp/MouseScrollDown key events.
	if (!bConsumedDiscreteWheelInput)
	{
		const float WheelAxis = GetInputAnalogKeyState(EKeys::MouseWheelAxis);
		if (!FMath::IsNearlyZero(WheelAxis, KINDA_SMALL_NUMBER))
		{
			DesiredCameraDistance -= WheelAxis * MouseWheelZoomStep;
		}
	}

	DesiredCameraDistance = FMath::Clamp(DesiredCameraDistance, MinCameraDistance, MaxCameraDistance);
	SpringArm->TargetArmLength = FMath::FInterpTo(
		SpringArm->TargetArmLength,
		DesiredCameraDistance,
		DeltaTime,
		CameraZoomInterpSpeed);
}

void AcharactersPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bCinematicCameraModeEnabled)
	{
		UpdateCinematicCamera(DeltaTime);
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn && !bCinematicCameraModeEnabled)
	{
		return;
	}

	if (ControlledPawn && bEnableKeyboardFallbackMovement && !bCinematicCameraModeEnabled)
	{
		const float ForwardRaw =
			(IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f) -
			(IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);

		const float RightRaw =
			(IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) -
			(IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f);

		if (!FMath::IsNearlyZero(ForwardRaw) || !FMath::IsNearlyZero(RightRaw))
		{
			const FRotator CurrentControlRotation = GetControlRotation();
			const FRotator YawRotation(0.0f, CurrentControlRotation.Yaw, 0.0f);
			const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

			ControlledPawn->AddMovementInput(ForwardDirection, ForwardRaw);
			ControlledPawn->AddMovementInput(RightDirection, RightRaw);
		}
	}

	if (ControlledPawn && bEnableMouseFallbackLook && !bCinematicCameraModeEnabled)
	{
		float MouseDeltaX = 0.0f;
		float MouseDeltaY = 0.0f;
		GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

		if (!FMath::IsNearlyZero(MouseDeltaX) || !FMath::IsNearlyZero(MouseDeltaY))
		{
			AddYawInput(MouseDeltaX * MouseFallbackSensitivity);
			AddPitchInput(-MouseDeltaY * MouseFallbackSensitivity);
		}
	}

	if (ControlledPawn && !bLoggedMovementAnimDiagnostics)
	{
		const float Speed2D = ControlledPawn->GetVelocity().Size2D();
		if (Speed2D > 10.0f)
		{
			if (ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
			{
				if (UCharacterMovementComponent* MovementComp = ControlledCharacter->GetCharacterMovement())
				{
					UE_LOG(Logcharacters, Log,
						TEXT("MovementDiag: Pawn='%s' MaxWalkSpeed=%.2f MaxAcceleration=%.2f InputPending=%.3f InputLast=%.3f MovementMode=%d"),
						*GetNameSafe(ControlledPawn),
						MovementComp->MaxWalkSpeed,
						MovementComp->MaxAcceleration,
						ControlledCharacter->GetPendingMovementInputVector().Size(),
						ControlledCharacter->GetLastMovementInputVector().Size(),
						static_cast<int32>(MovementComp->MovementMode));

					const UCapsuleComponent* CapsuleComp = ControlledCharacter->GetCapsuleComponent();
					if (CapsuleComp)
					{
						const float CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
						const float CapsuleBottomZ = CapsuleComp->GetComponentLocation().Z - CapsuleHalfHeight;

						USkeletalMeshComponent* BodyMesh = nullptr;
						TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
						ControlledCharacter->GetComponents(SkeletalMeshes);
						for (USkeletalMeshComponent* MeshComp : SkeletalMeshes)
						{
							if (!MeshComp)
							{
								continue;
							}

							if (MeshComp->GetFName() == TEXT("Body"))
							{
								BodyMesh = MeshComp;
								break;
							}

							if (!BodyMesh && MeshComp->GetAnimInstance())
							{
								BodyMesh = MeshComp;
							}
						}

						if (BodyMesh)
						{
							const FBoxSphereBounds MeshBounds = BodyMesh->Bounds;
							const float MeshFeetWorldZ = MeshBounds.Origin.Z - MeshBounds.BoxExtent.Z;
							const float MeshFeetVsCapsuleBottom = MeshFeetWorldZ - CapsuleBottomZ;

							UE_LOG(Logcharacters, Warning,
								TEXT("GroundDiag: Pawn='%s' CapsuleHalfHeight=%.2f CapsuleBottomZ=%.2f BodyRelZ=%.2f BodyWorldZ=%.2f MeshFeetWorldZ=%.2f MeshFeetVsCapsuleBottom=%.2f FloorDist=%.2f LineDist=%.2f bWalkable=%s"),
								*GetNameSafe(ControlledCharacter),
								CapsuleHalfHeight,
								CapsuleBottomZ,
								BodyMesh->GetRelativeLocation().Z,
								BodyMesh->GetComponentLocation().Z,
								MeshFeetWorldZ,
								MeshFeetVsCapsuleBottom,
								MovementComp->CurrentFloor.FloorDist,
								MovementComp->CurrentFloor.LineDist,
								MovementComp->CurrentFloor.IsWalkableFloor() ? TEXT("true") : TEXT("false"));
						}
					}
				}
			}

			UE_LOG(Logcharacters, Log,
				TEXT("AnimDiag: Pawn='%s' Speed2D=%.2f"),
				*GetNameSafe(ControlledPawn),
				Speed2D);

			TInlineComponentArray<USkeletalMeshComponent*> SkeletalMeshes;
			ControlledPawn->GetComponents(SkeletalMeshes);

			for (int32 MeshIndex = 0; MeshIndex < SkeletalMeshes.Num(); ++MeshIndex)
			{
				USkeletalMeshComponent* MeshComp = SkeletalMeshes[MeshIndex];
				if (!MeshComp)
				{
					continue;
				}

				const TCHAR* AnimationModeLabel = TEXT("Unknown");
				switch (MeshComp->GetAnimationMode())
				{
				case EAnimationMode::AnimationBlueprint:
					AnimationModeLabel = TEXT("AnimationBlueprint");
					break;
				case EAnimationMode::AnimationSingleNode:
					AnimationModeLabel = TEXT("AnimationSingleNode");
					break;
				case EAnimationMode::AnimationCustomMode:
					AnimationModeLabel = TEXT("AnimationCustomMode");
					break;
				default:
					break;
				}

				UE_LOG(Logcharacters, Log,
					TEXT("AnimDiagMesh[%d]: Comp='%s' Visible=%s Mesh='%s' Skeleton='%s' AnimMode=%s AnimClass='%s' AnimInstance='%s'"),
					MeshIndex,
					*GetNameSafe(MeshComp),
					MeshComp->IsVisible() ? TEXT("true") : TEXT("false"),
					*GetNameSafe(MeshComp->GetSkeletalMeshAsset()),
					(MeshComp->GetSkeletalMeshAsset() && MeshComp->GetSkeletalMeshAsset()->GetSkeleton()) ? *GetNameSafe(MeshComp->GetSkeletalMeshAsset()->GetSkeleton()) : TEXT("none"),
					AnimationModeLabel,
					*GetNameSafe(MeshComp->GetAnimClass()),
					MeshComp->GetAnimInstance() ? *MeshComp->GetAnimInstance()->GetClass()->GetName() : TEXT("none"));
			}

			bLoggedMovementAnimDiagnostics = true;
		}
	}

	if (ACharacter* ControlledCharacter = Cast<ACharacter>(ControlledPawn))
	{
		if (UCharacterMovementComponent* MovementComp = ControlledCharacter->GetCharacterMovement())
		{
			const float PendingInput = ControlledCharacter->GetPendingMovementInputVector().Size();
			const float LastInput = ControlledCharacter->GetLastMovementInputVector().Size();
			const float InputMagnitude = FMath::Max(PendingInput, LastInput);

			if (!bMovementProbeActive && InputMagnitude > 0.9f)
			{
				bMovementProbeActive = true;
				MovementProbeElapsed = 0.0f;
				MovementProbePeakSpeed2D = 0.0f;
				MovementProbePeakAcceleration2D = 0.0f;
				MovementProbeSampleCount = 0;
			}

			if (bMovementProbeActive)
			{
				const float Speed2D = ControlledPawn->GetVelocity().Size2D();
				const float Accel2D = MovementComp->GetCurrentAcceleration().Size2D();
				MovementProbePeakSpeed2D = FMath::Max(MovementProbePeakSpeed2D, Speed2D);
				MovementProbePeakAcceleration2D = FMath::Max(MovementProbePeakAcceleration2D, Accel2D);
				MovementProbeElapsed += DeltaTime;
				++MovementProbeSampleCount;

				const bool bStopWindow = (MovementProbeElapsed >= 2.0f) || (InputMagnitude < 0.1f);
				if (bStopWindow)
				{
					UE_LOG(Logcharacters, Warning,
						TEXT("MovementProbe: Pawn='%s' Duration=%.2fs Samples=%d Input=%.3f SpeedNow=%.2f SpeedPeak=%.2f MaxWalkSpeed=%.2f MaxSpeed=%.2f AccelPeak=%.2f MaxAcceleration=%.2f BrakingDecelWalk=%.2f GroundFriction=%.2f BrakingFriction=%.2f BrakingFrictionFactor=%.2f MovementMode=%d"),
						*GetNameSafe(ControlledPawn),
						MovementProbeElapsed,
						MovementProbeSampleCount,
						InputMagnitude,
						Speed2D,
						MovementProbePeakSpeed2D,
						MovementComp->MaxWalkSpeed,
						MovementComp->GetMaxSpeed(),
						MovementProbePeakAcceleration2D,
						MovementComp->GetMaxAcceleration(),
						MovementComp->BrakingDecelerationWalking,
						MovementComp->GroundFriction,
						MovementComp->BrakingFriction,
						MovementComp->BrakingFrictionFactor,
						static_cast<int32>(MovementComp->MovementMode));

					bMovementProbeActive = false;
				}
			}
		}
	}

	if (ControlledPawn && !bCinematicCameraModeEnabled)
	{
		ApplyMouseWheelZoom(ControlledPawn, DeltaTime);
	}

	if (bRecordedTakeRenderInProgress && GEngine)
	{
		if (UMoviePipelineQueueEngineSubsystem* RenderSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>())
		{
			if (UMoviePipelineExecutorBase* ActiveExecutor = RenderSubsystem->GetActiveExecutor())
			{
				const float RenderProgress = FMath::Clamp(ActiveExecutor->GetStatusProgress(), 0.0f, 1.0f);
				RecordedTakeRenderStatusText = FString::Printf(TEXT("Rendering: %.0f%%"), RenderProgress * 100.0f);
				RecordedTakeRenderStatusColor = FColor::Yellow;
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
		if (!bUtilityKeysBound)
		{
			InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AcharactersPlayerController::HandleEscapePressed);
			InputComponent->BindKey(EKeys::Y, IE_Pressed, this, &AcharactersPlayerController::HandleYPressed);
			InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AcharactersPlayerController::HandleToggleAutopilotPressed);
			InputComponent->BindKey(EKeys::U, IE_Pressed, this, &AcharactersPlayerController::HandleRenderRecordedTakePressed);
			InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AcharactersPlayerController::HandleToggleCinematicCameraPressed);
			bUtilityKeysBound = true;
		}
	}

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		bAddedAnyMappingContext = false;

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
					bAddedAnyMappingContext = true;
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
						bAddedAnyMappingContext = true;
					}
				}
			}

			if (!bAddedAnyMappingContext)
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

void AcharactersPlayerController::HandleToggleAutopilotPressed()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTimeSeconds = World->GetTimeSeconds();
	if ((CurrentTimeSeconds - LastAutopilotToggleTimeSeconds) < AutopilotToggleDebounceSeconds)
	{
		return;
	}

	LastAutopilotToggleTimeSeconds = CurrentTimeSeconds;

	if (bAutopilotEnabled)
	{
		StopAutopilotTakeRecording();
		DisableAutopilotAndRepossess();
	}
	else
	{
		EnableAutopilotForCurrentPawn();
		StartAutopilotTakeRecording();
	}
}

void AcharactersPlayerController::HandleRenderRecordedTakePressed()
{
	SubmitLastRecordedTakeToRenderQueue();
}

void AcharactersPlayerController::HandleToggleCinematicCameraPressed()
{
	ToggleCinematicCameraMode();
}

void AcharactersPlayerController::ToggleCinematicCameraMode()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bCinematicCameraModeEnabled)
	{
		DisableCinematicCameraMode();
	}
	else
	{
		EnableCinematicCameraMode();
	}
}

AActor* AcharactersPlayerController::ResolveCinematicTargetActor() const
{
	if (APawn* ControlledPawn = GetPawn())
	{
		return ControlledPawn;
	}

	if (AutopilotPawn.IsValid())
	{
		return AutopilotPawn.Get();
	}

	if (AActor* ViewTarget = GetViewTarget())
	{
		return ViewTarget;
	}

	return nullptr;
}

FVector AcharactersPlayerController::ResolveCinematicFocusLocation(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return FVector::ZeroVector;
	}

	const FVector FallbackLocation = TargetActor->GetActorLocation() + FVector(0.0f, 0.0f, CinematicLookAtZOffset);

	const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return FallbackLocation;
	}

	const USkeletalMeshComponent* Mesh = TargetCharacter->GetMesh();
	if (!Mesh)
	{
		return FallbackLocation;
	}

	FVector HeadLocation = FVector::ZeroVector;
	FVector ChestLocation = FVector::ZeroVector;

	const bool bHasHead = TryGetFirstValidSocketLocation(
		Mesh,
		{TEXT("head"), TEXT("Head"), TEXT("headSocket"), TEXT("neck_01")},
		HeadLocation);

	const bool bHasChest = TryGetFirstValidSocketLocation(
		Mesh,
		{TEXT("spine_03"), TEXT("spine_02"), TEXT("Spine_03"), TEXT("chest")},
		ChestLocation);

	if (bHasHead && bHasChest)
	{
		return FMath::Lerp(ChestLocation, HeadLocation, FMath::Clamp(CinematicHeadFocusAlpha, 0.0f, 1.0f));
	}

	if (bHasHead)
	{
		return HeadLocation;
	}

	if (bHasChest)
	{
		return ChestLocation;
	}

	return FallbackLocation;
}

void AcharactersPlayerController::EnableCinematicCameraMode()
{
	if (bCinematicCameraModeEnabled)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !PlayerCameraManager)
	{
		return;
	}

	AActor* TargetActor = ResolveCinematicTargetActor();
	if (!TargetActor)
	{
		UE_LOG(Logcharacters, Warning, TEXT("CinematicCamera: no valid target actor found."));
		return;
	}

	if (!RuntimeCinematicCameraActor || !IsValid(RuntimeCinematicCameraActor))
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.ObjectFlags |= RF_Transient;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		RuntimeCinematicCameraActor = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(),
			PlayerCameraManager->GetCameraLocation(),
			PlayerCameraManager->GetCameraRotation(),
			SpawnParams);
	}

	if (!RuntimeCinematicCameraActor)
	{
		UE_LOG(Logcharacters, Error, TEXT("CinematicCamera: failed to spawn runtime camera actor."));
		return;
	}

	PreCinematicViewTarget = GetViewTarget();
	CinematicTargetActor = TargetActor;
	CinematicPanElapsedSeconds = 0.0f;
	CinematicSwayPhaseOffset = FMath::FRandRange(0.0f, 2.0f * PI);
	CinematicOrbitAccumulatedYawDegrees = 0.0f;
	CinematicOrbitDirectionSign = 1;
	CinematicDirectionTravelDegrees = 0.0f;
	CinematicCompletedTurnsThisDirection = 0;

	const FVector CameraLocation = PlayerCameraManager->GetCameraLocation();
	const FVector TargetLocation = ResolveCinematicFocusLocation(TargetActor);
	const FVector ToCamera = CameraLocation - TargetLocation;
	const FVector ToCameraXY(ToCamera.X, ToCamera.Y, 0.0f);

	CinematicOrbitRadius = FMath::Clamp(
		ToCameraXY.Size(),
		CinematicCloseOrbitRadius * 0.65f,
		CinematicCloseOrbitRadius * 1.35f);
	CinematicOrbitRadius = FMath::Max(100.0f, CinematicOrbitRadius);
	if (ToCameraXY.IsNearlyZero())
	{
		CinematicOrbitRadius = CinematicCloseOrbitRadius;
	}
	CinematicStartOrbitYawDegrees = ToCameraXY.IsNearlyZero()
		? TargetActor->GetActorRotation().Yaw
		: ToCameraXY.Rotation().Yaw;
	CinematicCameraHeightOffset = FMath::Clamp(ToCamera.Z, 30.0f, 160.0f);

	RuntimeCinematicCameraActor->SetActorLocationAndRotation(
		CameraLocation,
		PlayerCameraManager->GetCameraRotation());

	SetViewTargetWithBlend(RuntimeCinematicCameraActor, CinematicBlendInSeconds);
	bCinematicCameraModeEnabled = true;

	if (bDisablePlayerInputInCinematic)
	{
		SetIgnoreMoveInput(true);
		SetIgnoreLookInput(true);
	}

	UE_LOG(Logcharacters, Log,
		TEXT("CinematicCamera: ENABLED around '%s' (degrees=%.1f duration=%.2fs continuous=%s closeRadius=%.1f speedScale=%.2f turnsPerDirection=%d). Press V to exit."),
		*GetNameSafe(TargetActor),
		CinematicPanDegrees,
		CinematicPanDurationSeconds,
		bCinematicContinuousOrbit ? TEXT("true") : TEXT("false"),
		CinematicOrbitRadius,
		CinematicOrbitSpeedScale,
		CinematicTurnsPerDirection);

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("Cinematic Camera: ON (V to exit)"), FColor::Cyan, 2.0f);
	}
}

void AcharactersPlayerController::DisableCinematicCameraMode()
{
	if (!bCinematicCameraModeEnabled)
	{
		return;
	}

	bCinematicCameraModeEnabled = false;
	CinematicPanElapsedSeconds = 0.0f;
	CinematicOrbitAccumulatedYawDegrees = 0.0f;
	CinematicDirectionTravelDegrees = 0.0f;
	CinematicCompletedTurnsThisDirection = 0;
	CinematicTargetActor.Reset();

	if (bDisablePlayerInputInCinematic)
	{
		SetIgnoreMoveInput(false);
		SetIgnoreLookInput(false);
	}

	AActor* RestoreViewTarget = nullptr;
	if (PreCinematicViewTarget.IsValid())
	{
		RestoreViewTarget = PreCinematicViewTarget.Get();
	}
	else if (APawn* ControlledPawn = GetPawn())
	{
		RestoreViewTarget = ControlledPawn;
	}
	else if (AutopilotPawn.IsValid())
	{
		RestoreViewTarget = AutopilotPawn.Get();
	}

	if (RestoreViewTarget)
	{
		SetViewTargetWithBlend(RestoreViewTarget, CinematicBlendOutSeconds);
	}

	PreCinematicViewTarget.Reset();

	UE_LOG(Logcharacters, Log, TEXT("CinematicCamera: DISABLED. Restored normal camera."));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("Cinematic Camera: OFF"), FColor::Green, 2.0f);
	}
}

void AcharactersPlayerController::UpdateCinematicCamera(float DeltaTime)
{
	if (!bCinematicCameraModeEnabled || !RuntimeCinematicCameraActor)
	{
		return;
	}

	AActor* TargetActor = CinematicTargetActor.Get();
	if (!TargetActor)
	{
		DisableCinematicCameraMode();
		return;
	}

	CinematicPanElapsedSeconds += DeltaTime;
	const float Duration = FMath::Max(0.1f, CinematicPanDurationSeconds);
	const float BaseDegreesPerSecond = CinematicPanDegrees / Duration;
	const float SpeedScale = FMath::Max(0.05f, CinematicOrbitSpeedScale);
	const float MaxTravelDegreesForOneShot = FMath::Abs(CinematicPanDegrees);

	float DeltaYawDegrees = BaseDegreesPerSecond * DeltaTime * SpeedScale * static_cast<float>(CinematicOrbitDirectionSign);
	if (!bCinematicContinuousOrbit)
	{
		const float Remaining = MaxTravelDegreesForOneShot - FMath::Abs(CinematicOrbitAccumulatedYawDegrees);
		if (Remaining <= KINDA_SMALL_NUMBER)
		{
			DeltaYawDegrees = 0.0f;
		}
		else
		{
			DeltaYawDegrees = FMath::Clamp(DeltaYawDegrees, -Remaining, Remaining);
		}
	}

	CinematicOrbitAccumulatedYawDegrees += DeltaYawDegrees;

	if (bCinematicContinuousOrbit && !FMath::IsNearlyZero(DeltaYawDegrees))
	{
		CinematicDirectionTravelDegrees += FMath::Abs(DeltaYawDegrees);

		while (CinematicDirectionTravelDegrees >= 360.0f)
		{
			CinematicDirectionTravelDegrees -= 360.0f;
			++CinematicCompletedTurnsThisDirection;

			if (CinematicCompletedTurnsThisDirection >= FMath::Max(1, CinematicTurnsPerDirection))
			{
				CinematicCompletedTurnsThisDirection = 0;
				CinematicOrbitDirectionSign *= -1;
			}
		}
	}

	const float CurrentOrbitYaw = CinematicStartOrbitYawDegrees + CinematicOrbitAccumulatedYawDegrees;
	const float OrbitYawRadians = FMath::DegreesToRadians(CurrentOrbitYaw);
	const float BaseFrequencyRadians = FMath::Max(0.1f, CinematicSwayFrequency) * 2.0f * PI;
	const float TimeWithPhase = CinematicPanElapsedSeconds + CinematicSwayPhaseOffset;
	const float HorizontalSway = FMath::Sin(TimeWithPhase * BaseFrequencyRadians) * CinematicSwayHorizontalAmplitude;
	const float VerticalSway = FMath::Sin((TimeWithPhase * BaseFrequencyRadians * 0.57f) + 1.2f) * CinematicSwayVerticalAmplitude;

	FVector TargetLocation = ResolveCinematicFocusLocation(TargetActor);
	if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
	{
		TargetLocation += TargetCharacter->GetVelocity() * 0.05f;
	}

	const FVector OrbitDirection(FMath::Cos(OrbitYawRadians), FMath::Sin(OrbitYawRadians), 0.0f);
	const FVector OrbitRight(-OrbitDirection.Y, OrbitDirection.X, 0.0f);
	const FVector NewCameraLocation(
		TargetLocation.X + (OrbitDirection.X * CinematicOrbitRadius) + (OrbitRight.X * HorizontalSway),
		TargetLocation.Y + (OrbitDirection.Y * CinematicOrbitRadius) + (OrbitRight.Y * HorizontalSway),
		TargetLocation.Z + CinematicCameraHeightOffset + VerticalSway);

	const FRotator NewCameraRotation = (TargetLocation - NewCameraLocation).Rotation();
	RuntimeCinematicCameraActor->SetActorLocationAndRotation(NewCameraLocation, NewCameraRotation);
}

void AcharactersPlayerController::EnableAutopilotForCurrentPawn()
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		UE_LOG(Logcharacters, Warning, TEXT("Autopilot: no currently possessed pawn to hand to AI."));
		return;
	}

	AutopilotPawn = ControlledPawn;

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UClass* DesiredAIControllerClass = AutopilotAIControllerClass.Get();
	if (!DesiredAIControllerClass)
	{
		DesiredAIControllerClass = AcharactersWanderAIController::StaticClass();
	}

	AAIController* AIController = GetWorld()->SpawnActor<AAIController>(
		DesiredAIControllerClass,
		ControlledPawn->GetActorLocation(),
		ControlledPawn->GetActorRotation(),
		SpawnParams);

	if (!AIController)
	{
		UE_LOG(Logcharacters, Error, TEXT("Autopilot: failed to spawn AI controller '%s'."), *GetNameSafe(DesiredAIControllerClass));
		return;
	}

	UnPossess();
	AIController->Possess(ControlledPawn);
	SetViewTargetWithBlend(ControlledPawn, 0.0f);

	bAutopilotEnabled = true;

	UE_LOG(Logcharacters, Log, TEXT("Autopilot: ENABLED for pawn '%s'. Press J again to return to player control."), *GetNameSafe(ControlledPawn));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("AI Control: ON (press J to return)"), FColor::Cyan, 2.5f);
	}

	EnableCinematicCameraMode();
}

void AcharactersPlayerController::StartAutopilotTakeRecording()
{
	if (bAutopilotTakeRecordingActive || !IsLocalPlayerController())
	{
		return;
	}

#if WITH_EDITOR
	if (!GEngine)
	{
		return;
	}

	UTakeRecorderSubsystem* TakeRecorderSubsystem = GEngine->GetEngineSubsystem<UTakeRecorderSubsystem>();
	if (!TakeRecorderSubsystem)
	{
		UE_LOG(Logcharacters, Warning, TEXT("TakeRecorder: subsystem unavailable; recording was not started."));
		return;
	}

	if (UTakeRecorderProjectSettings* ProjectSettings = GetMutableDefault<UTakeRecorderProjectSettings>())
	{
		bPreviousTakeRecordToPossessable = ProjectSettings->Settings.bRecordToPossessable;
		bHasForcedTakeRecordToSpawnable = true;
		ProjectSettings->Settings.bRecordToPossessable = false;
	}

	TakeRecorderSubsystem->TakeRecorderFinished.RemoveDynamic(this, &AcharactersPlayerController::HandleTakeRecorderFinished);
	TakeRecorderSubsystem->TakeRecorderFinished.AddDynamic(this, &AcharactersPlayerController::HandleTakeRecorderFinished);

	TakeRecorderSubsystem->SetTargetSequence();
	TakeRecorderSubsystem->ClearSources();

	AActor* ActorToRecord = nullptr;
	if (APawn* ControlledPawn = GetPawn())
	{
		ActorToRecord = ControlledPawn;
	}
	else if (AutopilotPawn.IsValid())
	{
		ActorToRecord = AutopilotPawn.Get();
	}

	if (ActorToRecord)
	{
		TakeRecorderSubsystem->AddSourceForActor(ActorToRecord, true, false);
	}
	else
	{
		UE_LOG(Logcharacters, Warning, TEXT("TakeRecorder: no pawn actor found to record (player pawn is null and autopilot pawn invalid)."));
	}

	if (RuntimeCinematicCameraActor && IsValid(RuntimeCinematicCameraActor))
	{
		TakeRecorderSubsystem->AddSourceForActor(RuntimeCinematicCameraActor, true, false);
	}

	if (!TakeRecorderSubsystem->StartRecording(false, true))
	{
		UE_LOG(Logcharacters, Warning, TEXT("TakeRecorder: failed to start recording for autopilot take."));
		RestoreTakeRecorderRecordToPossessableSetting();
		return;
	}

	bAutopilotTakeRecordingActive = true;
	UE_LOG(Logcharacters, Log, TEXT("TakeRecorder: recording started for autopilot take."));
	UpdateRecordingStatusHud();

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("Take Recorder: ON (J to stop)"), FColor::Yellow, 2.5f);
	}
#else
	UE_LOG(Logcharacters, Warning, TEXT("TakeRecorder: unavailable in non-editor builds."));
#endif
}

void AcharactersPlayerController::StopAutopilotTakeRecording()
{
	if (!bAutopilotTakeRecordingActive || !IsLocalPlayerController())
	{
		return;
	}

#if WITH_EDITOR
	if (!GEngine)
	{
		bAutopilotTakeRecordingActive = false;
		return;
	}

	if (UTakeRecorderSubsystem* TakeRecorderSubsystem = GEngine->GetEngineSubsystem<UTakeRecorderSubsystem>())
	{
		if (TakeRecorderSubsystem->IsRecording())
		{
			TakeRecorderSubsystem->StopRecording();
		}
		else
		{
			RestoreTakeRecorderRecordToPossessableSetting();
		}
	}
	else
	{
		RestoreTakeRecorderRecordToPossessableSetting();
	}
#endif

	bAutopilotTakeRecordingActive = false;
	UE_LOG(Logcharacters, Log, TEXT("TakeRecorder: recording stop requested."));
	UpdateRecordingStatusHud();
}

void AcharactersPlayerController::HandleTakeRecorderFinished(ULevelSequence* SequenceAsset)
{
	#if WITH_EDITOR
	RestoreTakeRecorderRecordToPossessableSetting();
	#endif

	bAutopilotTakeRecordingActive = false;
	LastRecordedAutopilotTake = SequenceAsset;
	UpdateRecordingStatusHud();

	UE_LOG(Logcharacters, Log, TEXT("TakeRecorder: finished. Sequence='%s'"), *GetNameSafe(SequenceAsset));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("Take Recorder: saved (press U to render)"), FColor::Green, 3.0f);
	}
}

#if WITH_EDITOR
void AcharactersPlayerController::RestoreTakeRecorderRecordToPossessableSetting()
{
	if (!bHasForcedTakeRecordToSpawnable)
	{
		return;
	}

	if (UTakeRecorderProjectSettings* ProjectSettings = GetMutableDefault<UTakeRecorderProjectSettings>())
	{
		ProjectSettings->Settings.bRecordToPossessable = bPreviousTakeRecordToPossessable;
	}

	bHasForcedTakeRecordToSpawnable = false;
}
#endif

void AcharactersPlayerController::SubmitLastRecordedTakeToRenderQueue()
{
	if (!IsLocalPlayerController())
	{
		return;
	}

	if (bAutopilotTakeRecordingActive)
	{
		StopAutopilotTakeRecording();
	}

	ULevelSequence* SequenceToRender = LastRecordedAutopilotTake.Get();
	if (!SequenceToRender)
	{
		UE_LOG(Logcharacters, Warning, TEXT("MRQ: no recorded take available. Press J to record first."));
		if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
		{
			charactersHUD->AddTransientMessage(TEXT("No recorded take yet. Use J first."), FColor::Red, 2.5f);
		}
		return;
	}

	if (!GEngine)
	{
		return;
	}

	UMoviePipelineQueueEngineSubsystem* RenderSubsystem = GEngine->GetEngineSubsystem<UMoviePipelineQueueEngineSubsystem>();
	if (!RenderSubsystem)
	{
		UE_LOG(Logcharacters, Error, TEXT("MRQ: runtime subsystem unavailable."));
		return;
	}

	if (RenderSubsystem->IsRendering())
	{
		UE_LOG(Logcharacters, Warning, TEXT("MRQ: render already in progress."));
		bRecordedTakeRenderInProgress = true;
		RecordedTakeRenderStatusText = TEXT("Rendering: already in progress");
		RecordedTakeRenderStatusColor = FColor::Yellow;
		UpdateRecordingStatusHud();
		return;
	}

	RenderSubsystem->OnRenderFinished.RemoveDynamic(this, &AcharactersPlayerController::HandleRuntimeRenderFinished);
	RenderSubsystem->OnRenderFinished.AddDynamic(this, &AcharactersPlayerController::HandleRuntimeRenderFinished);

	UMoviePipelineExecutorJob* RenderJob = RenderSubsystem->AllocateJob(SequenceToRender);
	if (!RenderJob)
	{
		UE_LOG(Logcharacters, Error, TEXT("MRQ: failed to allocate render job."));
		return;
	}

	if (UWorld* CurrentWorld = GetWorld())
	{
		RenderJob->Map = FSoftObjectPath(CurrentWorld);
	}
	RenderJob->SetSequence(FSoftObjectPath(SequenceToRender));

	UMoviePipelinePrimaryConfig* RenderConfig = RenderJob->GetConfiguration();
	if (!RenderConfig)
	{
		UE_LOG(Logcharacters, Error, TEXT("MRQ: render job has no configuration."));
		return;
	}

	UMoviePipelineOutputSetting* OutputSetting = Cast<UMoviePipelineOutputSetting>(
		RenderConfig->FindOrAddSettingByClass(UMoviePipelineOutputSetting::StaticClass()));
	if (!OutputSetting)
	{
		UE_LOG(Logcharacters, Error, TEXT("MRQ: failed to add output setting."));
		return;
	}

	OutputSetting->OutputDirectory.Path = FPaths::ProjectSavedDir() / TEXT("Renders");
	OutputSetting->FileNameFormat = TEXT("{sequence_name}_{date}_{time}_{frame_number}");
	OutputSetting->OutputResolution = bRenderRecordedTakeAt4K ? FIntPoint(3840, 2160) : FIntPoint(2560, 1440);
	OutputSetting->bUseCustomFrameRate = true;
	OutputSetting->OutputFrameRate = FFrameRate(60, 1);

	if (UMovieScene* MovieScene = SequenceToRender->GetMovieScene())
	{
		const FFrameRate DisplayRate = MovieScene->GetDisplayRate();
		if (DisplayRate.IsValid() && DisplayRate.Numerator > 0)
		{
			OutputSetting->OutputFrameRate = DisplayRate;
		}
	}

	if (UMoviePipelineGameOverrideSetting* GameOverrideSetting = Cast<UMoviePipelineGameOverrideSetting>(
		RenderConfig->FindOrAddSettingByClass(UMoviePipelineGameOverrideSetting::StaticClass())))
	{
		UClass* DesiredGameModeClass = nullptr;
		if (UWorld* CurrentWorld = GetWorld())
		{
			if (AGameModeBase* ActiveGameMode = CurrentWorld->GetAuthGameMode())
			{
				DesiredGameModeClass = ActiveGameMode->GetClass();
			}
			else if (AWorldSettings* WorldSettings = CurrentWorld->GetWorldSettings())
			{
				DesiredGameModeClass = WorldSettings->DefaultGameMode;
			}
		}

		if (DesiredGameModeClass)
		{
			GameOverrideSetting->SoftGameModeOverride = DesiredGameModeClass;
		}
		else
		{
			GameOverrideSetting->SoftGameModeOverride = nullptr;
		}
	}

	RenderConfig->FindOrAddSettingByClass(UMoviePipelineDeferredPassBase::StaticClass());

	UClass* Mp4OutputClass = FindObject<UClass>(nullptr, TEXT("/Script/MovieRenderPipelineMP4Encoder.MoviePipelineMP4EncoderOutput"));
	if (!Mp4OutputClass)
	{
		Mp4OutputClass = StaticLoadClass(UMoviePipelineSetting::StaticClass(), nullptr, TEXT("/Script/MovieRenderPipelineMP4Encoder.MoviePipelineMP4EncoderOutput"));
	}

	if (Mp4OutputClass)
	{
		if (UMoviePipelineSetting* Mp4Setting = RenderConfig->FindOrAddSettingByClass(Mp4OutputClass))
		{
			if (FByteProperty* RateControlProp = FindFProperty<FByteProperty>(Mp4Setting->GetClass(), TEXT("EncodingRateControl")))
			{
				RateControlProp->SetPropertyValue_InContainer(Mp4Setting, 1);
			}

			if (FIntProperty* ConstantRateFactorProp = FindFProperty<FIntProperty>(Mp4Setting->GetClass(), TEXT("ConstantRateFactor")))
			{
				ConstantRateFactorProp->SetPropertyValue_InContainer(Mp4Setting, 18);
			}

			if (FBoolProperty* IncludeAudioProp = FindFProperty<FBoolProperty>(Mp4Setting->GetClass(), TEXT("bIncludeAudio")))
			{
				IncludeAudioProp->SetPropertyValue_InContainer(Mp4Setting, true);
			}
		}
	}
	else
	{
		UE_LOG(Logcharacters, Warning, TEXT("MRQ: MP4 output class unavailable. Ensure MovieRenderPipeline plugin is enabled."));
	}

	if (bRenderPngSequenceAlongsideMp4)
	{
		if (UMoviePipelineImageSequenceOutput_PNG* PngSetting = Cast<UMoviePipelineImageSequenceOutput_PNG>(
			RenderConfig->FindOrAddSettingByClass(UMoviePipelineImageSequenceOutput_PNG::StaticClass())))
		{
			PngSetting->bWriteAlpha = false;
		}
	}

	RenderSubsystem->SetConfiguration(nullptr, false);
	RenderSubsystem->RenderJob(RenderJob);
	bRecordedTakeRenderInProgress = true;
	RecordedTakeRenderStatusText = TEXT("Rendering: starting...");
	RecordedTakeRenderStatusColor = FColor::Yellow;
	UpdateRecordingStatusHud();

	UE_LOG(Logcharacters, Log,
		TEXT("MRQ: started render for '%s' at %s (%s + %s)."),
		*GetNameSafe(SequenceToRender),
		*OutputSetting->OutputResolution.ToString(),
		TEXT("H.264 MP4"),
		bRenderPngSequenceAlongsideMp4 ? TEXT("PNG") : TEXT("no PNG"));
	UE_LOG(Logcharacters, Log, TEXT("MRQ: output directory is '%s'"), *OutputSetting->OutputDirectory.Path);

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("Render started: MP4"), FColor::Cyan, 2.0f);
	}
}

void AcharactersPlayerController::HandleRuntimeRenderFinished(FMoviePipelineOutputData Results)
{
	bRecordedTakeRenderInProgress = false;
	RecordedTakeRenderStatusText = Results.bSuccess ? TEXT("Rendering: complete") : TEXT("Rendering: failed");
	RecordedTakeRenderStatusColor = Results.bSuccess ? FColor::Green : FColor::Red;
	UpdateRecordingStatusHud();

	UE_LOG(Logcharacters, Log,
		TEXT("MRQ: render finished success=%s shots=%d"),
		Results.bSuccess ? TEXT("true") : TEXT("false"),
		Results.ShotData.Num());

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(
			Results.bSuccess ? TEXT("Render complete") : TEXT("Render failed"),
			Results.bSuccess ? FColor::Green : FColor::Red,
			3.0f);
	}
}

void AcharactersPlayerController::UpdateRecordingStatusHud()
{
	AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>();
	if (!charactersHUD)
	{
		return;
	}

	charactersHUD->SetStatusLine(
		TEXT("RecordingStatus"),
		FString::Printf(TEXT("Recording: %s"), bAutopilotTakeRecordingActive ? TEXT("ON") : TEXT("OFF")),
		bAutopilotTakeRecordingActive ? FColor::Yellow : FColor::Silver);

	FString LastTakeStatus = TEXT("Last Take: none");
	FColor LastTakeColor = FColor::Silver;
	if (ULevelSequence* LastTake = LastRecordedAutopilotTake.Get())
	{
		FString TakeName = LastTake->GetName();
		const int32 MaxTakeNameChars = 32;
		if (TakeName.Len() > MaxTakeNameChars)
		{
			TakeName = TakeName.Left(MaxTakeNameChars - 3) + TEXT("...");
		}

		LastTakeStatus = FString::Printf(TEXT("Last Take: ready (%s)"), *TakeName);
		LastTakeColor = FColor::Green;
	}

	charactersHUD->SetStatusLine(TEXT("LastTakeStatus"), LastTakeStatus, LastTakeColor);
	charactersHUD->SetStatusLine(TEXT("RenderStatus"), RecordedTakeRenderStatusText, RecordedTakeRenderStatusColor);
}

void AcharactersPlayerController::DisableAutopilotAndRepossess()
{
	APawn* PawnToRepossess = AutopilotPawn.Get();
	if (!PawnToRepossess)
	{
		UE_LOG(Logcharacters, Warning, TEXT("Autopilot: no cached pawn to repossess."));
		bAutopilotEnabled = false;
		return;
	}

	if (AController* CurrentController = PawnToRepossess->GetController())
	{
		if (CurrentController != this)
		{
			CurrentController->UnPossess();

			if (CurrentController->IsA<AAIController>())
			{
				CurrentController->Destroy();
			}
		}
	}

	Possess(PawnToRepossess);
	SetViewTargetWithBlend(PawnToRepossess, 0.0f);

	bAutopilotEnabled = false;
	AutopilotPawn.Reset();

	UE_LOG(Logcharacters, Log, TEXT("Autopilot: DISABLED. Player control restored."));

	if (AcharactersHUD* charactersHUD = GetHUD<AcharactersHUD>())
	{
		charactersHUD->AddTransientMessage(TEXT("AI Control: OFF (player control restored)"), FColor::Green, 2.5f);
	}

	DisableCinematicCameraMode();
}

bool AcharactersPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

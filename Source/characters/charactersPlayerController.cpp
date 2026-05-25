// Copyright Epic Games, Inc. All Rights Reserved.


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
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Input/SVirtualJoystick.h"

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

	bLoggedMovementAnimDiagnostics = false;
	bMovementProbeActive = false;
	MovementProbeElapsed = 0.0f;
	MovementProbePeakSpeed2D = 0.0f;
	MovementProbePeakAcceleration2D = 0.0f;
	MovementProbeSampleCount = 0;

	// Find spring arm and camera on the newly possessed pawn.
	// Works for any character (Blueprint MetaHuman, AcharactersCharacter subclass, etc.)
	USpringArmComponent* SpringArm = InPawn->FindComponentByClass<USpringArmComponent>();
	UCameraComponent* FollowCam   = InPawn->FindComponentByClass<UCameraComponent>();

	if (SpringArm)
	{
		SpringArm->bUsePawnControlRotation = true;
		SpringArm->bDoCollisionTest        = false;
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
		TEXT("AcharactersPlayerController: Possessed '%s' (%s). SpringArm=%s Camera=%s."),
		*GetNameSafe(InPawn),
		*InPawn->GetClass()->GetName(),
		SpringArm ? TEXT("found") : TEXT("MISSING"),
		FollowCam  ? TEXT("found") : TEXT("MISSING"));
}

void AcharactersPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	if (!IsLocalPlayerController())
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	if (bEnableKeyboardFallbackMovement)
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

	if (bEnableMouseFallbackLook)
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

	if (!bLoggedMovementAnimDiagnostics)
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
		DisableAutopilotAndRepossess();
	}
	else
	{
		EnableAutopilotForCurrentPawn();
	}
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
}

bool AcharactersPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

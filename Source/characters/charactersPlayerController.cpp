// Copyright Epic Games, Inc. All Rights Reserved.


#include "charactersPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "Blueprint/UserWidget.h"
#include "characters.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
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

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
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
					}
				}
			}
		}
	}
}

bool AcharactersPlayerController::ShouldUseTouchControls() const
{
	// are we on a mobile platform? Should we force touch?
	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

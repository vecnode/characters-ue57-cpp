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
	UInputMappingContext* LoadMappingContextFallback(const TCHAR* AssetPath)
	{
		return Cast<UInputMappingContext>(StaticLoadObject(UInputMappingContext::StaticClass(), nullptr, AssetPath));
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

	if (WasInputKeyJustPressed(EKeys::MouseScrollUp))
	{
		DesiredCameraDistance -= MouseWheelZoomStep;
	}
	if (WasInputKeyJustPressed(EKeys::MouseScrollDown))
	{
		DesiredCameraDistance += MouseWheelZoomStep;
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
				if (UInputMappingContext* DefaultContext = LoadMappingContextFallback(TEXT("/Game/Input/IMC_Default.IMC_Default")))
				{
					DefaultMappingContexts.Add(DefaultContext);
				}
			}

			if (MobileExcludedMappingContexts.Num() == 0)
			{
				if (UInputMappingContext* MouseContext = LoadMappingContextFallback(TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook")))
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

// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.

#include "Gameplay/Controllers/MetaAgentPlayerController.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

void AMetaAgentPlayerController::ApplyFallbackMovementInput(APawn* ControlledPawn)
{
	if (!ControlledPawn || !InputFallback.bEnableKeyboardMovement || CinematicCamera.bModeEnabled)
	{
		return;
	}

	const float ForwardRaw =
		(IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up) ? 1.0f : 0.0f) -
		(IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down) ? 1.0f : 0.0f);

	const float RightRaw =
		(IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right) ? 1.0f : 0.0f) -
		(IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left) ? 1.0f : 0.0f);

	if (FMath::IsNearlyZero(ForwardRaw) && FMath::IsNearlyZero(RightRaw))
	{
		return;
	}

	const FRotator CurrentControlRotation = GetControlRotation();
	const FRotator YawRotation(0.0f, CurrentControlRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	ControlledPawn->AddMovementInput(ForwardDirection, ForwardRaw);
	ControlledPawn->AddMovementInput(RightDirection, RightRaw);
}

void AMetaAgentPlayerController::ApplyFallbackLookInput()
{
	if (!InputFallback.bEnableMouseLook || CinematicCamera.bModeEnabled)
	{
		return;
	}

	float MouseDeltaX = 0.0f;
	float MouseDeltaY = 0.0f;
	GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

	if (FMath::IsNearlyZero(MouseDeltaX) && FMath::IsNearlyZero(MouseDeltaY))
	{
		return;
	}

	AddYawInput(MouseDeltaX * InputFallback.MouseSensitivity);
	AddPitchInput(-MouseDeltaY * InputFallback.MouseSensitivity);
}


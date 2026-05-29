// Based on Unreal Engine template code.
// Project-specific implementation and modifications Copyright (c) vecnode, 2026.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "UObject/SoftObjectPtr.h"
#include "MetaAgentCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class AMetaAgentHUD;
struct FInputActionValue;

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMetaAgentCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	/** Optional soft reference fallback for jump input action asset. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Defaults")
	TSoftObjectPtr<UInputAction> JumpActionAsset;

	/** Optional soft reference fallback for move input action asset. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Defaults")
	TSoftObjectPtr<UInputAction> MoveActionAsset;

	/** Optional soft reference fallback for look input action asset. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Defaults")
	TSoftObjectPtr<UInputAction> LookActionAsset;

	/** Optional soft reference fallback for mouse look input action asset. */
	UPROPERTY(EditDefaultsOnly, Category="Input|Defaults")
	TSoftObjectPtr<UInputAction> MouseLookActionAsset;

public:

	/** Constructor */
	AMetaAgentCharacter();	

protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

	/** Prints hello world and the character world position when the H key is pressed. */
	void PrintHelloWorld();

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

public:

	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }
};



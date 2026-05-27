// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "charactersHUD.generated.h"

USTRUCT()
struct FcharactersHUDMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString Text;

	UPROPERTY()
	FColor Color = FColor::White;

	UPROPERTY()
	float TimeRemaining = 0.0f;
};

USTRUCT()
struct FcharactersHUDStatusLine
{
	GENERATED_BODY()

	UPROPERTY()
	FName Key = NAME_None;

	UPROPERTY()
	FString Text;

	UPROPERTY()
	FColor Color = FColor::White;
};

UCLASS()
class AcharactersHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void AddTransientMessage(const FString& Message, FColor Color = FColor::White, float DurationSeconds = 2.0f);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void SetStatusLine(FName Key, const FString& Message, FColor Color = FColor::White);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	void ClearStatusLine(FName Key);

private:
	UPROPERTY()
	TArray<FcharactersHUDMessage> MessageQueue;

	UPROPERTY()
	TArray<FcharactersHUDStatusLine> StatusLines;
};

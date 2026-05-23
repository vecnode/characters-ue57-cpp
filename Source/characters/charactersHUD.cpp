// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void AcharactersHUD::AddTransientMessage(const FString& Message, FColor Color, float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	FcharactersHUDMessage& Entry = MessageQueue.AddDefaulted_GetRef();
	Entry.Text = Message;
	Entry.Color = Color;
	Entry.TimeRemaining = FMath::Max(DurationSeconds, 0.1f);
}

void AcharactersHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas || MessageQueue.Num() == 0)
	{
		return;
	}

	const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.0f;

	for (int32 Index = MessageQueue.Num() - 1; Index >= 0; --Index)
	{
		MessageQueue[Index].TimeRemaining -= DeltaSeconds;
		if (MessageQueue[Index].TimeRemaining <= 0.0f)
		{
			MessageQueue.RemoveAt(Index);
		}
	}

	float DrawY = 60.0f;
	for (const FcharactersHUDMessage& Message : MessageQueue)
	{
		if (!Message.Text.IsEmpty())
		{
			DrawText(Message.Text, Message.Color, 40.0f, DrawY, GEngine ? GEngine->GetSmallFont() : nullptr, 1.2f, false);
			DrawY += 22.0f;
		}
	}
}

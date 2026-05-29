// Copyright (c) vecnode 2026. All Rights Reserved.

#include "UI/HUD/MetaAgentHUD.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void AMetaAgentHUD::SetStatusLine(FName Key, const FString& Message, FColor Color)
{
	if (Key.IsNone())
	{
		return;
	}

	if (Message.IsEmpty())
	{
		ClearStatusLine(Key);
		return;
	}

	for (FMetaAgentHUDStatusLine& Line : StatusLines)
	{
		if (Line.Key == Key)
		{
			Line.Text = Message;
			Line.Color = Color;
			return;
		}
	}

	FMetaAgentHUDStatusLine& NewLine = StatusLines.AddDefaulted_GetRef();
	NewLine.Key = Key;
	NewLine.Text = Message;
	NewLine.Color = Color;
}

void AMetaAgentHUD::ClearStatusLine(FName Key)
{
	if (Key.IsNone())
	{
		return;
	}

	for (int32 Index = StatusLines.Num() - 1; Index >= 0; --Index)
	{
		if (StatusLines[Index].Key == Key)
		{
			StatusLines.RemoveAt(Index);
		}
	}
}

void AMetaAgentHUD::AddTransientMessage(const FString& Message, FColor Color, float DurationSeconds)
{
	if (Message.IsEmpty())
	{
		return;
	}

	FMetaAgentHUDMessage& Entry = MessageQueue.AddDefaulted_GetRef();
	Entry.Text = Message;
	Entry.Color = Color;
	Entry.TimeRemaining = FMath::Max(DurationSeconds, 0.1f);
}

void AMetaAgentHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
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
	for (const FMetaAgentHUDMessage& Message : MessageQueue)
	{
		if (!Message.Text.IsEmpty())
		{
			DrawText(Message.Text, Message.Color, 40.0f, DrawY, GEngine ? GEngine->GetSmallFont() : nullptr, 1.2f, false);
			DrawY += 22.0f;
		}
	}

	if (StatusLines.Num() == 0)
	{
		return;
	}

	const UFont* StatusFont = GEngine ? GEngine->GetSmallFont() : nullptr;
	const float StatusScale = 1.0f;
	const float PanelPadding = 8.0f;
	const float PanelLineHeight = 20.0f;
	const float PanelMinWidth = 320.0f;

	float MaxTextWidth = 0.0f;
	for (const FMetaAgentHUDStatusLine& Line : StatusLines)
	{
		float SizeX = 0.0f;
		float SizeY = 0.0f;
		Canvas->StrLen(StatusFont, Line.Text, SizeX, SizeY);
		MaxTextWidth = FMath::Max(MaxTextWidth, SizeX * StatusScale);
	}

	const float PanelWidth = FMath::Max(PanelMinWidth, MaxTextWidth + (PanelPadding * 2.0f));
	const float PanelHeight = (StatusLines.Num() * PanelLineHeight) + (PanelPadding * 2.0f);
	const float PanelX = Canvas->ClipX - PanelWidth - 24.0f;
	const float PanelY = 24.0f;

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.35f), PanelX, PanelY, PanelWidth, PanelHeight);

	float StatusY = PanelY + PanelPadding;
	for (const FMetaAgentHUDStatusLine& Line : StatusLines)
	{
		DrawText(Line.Text, Line.Color, PanelX + PanelPadding, StatusY, const_cast<UFont*>(StatusFont), StatusScale, false);
		StatusY += PanelLineHeight;
	}
}


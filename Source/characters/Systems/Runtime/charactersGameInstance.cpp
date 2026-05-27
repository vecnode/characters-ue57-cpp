// Copyright (c) vecnode 2026. All Rights Reserved.

#include "Systems/Runtime/charactersGameInstance.h"
#include "Engine/World.h"

UcharactersGameInstance* UcharactersGameInstance::Get(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return nullptr;
	}

	UWorld* World = WorldContextObject->GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	return Cast<UcharactersGameInstance>(World->GetGameInstance());
}

#include "MetaAgentBlueprintLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MetaAgentRuntimeSubsystem.h"

bool UMetaAgentBlueprintLibrary::IsMetaAgentRuntimeActive(const UObject* WorldContextObject)
{
	if (!IsValid(WorldContextObject))
	{
		return false;
	}

	const UWorld* World = WorldContextObject->GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	UGameInstance* GameInstance = World->GetGameInstance();
	if (!IsValid(GameInstance))
	{
		return false;
	}

	const UMetaAgentRuntimeSubsystem* RuntimeSubsystem = GameInstance->GetSubsystem<UMetaAgentRuntimeSubsystem>();
	return IsValid(RuntimeSubsystem) && RuntimeSubsystem->IsActive();
}

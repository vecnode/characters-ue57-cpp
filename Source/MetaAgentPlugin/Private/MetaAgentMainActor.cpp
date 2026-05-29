#include "MetaAgentMainActor.h"

#include "Core/MetaAgent.h"
#include "MetaAgentPlugin.h"

AMetaAgentMainActor::AMetaAgentMainActor()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AMetaAgentMainActor::BeginPlay()
{
	Super::BeginPlay();
	GMetaAgentRuntimeActive = bActive;
}

void AMetaAgentMainActor::ActivateAgent()
{
	bActive = true;
	GMetaAgentRuntimeActive = true;
	UE_LOG(LogMetaAgentPlugin, Log, TEXT("MetaAgentMainActor set to ACTIVE."));
}

void AMetaAgentMainActor::DeactivateAgent()
{
	bActive = false;
	GMetaAgentRuntimeActive = false;
	UE_LOG(LogMetaAgentPlugin, Log, TEXT("MetaAgentMainActor set to INACTIVE."));
}

void AMetaAgentMainActor::ToggleAgentActive()
{
	bActive = !bActive;
	GMetaAgentRuntimeActive = bActive;
	UE_LOG(LogMetaAgentPlugin, Log, TEXT("MetaAgentMainActor toggled to %s."), bActive ? TEXT("ACTIVE") : TEXT("INACTIVE"));
}

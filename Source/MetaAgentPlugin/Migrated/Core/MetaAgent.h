// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogMetaAgent, Log, All);

/** Runtime switch controlled by plugin settings and MetaAgentMainActor Active toggle. */
extern bool GMetaAgentRuntimeActive;

FORCEINLINE bool IsMetaAgentRuntimeActive()
{
	return GMetaAgentRuntimeActive;
}

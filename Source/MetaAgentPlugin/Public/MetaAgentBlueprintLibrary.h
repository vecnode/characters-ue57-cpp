#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MetaAgentBlueprintLibrary.generated.h"

class UObject;

UCLASS()
class METAAGENTPLUGIN_API UMetaAgentBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="MetaAgent|Runtime", meta=(WorldContext="WorldContextObject"))
	static bool IsMetaAgentRuntimeActive(const UObject* WorldContextObject);
};

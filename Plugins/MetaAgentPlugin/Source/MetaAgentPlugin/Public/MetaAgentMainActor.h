#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MetaAgentMainActor.generated.h"

UCLASS(BlueprintType, Blueprintable, meta=(DisplayName="Meta Agent Main Actor"))
class METAAGENTPLUGIN_API AMetaAgentMainActor : public AActor
{
	GENERATED_BODY()

public:
	AMetaAgentMainActor();

	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MetaAgent|Control")
	bool bActive = true;

	UFUNCTION(CallInEditor, BlueprintCallable, Category="MetaAgent|Control")
	void ActivateAgent();

	UFUNCTION(CallInEditor, BlueprintCallable, Category="MetaAgent|Control")
	void DeactivateAgent();

	UFUNCTION(CallInEditor, BlueprintCallable, Category="MetaAgent|Control")
	void ToggleAgentActive();
};

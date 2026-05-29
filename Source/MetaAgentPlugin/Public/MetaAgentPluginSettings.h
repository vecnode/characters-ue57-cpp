#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MetaAgentPluginSettings.generated.h"

UCLASS(Config=Game, defaultconfig, meta=(DisplayName="Meta Agent Plugin"))
class METAAGENTPLUGIN_API UMetaAgentPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMetaAgentPluginSettings();

	virtual FName GetCategoryName() const override;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Startup")
	bool bActive = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableInputSystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableCameraSystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableAISystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableNetworkingSystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableRecordingSystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Feature Flags")
	bool bEnableUISystems = true;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Networking|HTTP Server", meta=(ClampMin="1024", ClampMax="65535"))
	int32 LocalHttpServerPort = 30080;

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Networking|Platform")
	FString PlatformBaseUrl = TEXT("http://127.0.0.1:8000");

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Networking|Platform")
	FString PlatformEventEndpoint = TEXT("/api/unreal/event");

	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, Category="MetaAgent|Networking|Platform")
	FString PlatformSessionId = TEXT("characters-local");
};

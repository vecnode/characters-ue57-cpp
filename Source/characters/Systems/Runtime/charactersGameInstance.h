// Copyright (c) vecnode 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "HttpRequestHandler.h"
#include "HttpRouteHandle.h"
#include "charactersGameInstance.generated.h"

class AcharactersCharacter;
class IHttpRouter;
struct FHttpServerRequest;

/**
 * Singleton Game Instance for the characters project.
 *
 * Persists across level transitions and provides a single, globally accessible
 * reference to the active player character.  Any C++ class that has a world
 * context object can reach it via UcharactersGameInstance::Get(this).
 *
 * --- Editor setup required ---
 * Project Settings → Maps & Modes → Game Instance Class = charactersGameInstance
 */
UCLASS(Config=Game)
class UcharactersGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:

	/**
	 * The Blueprint class used to spawn the player character.
	 * Assign BP_character1 here in the Editor (or via Project Settings default pawn).
	 * Exposed to Blueprints so level designers can swap characters without recompiling.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Character")
	TSubclassOf<AcharactersCharacter> PlayerCharacterClass;

	/**
	 * Live reference to the currently possessed player character.
	 * Populated automatically from AcharactersMHPlayer::BeginPlay().
	 * Read-only from Blueprints to prevent accidental replacement.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Character")
	TObjectPtr<AcharactersCharacter> ActivePlayerCharacter;

	/**
	 * Global accessor.  Returns the game instance cast to this type.
	 * Returns nullptr if the world context is invalid or the Game Instance
	 * class has not been set to UcharactersGameInstance in Project Settings.
	 *
	 * Usage from any UObject:
	 *   if (UcharactersGameInstance* GI = UcharactersGameInstance::Get(this))
	 *       GI->ActivePlayerCharacter->DoSomething();
	 */
	static UcharactersGameInstance* Get(const UObject* WorldContextObject);

	virtual void Init() override;
	virtual void Shutdown() override;

	UFUNCTION(BlueprintCallable, Category="Networking|HTTP Server")
	FString GetLocalHttpServerStatusText() const;

	UFUNCTION(BlueprintCallable, Category="Networking|Platform")
	void SendEventToPlatform(const FString& EventName, const FString& Message, const FString& SourceOverride = TEXT(""));

	UPROPERTY(Config, EditAnywhere, Category="Networking|HTTP Server")
	bool bEnableLocalHttpServer = true;

	UPROPERTY(Config, EditAnywhere, Category="Networking|HTTP Server", meta=(ClampMin="1024", ClampMax="65535"))
	int32 LocalHttpServerPort = 30080;

	UPROPERTY(Config, EditAnywhere, Category="Networking|Platform")
	bool bEnablePlatformForwarding = true;

	UPROPERTY(Config, EditAnywhere, Category="Networking|Platform")
	FString PlatformBaseUrl = TEXT("http://127.0.0.1:8000");

	UPROPERTY(Config, EditAnywhere, Category="Networking|Platform")
	FString PlatformEventEndpoint = TEXT("/api/unreal/event");

	UPROPERTY(Config, EditAnywhere, Category="Networking|Platform")
	FString PlatformSessionId;

private:

	bool HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleEchoRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool HandleNotifyRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	FString BuildPlatformUrl() const;
	void StartLocalHttpServer();
	void StopLocalHttpServer();

	TSharedPtr<IHttpRouter> LocalHttpRouter;
	TArray<FHttpRouteHandle> RouteHandles;
};

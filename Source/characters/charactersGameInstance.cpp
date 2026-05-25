// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersGameInstance.h"
#include "characters.h"
#include "charactersCharacter.h"
#include "charactersHUD.h"
#include "Engine/World.h"
#include "HttpPath.h"
#include "IHttpRouter.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	const TCHAR* GetBuildConfigurationLabel()
	{
#if UE_BUILD_SHIPPING
		return TEXT("Shipping");
#elif UE_BUILD_DEVELOPMENT
		return TEXT("Development");
#elif UE_BUILD_DEBUG
		return TEXT("Debug");
#else
		return TEXT("Other");
#endif
	}

	FString EscapeJson(const FString& InText)
	{
		FString Out = InText;
		Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
		Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
		Out.ReplaceInline(TEXT("\n"), TEXT("\\n"));
		Out.ReplaceInline(TEXT("\r"), TEXT("\\r"));
		Out.ReplaceInline(TEXT("\t"), TEXT("\\t"));
		return Out;
	}

	void AddBoundRoute(
		const TSharedPtr<IHttpRouter>& Router,
		TArray<FHttpRouteHandle>& Handles,
		const TCHAR* Path,
		EHttpServerRequestVerbs Verbs,
		const FHttpRequestHandler& Handler)
	{
		if (!Router.IsValid())
		{
			return;
		}

		FHttpRouteHandle Handle = Router->BindRoute(FHttpPath(Path), Verbs, Handler);
		if (Handle.IsValid())
		{
			Handles.Add(Handle);
		}
	}
}

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

void UcharactersGameInstance::Init()
{
	Super::Init();

	UE_LOG(Logcharacters, Log,
		TEXT("Startup: Build=%s HTTPServerEnabled=%s Port=%d"),
		GetBuildConfigurationLabel(),
		bEnableLocalHttpServer ? TEXT("true") : TEXT("false"),
		LocalHttpServerPort);

	StartLocalHttpServer();
}

void UcharactersGameInstance::Shutdown()
{
	StopLocalHttpServer();
	Super::Shutdown();
}

FString UcharactersGameInstance::BuildPlatformUrl() const
{
	const FString Base = PlatformBaseUrl.EndsWith(TEXT("/")) ? PlatformBaseUrl.LeftChop(1) : PlatformBaseUrl;
	const FString Path = PlatformEventEndpoint.StartsWith(TEXT("/")) ? PlatformEventEndpoint : FString::Printf(TEXT("/%s"), *PlatformEventEndpoint);
	return Base + Path;
}

void UcharactersGameInstance::SendEventToPlatform(const FString& EventName, const FString& Message, const FString& SourceOverride)
{
	if (!bEnablePlatformForwarding)
	{
		UE_LOG(Logcharacters, Verbose, TEXT("Platform forwarding disabled. Event '%s' was not sent."), *EventName);
		return;
	}

	const FString RequestUrl = BuildPlatformUrl();
	if (RequestUrl.IsEmpty())
	{
		UE_LOG(Logcharacters, Warning, TEXT("Platform forwarding URL is empty. Configure PlatformBaseUrl/PlatformEventEndpoint."));
		return;
	}

	TSharedRef<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("source"), SourceOverride.IsEmpty() ? TEXT("unreal") : SourceOverride);
	Payload->SetStringField(TEXT("event"), EventName);
	Payload->SetStringField(TEXT("message"), Message);
	Payload->SetStringField(TEXT("session_id"), PlatformSessionId.IsEmpty() ? TEXT("default") : PlatformSessionId);
	Payload->SetStringField(TEXT("timestamp_utc"), FDateTime::UtcNow().ToIso8601());

	TSharedRef<FJsonObject> Metadata = MakeShared<FJsonObject>();
	Metadata->SetStringField(TEXT("map"), GetWorld() ? GetWorld()->GetMapName() : TEXT("unknown"));
	Metadata->SetStringField(TEXT("build"), GetBuildConfigurationLabel());
	Payload->SetObjectField(TEXT("metadata"), Metadata);

	FString RequestBody;
	{
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBody);
		if (!FJsonSerializer::Serialize(Payload, Writer))
		{
			UE_LOG(Logcharacters, Warning, TEXT("Failed to serialize platform event payload for '%s'."), *EventName);
			return;
		}
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(RequestUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(RequestBody);

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, EventName](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			if (!bWasSuccessful)
			{
				UE_LOG(Logcharacters, Warning, TEXT("Platform event '%s' failed to send (network failure)."), *EventName);
				return;
			}

			if (!Response.IsValid())
			{
				UE_LOG(Logcharacters, Warning, TEXT("Platform event '%s' received no HTTP response."), *EventName);
				return;
			}

			const int32 StatusCode = Response->GetResponseCode();
			if (StatusCode >= 200 && StatusCode < 300)
			{
				UE_LOG(Logcharacters, Log, TEXT("Platform event '%s' acknowledged [%d]."), *EventName, StatusCode);

				bool bAgentRunning = false;
				FString AgentAction;
				TSharedPtr<FJsonObject> ResponseJson;
				const FString ResponseBody = Response->GetContentAsString();
				const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseBody);
				if (FJsonSerializer::Deserialize(Reader, ResponseJson) && ResponseJson.IsValid())
				{
					ResponseJson->TryGetBoolField(TEXT("agent_running"), bAgentRunning);
					ResponseJson->TryGetStringField(TEXT("agent_action"), AgentAction);
				}

				FString HudMessage;
				if (!AgentAction.IsEmpty())
				{
					HudMessage = FString::Printf(
						TEXT("Agent %s (%s)"),
						*AgentAction.ToUpper(),
						bAgentRunning ? TEXT("RUNNING") : TEXT("STOPPED"));
				}
				else if (ResponseJson.IsValid())
				{
					HudMessage = FString::Printf(TEXT("Agent %s"), bAgentRunning ? TEXT("RUNNING") : TEXT("STOPPED"));
				}

				if (!HudMessage.IsEmpty())
				{
					if (UWorld* World = GetWorld())
					{
						if (APlayerController* PC = World->GetFirstPlayerController())
						{
							if (AcharactersHUD* HUD = Cast<AcharactersHUD>(PC->GetHUD()))
							{
								HUD->AddTransientMessage(HudMessage, bAgentRunning ? FColor::Green : FColor::Yellow, 2.0f);
							}
						}
					}
				}
			}
			else
			{
				UE_LOG(Logcharacters, Warning,
					TEXT("Platform event '%s' returned HTTP %d. Body: %s"),
					*EventName,
					StatusCode,
					*Response->GetContentAsString());
			}
		});

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(Logcharacters, Warning, TEXT("Failed to dispatch platform event '%s' request."), *EventName);
	}
}

FString UcharactersGameInstance::GetLocalHttpServerStatusText() const
{
	const TCHAR* EnabledText = bEnableLocalHttpServer ? TEXT("enabled") : TEXT("disabled");
	const TCHAR* BoundText = LocalHttpRouter.IsValid() ? TEXT("bound") : TEXT("not-bound");

	return FString::Printf(
		TEXT("HTTP %s, port=%d, router=%s, endpoints=/health,/echo,/notify"),
		EnabledText,
		LocalHttpServerPort,
		BoundText);
}

void UcharactersGameInstance::StartLocalHttpServer()
{
	if (!bEnableLocalHttpServer)
	{
		UE_LOG(Logcharacters, Log, TEXT("HTTP server disabled by config."));
		return;
	}

	if (!FHttpServerModule::IsAvailable())
	{
		FModuleManager::LoadModuleChecked<FHttpServerModule>(TEXT("HTTPServer"));
	}

	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	LocalHttpRouter = HttpServerModule.GetHttpRouter(static_cast<uint32>(LocalHttpServerPort), true);
	if (!LocalHttpRouter.IsValid())
	{
		UE_LOG(Logcharacters, Warning, TEXT("HTTP server failed to bind port %d."), LocalHttpServerPort);
		return;
	}

	RouteHandles.Reset();

	const FHttpRequestHandler HealthHandler = FHttpRequestHandler::CreateUObject(this, &UcharactersGameInstance::HandleHealthRequest);
	const FHttpRequestHandler EchoHandler = FHttpRequestHandler::CreateUObject(this, &UcharactersGameInstance::HandleEchoRequest);
	const FHttpRequestHandler NotifyHandler = FHttpRequestHandler::CreateUObject(this, &UcharactersGameInstance::HandleNotifyRequest);

	// Register both variants because Unreal routes are exact-path matches.
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/health"), EHttpServerRequestVerbs::VERB_GET, HealthHandler);
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/health/"), EHttpServerRequestVerbs::VERB_GET, HealthHandler);

	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/echo"), EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST, EchoHandler);
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/echo/"), EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST, EchoHandler);

	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/notify"), EHttpServerRequestVerbs::VERB_POST, NotifyHandler);
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/notify/"), EHttpServerRequestVerbs::VERB_POST, NotifyHandler);

	HttpServerModule.StartAllListeners();
	UE_LOG(Logcharacters, Log, TEXT("HTTP server listening on port %d. Endpoints: /health, /echo, /notify"), LocalHttpServerPort);
}

void UcharactersGameInstance::StopLocalHttpServer()
{
	if (!LocalHttpRouter.IsValid())
	{
		return;
	}

	for (FHttpRouteHandle& RouteHandle : RouteHandles)
	{
		if (RouteHandle.IsValid())
		{
			LocalHttpRouter->UnbindRoute(RouteHandle);
			RouteHandle.Reset();
		}
	}
	RouteHandles.Reset();

	LocalHttpRouter.Reset();
	UE_LOG(Logcharacters, Log, TEXT("HTTP server routes unbound."));
}

bool UcharactersGameInstance::HandleHealthRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	const FString Response = FString::Printf(
		TEXT("{\"status\":\"ok\",\"map\":\"%s\",\"build\":\"%s\"}"),
		*EscapeJson(GetWorld() ? GetWorld()->GetMapName() : FString(TEXT("unknown"))),
#if UE_BUILD_SHIPPING
		TEXT("Shipping")
#elif UE_BUILD_DEVELOPMENT
		TEXT("Development")
#else
		TEXT("Other")
#endif
	);

	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(Response, TEXT("application/json"));
	HttpResponse->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(HttpResponse));
	return true;
}

bool UcharactersGameInstance::HandleEchoRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	FString EchoText;
	if (const FString* QueryValue = Request.QueryParams.Find(TEXT("msg")))
	{
		EchoText = *QueryValue;
	}
	else
	{
		if (Request.Body.Num() > 0)
		{
			FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
			EchoText = FString(Converter.Length(), Converter.Get());
		}
	}

	const FString Response = FString::Printf(TEXT("{\"echo\":\"%s\"}"), *EscapeJson(EchoText));
	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(Response, TEXT("application/json"));
	HttpResponse->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(HttpResponse));
	return true;
}

bool UcharactersGameInstance::HandleNotifyRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Parse the JSON body to extract the "message" field.
	FString NotifyMessage;
	FString RawBody;
	if (Request.Body.Num() > 0)
	{
		FUTF8ToTCHAR Converter(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
		RawBody = FString(Converter.Length(), Converter.Get());
	}

	if (!RawBody.IsEmpty())
	{
		TSharedPtr<FJsonObject> JsonObj;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawBody);
		if (FJsonSerializer::Deserialize(Reader, JsonObj) && JsonObj.IsValid())
		{
			JsonObj->TryGetStringField(TEXT("message"), NotifyMessage);
		}
	}

	if (NotifyMessage.IsEmpty())
	{
		NotifyMessage = RawBody.IsEmpty() ? TEXT("(no message)") : RawBody;
	}

	// Display the message on the player's HUD.
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (AcharactersHUD* HUD = Cast<AcharactersHUD>(PC->GetHUD()))
			{
				HUD->AddTransientMessage(NotifyMessage, FColor::Cyan, 3.0f);
			}
		}
	}

	UE_LOG(Logcharacters, Log, TEXT("Platform notify received: %s"), *NotifyMessage);

	const FString Response = TEXT("{\"ok\":true}");
	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(Response, TEXT("application/json"));
	HttpResponse->Code = EHttpServerResponseCodes::Ok;
	OnComplete(MoveTemp(HttpResponse));
	return true;
}

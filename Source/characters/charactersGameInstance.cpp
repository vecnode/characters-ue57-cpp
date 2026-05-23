// Copyright (c) vecnode 2026. All Rights Reserved.

#include "charactersGameInstance.h"
#include "characters.h"
#include "charactersCharacter.h"
#include "Engine/World.h"
#include "HttpPath.h"
#include "IHttpRouter.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"

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

FString UcharactersGameInstance::GetLocalHttpServerStatusText() const
{
	const TCHAR* EnabledText = bEnableLocalHttpServer ? TEXT("enabled") : TEXT("disabled");
	const TCHAR* BoundText = LocalHttpRouter.IsValid() ? TEXT("bound") : TEXT("not-bound");

	return FString::Printf(
		TEXT("HTTP %s, port=%d, router=%s, endpoints=/health,/echo"),
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

	// Register both variants because Unreal routes are exact-path matches.
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/health"), EHttpServerRequestVerbs::VERB_GET, HealthHandler);
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/health/"), EHttpServerRequestVerbs::VERB_GET, HealthHandler);

	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/echo"), EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST, EchoHandler);
	AddBoundRoute(LocalHttpRouter, RouteHandles, TEXT("/echo/"), EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST, EchoHandler);

	HttpServerModule.StartAllListeners();
	UE_LOG(Logcharacters, Log, TEXT("HTTP server listening on port %d. Endpoints: /health, /echo"), LocalHttpServerPort);
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

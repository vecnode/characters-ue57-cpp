#include "MetaAgentPlugin.h"

DEFINE_LOG_CATEGORY(LogMetaAgentPlugin);

#define LOCTEXT_NAMESPACE "FMetaAgentPluginModule"

void FMetaAgentPluginModule::StartupModule()
{
	UE_LOG(LogMetaAgentPlugin, Log, TEXT("MetaAgentPlugin module startup."));
}

void FMetaAgentPluginModule::ShutdownModule()
{
	UE_LOG(LogMetaAgentPlugin, Log, TEXT("MetaAgentPlugin module shutdown."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMetaAgentPluginModule, MetaAgentPlugin)

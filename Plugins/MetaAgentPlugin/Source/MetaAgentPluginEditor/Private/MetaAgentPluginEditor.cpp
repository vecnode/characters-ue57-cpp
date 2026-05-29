#include "MetaAgentPluginEditor.h"

#include "Logging/LogMacros.h"

void FMetaAgentPluginEditorModule::StartupModule()
{
	UE_LOG(LogTemp, Log, TEXT("MetaAgentPluginEditor module startup."));
}

void FMetaAgentPluginEditorModule::ShutdownModule()
{
	UE_LOG(LogTemp, Log, TEXT("MetaAgentPluginEditor module shutdown."));
}

IMPLEMENT_MODULE(FMetaAgentPluginEditorModule, MetaAgentPluginEditor)

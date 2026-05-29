#pragma once

#include "Modules/ModuleManager.h"

class FMetaAgentPluginEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

#include "MetaAgentPluginSettings.h"

UMetaAgentPluginSettings::UMetaAgentPluginSettings()
{
	CategoryName = TEXT("Plugins");
	SectionName = TEXT("MetaAgentPlugin");
}

FName UMetaAgentPluginSettings::GetCategoryName() const
{
	return TEXT("Plugins");
}

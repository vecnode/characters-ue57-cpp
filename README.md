# MetaAgentPlugin

Plugin migration of the gameplay/runtime code from `Source/characters` into a reusable plugin module.

## Current status

- Runtime module created: `MetaAgentPlugin`
- Editor module created: `MetaAgentPluginEditor`
- Plugin-native migrated runtime classes under `Source/MetaAgentPlugin/Migrated`
- Startup class routing set to plugin classes:
	- `GameInstanceClass=/Script/MetaAgentPlugin.MetaAgentGameInstance`
	- `GlobalDefaultGameMode=/Script/MetaAgentPlugin.MetaAgentGameMode`
- Startup runtime hook available via `UMetaAgentRuntimeSubsystem`
- Settings registered via `UMetaAgentPluginSettings`
- Blueprint interaction class available via `AMetaAgentMainActor` (`bActive`, Activate/Deactivate/Toggle)

## Startup behavior

When enabled, the plugin loads at game startup and initializes `UMetaAgentRuntimeSubsystem`.

`UMetaAgentRuntimeSubsystem`:
- Reads active/feature flags from plugin settings
- Hooks world begin play and logs startup orchestration point
- Provides `IsActive()` to Blueprints/C++

## Migration target mapping

Migrated runtime folders in plugin:

- `Migrated/Core`
- `Migrated/Gameplay/AI`
- `Migrated/Gameplay/Characters`
- `Migrated/Gameplay/Controllers`
- `Migrated/Gameplay/Modes`
- `Migrated/Systems/Autopilot`
- `Migrated/Systems/Camera`
- `Migrated/Systems/Diagnostics`
- `Migrated/Systems/Input`
- `Migrated/Systems/Networking`
- `Migrated/Systems/Recording`
- `Migrated/Systems/Runtime`
- `Migrated/UI/HUD`

## Next implementation slices

1. Add CoreRedirects from `/Script/characters.*` to `/Script/MetaAgentPlugin.*` if assets still reference old native classes.
2. Migrate remaining direct runtime dependencies from project module to plugin-native paths.
3. Remove now-redundant project-module class usage once validation is complete.

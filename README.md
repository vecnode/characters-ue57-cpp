# MetaAgentPlugin

- UE5 gameplay plugin with reusable runtime systems.
- Modules: MetaAgentPlugin (Runtime), MetaAgentPluginEditor (Editor).
- Plugin deps: EnhancedInput, StateTree, GameplayStateTree, MovieRenderPipeline, Takes (Editor only).

## Current Scope

- Runtime code is centralized under Source/MetaAgentPlugin/Core, Gameplay, Systems, UI, Public, and Private.
- Support/plugin-native classes live under Source/MetaAgentPlugin/Public and Private.
- Editor module currently provides startup/shutdown hooks with logging.

## Main Runtime Pieces

- Runtime gate: global flag GMetaAgentRuntimeActive.
- Settings: UMetaAgentPluginSettings with feature flags and networking config.
- Subsystem path: UMetaAgentRuntimeSubsystem with active-state API and HTTP/platform helpers.
- Gameplay path: UMetaAgentGameInstance + AMetaAgentGameMode + AMetaAgentPlayerController.
- Blueprint helper: UMetaAgentBlueprintLibrary exposes runtime active query.
- World toggle actor: AMetaAgentMainActor with Activate/Deactivate/Toggle.

## Implemented Features

- Character/controller runtime with Enhanced Input + keyboard/mouse fallback.
- Autopilot handoff to AI controller and runtime patrol behavior tree.
- Cinematic orbit camera mode with runtime camera actor.
- HUD transient messages and persistent status lines.
- Take Recorder + Movie Render Queue flow for autopilot takes.
- HTTP server endpoints: /health, /echo, /notify.
- Outbound platform event forwarding with JSON payload + response-driven HUD feedback.

## Important Findings

- Startup architecture is currently mixed: subsystem path and game instance path both exist.
- Networking/server logic is duplicated across subsystem and game instance paths.
- Subsystem defines StartLocalHttpServer but does not invoke it in Initialize.
- Game instance path invokes StartLocalHttpServer in Init.
- Project config currently does not set plugin-native GameInstanceClass in DefaultEngine.ini.
- Project config currently uses a blueprint GlobalDefaultGameMode path.

## New Project Possession Checklist

- Set your startup game mode to AMetaAgentGameMode (or a BP derived from it).
- Place exactly one MetaHuman pawn/character in the level with name MAIN_CHARACTER.
- For strict production behavior, keep:
	- bRequireExactPreferredPawnName=True
	- bRequireUniquePreferredPawnName=True
	- bAllowSpawnFallback=False
- If the placed MetaHuman has no spring-arm/camera rig, the controller now adds a runtime third-person camera fallback.
- If you want another actor name, change PreferredPlacedPawnName in MetaAgentGameMode config/defaults.


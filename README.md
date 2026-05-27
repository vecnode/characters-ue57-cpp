# UE5 MetaHuman Control Template

UE 5.7 C++ project focused on direct possession of a placed MetaHuman pawn and a small set of runtime systems.

## Dependencies

Lorem ipsum.

## Source Features
- `charactersGameMode`
	- Possesses a placed pawn first, using name, tag, class, or first valid fallback.
	- Keeps existing player possession when one already exists.
	- Falls back to normal spawning only when enabled.
	- Updates or creates NavMesh bounds at startup and builds navigation.
- `charactersPlayerController`
	- Sets up pawn camera control and disables UE camera auto-management.
	- Applies mouse-wheel zoom with clamping and smooth interpolation.
	- Syncs mesh components through `LeaderPose` when needed.
	- Forces useful movement defaults when a possessed character looks underconfigured.
- `AcharactersCharacter`
	- Third-person character base with spring arm and follow camera.
	- Binds move, look, and jump input actions.
	- Exposes Blueprint-callable move/look/jump helpers.
- `AcharactersMHPlayer`
	- Marks the active player character in the game instance.
	- Serves as the recommended Blueprint parent for the possessed MetaHuman.
- `UcharactersGameInstance`
	- Starts a local HTTP server with `/health`, `/echo`, and `/notify` routes.
	- Sends platform events over HTTP when forwarding is enabled.
	- Pushes HTTP responses and notifications to the HUD.
- `AcharactersHUD`
	- Draws transient messages.
	- Draws persistent status lines in a small overlay panel.
- `AcharactersWanderAIController`
	- Builds a runtime behavior tree for wandering patrol AI.
	- Uses a runtime blackboard and patrol radius / wait settings.
- `UcharactersBTTask_SetRandomPatrolPoint`
	- Picks reachable patrol points from navigation data.
	- Uses a fallback point when nav data is missing.
- `UcharactersBTTask_MoveToPatrolPoint`
	- Moves an AI pawn toward the selected patrol point.
	- Detects stuck movement and times out failed movement attempts.

## Run
- Open the workspace and press `Ctrl + F5`.
- Use the `charactersEditor Win64 Development` launch config.
- In PIE, confirm a placed pawn exists if you want direct possession.


## Project (C++)

- `Source/characters/Core`
	- `characters.h/.cpp`: module log category and module bootstrap.
- `Source/characters/Gameplay/Characters`
	- `charactersCharacter.*`: base playable character, enhanced input action binding (`Move`, `Look`, `Jump`).
	- `charactersMHPlayer.*`: MetaHuman-oriented player pawn integration with `GameInstance`.
- `Source/characters/Gameplay/Controllers`
	- `charactersPlayerController.*`: orchestration layer for input setup, possession flow, and runtime feature coordination.
- `Source/characters/Gameplay/Modes`
	- `charactersGameMode.*`: placed-pawn-first possession and runtime navmesh setup.
- `Source/characters/Gameplay/AI`
	- `charactersWanderAIController.*`: runtime wandering AI controller.
	- `Tasks/charactersBTTask_SetRandomPatrolPoint.*`: chooses reachable patrol points.
	- `Tasks/charactersBTTask_MoveToPatrolPoint.*`: drives AI movement to patrol points.
- `Source/characters/UI/HUD`
	- `charactersHUD.*`: transient notifications and persistent status lines.
- `Source/characters/Systems/Runtime`
	- `charactersGameInstance.*`: global runtime instance hooks and accessors.
- `Source/characters/Systems/Networking`
	- `charactersGameInstanceNetworking.cpp`: local HTTP server (`/health`, `/echo`, `/notify`) and platform event forwarding.
- `Source/characters/Systems/Camera`
	- `charactersPlayerControllerCamera.cpp`: zoom and cinematic orbit camera implementation.
- `Source/characters/Systems/Input`
	- `charactersPlayerControllerInputFallback.cpp`: raw keyboard/mouse fallback input path.
- `Source/characters/Systems/Diagnostics`
	- `charactersPlayerControllerDiagnostics.cpp`: movement/animation diagnostics and probe telemetry.
- `Source/characters/Systems/Autopilot`
	- `charactersPlayerControllerAutopilot.cpp`: runtime AI autopilot handoff and repossession.
- `Source/characters/Systems/Recording`
	- `charactersPlayerControllerRecording.cpp`: Take Recorder + Movie Render Queue flow.

## Keyboard

- `W/A/S/D` or Arrow keys: fallback movement input (when fallback is enabled).
- `Mouse move`: look input (Enhanced Input path).
- `Mouse wheel`: camera zoom in/out.
- `Space`: jump (Enhanced Input jump action).
- `Escape`: quit game.
- `Y`: send platform toggle event (`key_pressed/toggle_agent`).
- `J`: toggle autopilot on/off (also starts/stops take recording flow).
- `U`: render last recorded take via MRQ.
- `V`: toggle cinematic camera mode on/off.
- `H`: print debug hello message from character class.

Note: movement/look/jump depend on your Input Mapping Context assets (`IMC_Default`, `IMC_MouseLook`).
If those mappings are missing, fallback movement/look still work from the controller's raw input path.

## License

Licensed under [MIT License](LICENSE)
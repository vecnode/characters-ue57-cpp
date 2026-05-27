# UE5 MetaHuman Template

UE 5.7 C++ project focused on direct possession of a placed MetaHuman pawn and a small set of runtime systems.

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

## License

Licensed under [MIT License](LICENSE)
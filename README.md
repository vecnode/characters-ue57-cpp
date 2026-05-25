# characters-ue57-cpp

UE 5.7 C++ project.
Direct possession of a placed MetaHuman Pawn/Character.

Status: Ongoing.

## Start
- Keep one placed actor named `MAIN_CHARACTER`.
- Possess that actor directly at runtime.
- Avoid default spawn flow unless fallback is enabled.

## Classes
- `charactersGameMode`
	- `RestartPlayer` searches for `MAIN_CHARACTER` first.
	- Falls back to normal spawn only when configured.
- `charactersPlayerController`
	- Camera authority for view target and third-person camera state.
	- Mouse wheel zoom with clamp + interpolation.
	- Guards against wheel key/axis double-apply.
- `AcharactersMHPlayer` (recommended BP parent)
	- Stable direct possession target.
	- MetaHuman mesh sync via `LeaderPose`.

## Level Setup
- Place 1 Pawn/Character named `MAIN_CHARACTER`.
- Prefer a Blueprint derived from `AcharactersMHPlayer`.
- Set gameplay classes to:
	- `charactersGameMode`
	- `charactersPlayerController`

## Folders in VC

- `Source/`
	- Main C++ gameplay code and targets.
- `Config/`
	- Project runtime and editor settings.
- `Content/`
	- Maps, Blueprints, MetaHumans, assets.

## Run
- In VS Code: press `Ctrl + F5`.
- Build task: `charactersEditor Win64 Development`.
- Start PIE and confirm one `MAIN_CHARACTER` exists.


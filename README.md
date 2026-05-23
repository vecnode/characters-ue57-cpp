# characters-ue57-cpp

UE 5.7 C++ project for direct player possession of a placed MetaHuman Pawn/Character.

Ongoing. AI not implemented yet. On VSCode press Ctrl + F5

## Required Level Setup
- Place 1 Pawn/Character in the level named `MAIN_CHARACTER`.
- Recommended: use a Blueprint derived from `AcharactersMHPlayer` so possession is direct and production-safe.
- Use `charactersGameMode` and `charactersPlayerController` as active gameplay classes.

## Setup Notes
- Reparent your character Blueprint to a Pawn/Character class (`AcharactersMHPlayer` recommended).
- Keep the placed actor name `MAIN_CHARACTER` (or use the configured tag/class filters).

## What the C++ Layer Does
1. `charactersGameMode::RestartPlayer` looks for `MAIN_CHARACTER` first.
2. Direct pawn possession is used for placed Pawn/Character actors.
3. If no placed pawn is found and spawn fallback is enabled, normal pawn spawn is used.
4. `charactersPlayerController` is the single camera authority (view target setup + third-person camera state).
5. Mouse wheel zoom updates spring-arm distance with clamp and interpolation, without double-applying when both key and axis wheel events fire.

## Features
- Uses Character movement, possession, camera, and input systems safely.
- Uses direct Pawn/Character possession architecture.

## Run
- Build: `charactersEditor Win64 Development`.
- Start PIE and verify one `MAIN_CHARACTER` exists in the active level.


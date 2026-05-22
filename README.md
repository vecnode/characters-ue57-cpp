# characters-ue57-cpp

UE 5.7 C++ project that enables player control of a placed MetaHuman actor not derived from APawn.

## Required Level Setup
- Place 1 MetaHuman actor in the level named `MAIN_CHARACTER`.
- `MAIN_CHARACTER` can be a Blueprint Actor (for example `BP_character2_C`), not a `Pawn`.
- Use `charactersGameMode` and `charactersPlayerController` as active gameplay classes.

## What the C++ Layer Does
1. `charactersGameMode::RestartPlayer` looks for `MAIN_CHARACTER` first.
2. If `MAIN_CHARACTER` is not a `Pawn`, the game spawns `AcharactersMHPlayer` at the same transform.
3. The original MetaHuman actor is attached to the spawned pawn with `KeepWorldTransform`.
4. The original actor remains visible; collision is disabled to avoid duplicate physics.
5. A source skeletal mesh (with `AnimClass`) is selected from `MAIN_CHARACTER`.
6. That mesh and anim class are copied to the hidden pawn mesh (animation driver).
7. Driver mesh tick is forced to `AlwaysTickPoseAndRefreshBones`.
8. Compatible source meshes are linked with `LeaderPose` to follow the driver animation.
9. `charactersPlayerController` possesses `AcharactersMHPlayer` and sets third-person camera state.
10. Mouse wheel zoom updates spring-arm distance with clamp and interpolation.

## Features
- Uses Character movement, possession, camera, and input systems safely.
- Avoids fragile runtime cloning of full MetaHuman component hierarchies.

## Run
- Build: `charactersEditor Win64 Development`.
- Start PIE and verify one `MAIN_CHARACTER` exists in the active level.


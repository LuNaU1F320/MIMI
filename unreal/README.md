# Unreal Engine

The Unreal domain owns runtime content behavior, WebSocket integration with the backend, presentation mode, recreation mode, immersive visual feedback, and Blueprint extension points.

## Current Status

This folder is a placeholder for the first Unreal Engine implementation. No `.uproject` or production Unreal code has been added yet.

## Expected First Slice

- Connect to local backend WebSocket server
- Parse shared protocol messages
- Receive `session.state`
- Receive `controller.input`
- Trigger basic presentation and recreation behaviors
- Display simple per-user visual feedback

## Integration Note Requirement

Any Unreal feature that consumes or emits network messages must reference `shared/protocol/WebSocketMessages.md` and name the schemas it depends on.

## Local Demo: Sample Host Controller to PlayWorld

The Unreal demo can consume the existing sample server at `C:\workspace\Hackathon_Sample\Hackathon_Sample` without copying backend code into this repository.

- Affected domain: Unreal only in this repository.
- Backend source: external sample server on `http://localhost:3000`.
- Protocol surface: the sample HTTP endpoint `GET /api/unreal/inputs`, which represents controller movement input for the Unreal runtime.
- Unreal entry point: place one `ControllerInputPollingBridge` bootstrap actor in the level, or spawn it from a Blueprint/test level.
- Local test:
  1. Run `npm start` in `C:\workspace\Hackathon_Sample\Hackathon_Sample`, or let the Unreal bridge start that Node server from PIE.
  2. Open `http://localhost:3000/host.html`.
  3. Start PlayWorld PIE with a `ControllerInputPollingBridge` bootstrap actor in the level.
  4. Join from the mobile page and press Start Game in the host page.

The bridge polls controller input every `0.1s`, maps up to `MaxDemoPlayers` non-bot sample server inputs to `AMyCharacter` instances, and maps up to `MaxDemoBots` sample server `bot_` inputs to moving Unreal bot characters. Web-created bots are randomly spawned inside `BotSpawnCenter +/- BotSpawnAreaExtent`, then receive the same `moveX` and `moveY` values reported by the sample server. `BotCount` is a local fallback and defaults to `0`; use the host page's bot controls for the demo. If the sample server is unavailable or returns no input, controlled demo characters are stopped.

When the sample server enters `Result`, only the winner keeps sending controller input. The mobile winner view keeps the joystick area visible and replaces the minimap area with a victory message. Non-winners still move to the result screen. This reuses the existing `moveInput` and `inputsUpdated` messages and does not add protocol fields.

Packaged builds stage the sample Node server beside the real packaged game executable, typically `PlayWorld/Binaries/Win64/Hackathon_Sample` under the package root. The top-level `PlayWorld.exe` is a launcher, so the server folder is not expected to appear beside that launcher. The bridge checks the editor project sibling path first, then packaged launch/executable-relative paths, and logs every searched path if the server script is missing. Node.js must still be installed on the machine running the build.

## Local Demo: Circular Field Boundary

The PlayWorld battle royale map uses `BattleRoyaleSettings.MapCenter` for battle royale zone logic, and `BoundaryCenter` for the runtime circular collision boundary. The visible map can keep the default `(3000, 3000)` extent, while `BoundaryRadius` defaults to `3000` so the runtime boundary matches the initial full map radius.

- Affected domains: Unreal and the sample Node server.
- Protocol surface: no new messages; existing player `posX` and `posY` percentages are constrained to the circular field.
- Runtime behavior: `ControllerInputBridgeSubsystem` gives registered characters the circular movement boundary so outward input is removed at the edge while tangent movement remains possible. It also clamps registered player and bot locations back inside the circle every `0.05s` as a fallback if collision or spawn placement bypasses input filtering.
- Editor configuration: select the `ControllerInputPollingBridge` actor and edit `BattleRoyale|Boundary > BoundaryCenter`, `BoundaryRadius`, and `BoundaryClampMargin`. The red debug circle shows the effective character-center clamp line, which subtracts the configured margin and the character capsule radius.
- Spawn behavior: backend player preview positions, Unreal player spawns, bot spawns, and supply drops are generated or clamped inside the circle.
- Local test: start PIE with one `ControllerInputPollingBridge`, join from the mobile page, spawn bots from the host page, and verify characters stop at the circular edge while host positions remain inside the round field.

## Runtime GameMode

`APlayWorldGameMode` is the project default GameMode. It disables default pawn, HUD, and spectator pawn spawning so PIE does not create an extra player pawn. Unreal still creates a local `PlayerController` for viewport ownership, camera view targets, and UMG widgets; gameplay input is handled by the bridge subsystem instead of a possessed pawn.

## Local Demo: Battle Royale Rules

The `ControllerInputPollingBridge` keeps level-editable demo settings in its Details panel, then starts `UControllerInputBridgeSubsystem`. The subsystem owns WebSocket input, character mapping, world-state sync, and `UShowdownBattleRoyaleSubsystem` configuration. Keep one bridge actor in the level as the bootstrap/config source.

- `BattleRoyaleSettings.MapCenter`: world-space center for minimap, safe zone, and supply placement.
- `BattleRoyaleSettings.MapExtent`: half-size of the playable map.
- `BattleRoyaleSettings.PhaseDuration`: safe-zone phase duration, default `15s`.
- `BattleRoyaleSettings.PhaseCount`: safe-zone phase count, default `4`.
- Host state source: `GET /api/status` from the sample server.
- Start condition: sample host state becomes `Playing`.
- Reset condition: sample host state leaves `Playing`.
- Runtime systems: circular current/next safe-zone display, minimap widget, per-phase supply drop, zone damage, and stacked supply equipment effects.
- Attack start behavior: newly joined or spawned characters do not auto-attack while the host room is waiting. `StartBattleRoyale()` enables auto-attack for registered players and bots only after the sample host state becomes `Playing`.
- Death behavior: when an `AMyCharacter` reaches `0` HP, Unreal hides the character actor, removes it from the Unreal minimap, disables collision, stops movement, and keeps reporting `alive=false` in the existing `worldState` payload so the sample server can update ranking. `ResetForNextRound()` makes the same actor visible and controllable again.
- Combat debug: attack sweep damage still uses the same overlap checks, but attack range debug lines/spheres and per-attack logs are not emitted.
- Local test: start PIE with one `ControllerInputPollingBridge`, start the sample host game, let a player or bot reach `0` HP through zone or attack damage, and verify the character disappears in Unreal while the host/mobile UI shows the death state.

## Local Demo: Battle Royale Zone Camera

The Battle Royale demo includes a quarter-view camera for the safe-zone system.

- Affected domain: Unreal only.
- Protocol surface: none; this uses local `UShowdownBattleRoyaleSubsystem` state.
- Entry point: keep one `ControllerInputPollingBridge` bootstrap actor in the level. `UControllerInputBridgeSubsystem` will reuse an existing `BattleRoyaleZoneCameraActor` or spawn one when `bAutoCreateZoneCamera` is enabled.
- Default view: the camera starts by framing the full configured map. With the default `MapExtent` of `(3000, 3000)`, the playable field is `6000 x 6000`.
- Framing behavior: the camera defaults to a very low-FOV perspective view (`FOV=12`, `CameraDistance=45000`, `Pitch=-50.473487`, `Yaw=-0.166535`) to reduce trapezoid distortion while keeping a 3D look. Switch `ProjectionMode` to Orthographic only when distortion must be fully removed.
- Initial framing override: `BattleRoyaleZoneCameraActor` defaults `InitialViewExtentOverride` to `(3000, 3000)` so the initial camera fills the viewport with the `6000 x 6000` floor even if level demo settings drift larger.
- Runtime behavior: when the match leaves warmup, the camera follows the current safe-zone center and interpolates its orthographic width to the projected safe-zone bounds. Use `bCoverViewportWithMap` for skybox-free framing or disable it if the full bounds must always remain visible.
- Visual behavior: the current safe zone draws a highlighted interior plus a strong border; the next safe zone draws a border only.
- Local test:
  1. Disable Live Coding or close Unreal Editor before compiling C++.
  2. Start PIE with one `ControllerInputPollingBridge` bootstrap actor in the level.
  3. Start the sample host game so the Battle Royale subsystem enters `Playing`.
  4. Verify the camera smoothly follows each shrinking safe zone and that the ground no longer appears trapezoidal at distance.

Perspective projection cannot fully remove floor trapezoid distortion. The default low-FOV, long-distance camera reduces it while preserving depth.


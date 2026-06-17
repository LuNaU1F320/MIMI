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
- Unreal entry point: place one `ControllerInputPollingBridge` actor in the level, or spawn it from a Blueprint/test level.
- Local test:
  1. Run `npm start` in `C:\workspace\Hackathon_Sample\Hackathon_Sample`.
  2. Open `http://localhost:3000/host.html`.
  3. Start PlayWorld PIE with a `ControllerInputPollingBridge` actor in the level.
  4. Join from the sample mobile page and press Start Game in the host page.

The bridge polls controller input every `0.1s`, maps up to `MaxDemoPlayers` non-bot sample server inputs to `AMyCharacter` instances, and maps up to `MaxDemoBots` sample server `bot_` inputs to moving Unreal bot characters. Web-created bots are randomly spawned inside `BotSpawnCenter +/- BotSpawnAreaExtent`, then receive the same `moveX` and `moveY` values reported by the sample server. `BotCount` is a local fallback and defaults to `0`; use the host page's bot controls for the demo. If the sample server is unavailable or returns no input, controlled demo characters are stopped.


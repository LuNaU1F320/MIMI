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


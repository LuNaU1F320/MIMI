# Backend

The backend owns MIMI room creation, QR/mobile join flows, WebSocket sessions, user presence, controller input routing, and session state synchronization.

## Current Status

This folder is a placeholder for the first backend implementation. No production backend code has been added yet.

## Expected First Slice

- Local session creation
- QR/mobile join URL generation
- WebSocket endpoint
- In-memory session registry
- Shared schema validation
- Broadcast of `controller.input`, `session.state`, `user.joined`, and `user.left`

## Integration Note Requirement

Any backend feature that affects Unreal, AI, Technical Art, or mobile controllers must include a short integration note naming the shared protocol messages involved.


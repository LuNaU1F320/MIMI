# Backend

The backend is the authoritative coordinator for MIMI sessions.

## Responsibilities

- Create rooms and sessions.
- Generate QR Code based mobile join flows.
- Manage WebSocket connections.
- Authenticate or identify mobile browser controllers as the product evolves.
- Connect Unreal Engine clients to active sessions.
- Validate controller input.
- Broadcast controller input to Unreal Engine and authorized observers.
- Synchronize session state.
- Support local-first development.
- Preserve a clean path to cloud deployment.

## Initial Technical Direction

The initial backend can be implemented with either Node.js or Python. The repository does not mandate one yet because the first step is contract stabilization.

When implementation begins, choose one runtime for the first vertical slice and document the decision in `backend/README.md`.

Recommended initial shape:

```text
backend/
  README.md
  src/
    server/
    sessions/
    websocket/
    validation/
  tests/
```

## Session Concepts

- Room: Host-created container for an event or experience.
- Session: Active runtime instance connected to one room.
- Participant: Mobile controller user, Unreal client, host, or observer.
- Join Code: Short code used by QR/mobile flows.
- Session State: Authoritative snapshot sent through `session.state`.

## Backend Rules

- Validate inbound messages against schemas in `shared/schemas/` whenever practical.
- Do not create backend-only message types for real-time integration with Unreal.
- Keep transport logic separate from session state logic.
- Keep cloud adapters separate from local session behavior.
- Include integration notes for every cross-domain feature.

## Local Development Target

The first backend milestone should expose:

- HTTP endpoint for creating a session.
- HTTP endpoint or generated URL for QR Code join.
- WebSocket endpoint for mobile controllers.
- WebSocket endpoint or role negotiation for Unreal Engine.
- In-memory session registry.
- Schema validation for `controller.input`.
- Broadcast of `user.joined`, `user.left`, `controller.input`, and `session.state`.

## Cloud-Ready Path

Future cloud deployment should externalize:

- Session persistence
- Pub/sub fan-out
- TLS and domain routing
- Observability
- Rate limiting
- Authentication

These should be added as adapters, not embedded directly into the core local session loop.


# Unreal Engine

The Unreal Engine domain owns runtime content integration for MIMI.

## Responsibilities

- Connect to the backend WebSocket server.
- Join or attach to an active session.
- Receive multi-user controller input.
- Receive session state and presence updates.
- Convert raw input into gameplay, presentation, or content events.
- Support presentation mode.
- Support recreation mode.
- Render immersive visual feedback.
- Expose Blueprint-friendly extension points when useful.

## Initial Technical Direction

The Unreal implementation should be structured around a small protocol adapter and runtime event layer.

Recommended initial shape:

```text
unreal/
  README.md
  MIMI.uproject
  Source/
    MIMIRuntime/
      Private/
      Public/
  Content/
    MIMI/
```

This repository does not include Unreal project files yet. Add them when the first Unreal vertical slice begins.

## Runtime Layers

### WebSocket Adapter

Owns connection, reconnect behavior, message parsing, and dispatch into Unreal-native events.

### Protocol Mapper

Maps shared messages into Unreal structs and delegates. This layer must preserve shared field names where practical and document any runtime aliases.

### Content Event Layer

Converts protocol-level events into gameplay, presentation, recreation, or visual feedback behavior.

### Blueprint API

Exposes stable events for designers and technical artists:

- User joined
- User left
- Controller input received
- Session state updated
- Content command received
- Heartbeat status changed

## Unreal Rules

- Do not invent private WebSocket message formats.
- Use `shared/protocol/WebSocketMessages.md` as the source of truth.
- Keep protocol parsing separate from gameplay behavior.
- Treat unknown optional fields as forward-compatible.
- Prefer data-driven mappings for input-to-content behavior.
- Coordinate visual triggers with `ta/` naming and style documents.

## First Unreal Milestone

1. Connect to a local backend WebSocket endpoint.
2. Receive `session.state`.
3. Receive `controller.input`.
4. Display simple per-user input visualization.
5. Trigger one presentation action from a `content.command`.
6. Trigger one recreation interaction from controller input.


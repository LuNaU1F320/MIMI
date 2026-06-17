# Protocol

MIMI uses WebSocket messages as the primary real-time contract between mobile controllers, the backend, Unreal Engine, and future tools.

The canonical message reference is `shared/protocol/WebSocketMessages.md`.

## Protocol Goals

- Keep mobile browser clients simple.
- Keep Unreal Engine integration predictable.
- Give the backend a clear validation target.
- Allow AI and tools to observe events without owning the real-time loop.
- Preserve compatibility as features are added.

## Message Envelope

Every WebSocket message should follow this envelope shape:

```json
{
  "type": "controller.input",
  "messageId": "msg_01J9V9N6E4C1S0P7R8A9B2C3D4",
  "sessionId": "ses_demo_001",
  "timestamp": "2026-06-17T04:00:00.000Z",
  "payload": {}
}
```

### Envelope Fields

- `type`: Stable message type.
- `messageId`: Unique message identifier for logging and idempotency.
- `sessionId`: Session this message belongs to, when applicable.
- `timestamp`: ISO 8601 UTC timestamp set by the sender.
- `payload`: Message-specific body.

## Compatibility Rules

- Message type names are lowercase dot-separated strings.
- Consumers should ignore unknown optional payload fields.
- Required fields must not be removed without a documented protocol version decision.
- A message with a stable payload structure should receive a JSON Schema under `shared/schemas/`.
- Schema filenames use PascalCase and end in `.schema.json`.

## Validation Strategy

The backend should validate inbound messages from mobile controllers and Unreal Engine.

Unreal Engine should validate or defensively parse messages at its connection boundary. It should not assume a malformed message can never arrive.

AI consumers should treat event data as external input and validate before analysis.

## Error Handling

Use the `error` message type for recoverable protocol or session errors.

Example:

```json
{
  "type": "error",
  "messageId": "msg_error_001",
  "sessionId": "ses_demo_001",
  "timestamp": "2026-06-17T04:00:05.000Z",
  "payload": {
    "code": "invalid_message",
    "message": "controller.input payload failed schema validation.",
    "retryable": false,
    "correlationId": "msg_01J9V9N6E4C1S0P7R8A9B2C3D4"
  }
}
```

## Message Ownership

- Backend owns validation, routing, presence, and state snapshots.
- Mobile controllers initiate joins and input messages.
- Unreal consumes state, input, and commands.
- AI observes events and returns advisory outputs through future documented messages or backend APIs.

## Schema Coverage

The initial repository includes schemas for:

- `controller.input`
- `session.state`
- `content.command`
- `user.joined`
- `user.left`
- `heartbeat`

`session.create`, `session.join`, and `error` are documented in the protocol reference and should receive schemas when their implementation stabilizes.


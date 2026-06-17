# WebSocket Messages

This document is the canonical WebSocket protocol reference for MIMI.

Backend, Unreal Engine, mobile browser controllers, AI tools, and future utilities must use the message types documented here. No domain may invent private real-time message formats for cross-domain behavior.

## Envelope

All messages use a common envelope:

```json
{
  "type": "controller.input",
  "messageId": "msg_01J9V9N6E4C1S0P7R8A9B2C3D4",
  "sessionId": "ses_demo_001",
  "timestamp": "2026-06-17T04:00:00.000Z",
  "payload": {}
}
```

### Common Fields

- `type`: Message type.
- `messageId`: Unique message ID for tracing.
- `sessionId`: Session ID. Some pre-session messages may omit this until a session is created.
- `timestamp`: ISO 8601 UTC timestamp.
- `payload`: Message-specific payload.

## Message Type Summary

| Type | Direction | Purpose | Schema |
| --- | --- | --- | --- |
| `session.create` | Host client to backend | Request a room/session | Planned |
| `session.join` | Mobile, Unreal, or host client to backend | Join an existing session | Planned |
| `session.state` | Backend to clients | Synchronize session snapshot | `SessionState.schema.json` |
| `user.joined` | Backend to clients | Announce participant join | `UserJoined.schema.json` |
| `user.left` | Backend to clients | Announce participant departure | `UserLeft.schema.json` |
| `controller.input` | Mobile controller to backend, backend to Unreal | Send controller input | `ControllerInput.schema.json` |
| `content.command` | Backend or authorized controller to Unreal and clients | Trigger content behavior | `ContentCommand.schema.json` |
| `heartbeat` | Any connected client or backend | Keepalive and latency check | `Heartbeat.schema.json` |
| `error` | Backend or client | Report recoverable protocol error | Planned |

## `session.create`

Requests creation of a new room/session.

### Direction

Host client to backend.

### Example

```json
{
  "type": "session.create",
  "messageId": "msg_create_001",
  "timestamp": "2026-06-17T04:00:00.000Z",
  "payload": {
    "roomName": "Main Hall Demo",
    "mode": "presentation",
    "hostDisplayName": "Host"
  }
}
```

### Notes

The backend should respond with a `session.state` message after creating the session.

## `session.join`

Requests connection to an existing session.

### Direction

Mobile controller, Unreal Engine client, host dashboard, or observer to backend.

### Example

```json
{
  "type": "session.join",
  "messageId": "msg_join_001",
  "sessionId": "ses_demo_001",
  "timestamp": "2026-06-17T04:00:01.000Z",
  "payload": {
    "joinCode": "MIMI42",
    "role": "controller",
    "displayName": "Player 1",
    "client": {
      "platform": "mobile-web",
      "version": "0.1.0"
    }
  }
}
```

### Notes

Roles should initially be one of:

- `host`
- `controller`
- `unreal`
- `observer`
- `ai`

## `session.state`

Synchronizes authoritative session state.

### Direction

Backend to clients.

### Example Payload

```json
{
  "sessionId": "ses_demo_001",
  "roomId": "room_demo_001",
  "mode": "presentation",
  "status": "active",
  "joinCode": "MIMI42",
  "participantCount": 2,
  "participants": [
    {
      "userId": "user_host_001",
      "displayName": "Host",
      "role": "host",
      "connected": true
    },
    {
      "userId": "user_player_001",
      "displayName": "Player 1",
      "role": "controller",
      "connected": true
    }
  ],
  "content": {
    "activeScene": "Opening",
    "activeCommandId": "cmd_intro_001"
  }
}
```

See `shared/schemas/SessionState.schema.json`.

## `user.joined`

Announces that a user joined the session.

### Direction

Backend to clients.

### Example Payload

```json
{
  "userId": "user_player_001",
  "displayName": "Player 1",
  "role": "controller",
  "joinedAt": "2026-06-17T04:00:02.000Z"
}
```

See `shared/schemas/UserJoined.schema.json`.

## `user.left`

Announces that a user left the session.

### Direction

Backend to clients.

### Example Payload

```json
{
  "userId": "user_player_001",
  "displayName": "Player 1",
  "role": "controller",
  "reason": "disconnect",
  "leftAt": "2026-06-17T04:05:00.000Z"
}
```

See `shared/schemas/UserLeft.schema.json`.

## `controller.input`

Carries raw controller intent from mobile clients through the backend to Unreal Engine.

### Direction

Mobile controller to backend. Backend to Unreal Engine and authorized observers.

### Example Payload

```json
{
  "userId": "user_player_001",
  "inputId": "inp_001",
  "inputType": "button",
  "action": "confirm",
  "value": 1,
  "sequence": 12,
  "clientTime": "2026-06-17T04:00:03.100Z"
}
```

See `shared/schemas/ControllerInput.schema.json`.

### Input Type Guidance

Initial input types:

- `button`
- `axis`
- `gesture`
- `choice`
- `text`

## `content.command`

Requests or announces a content-level action.

### Direction

Backend to Unreal Engine and clients. Authorized host tools may send it to backend.

### Example Payload

```json
{
  "commandId": "cmd_next_slide_001",
  "command": "presentation.next",
  "target": "unreal",
  "issuedBy": "user_host_001",
  "parameters": {
    "transition": "fade"
  }
}
```

See `shared/schemas/ContentCommand.schema.json`.

## `heartbeat`

Keeps connections alive and measures latency.

### Direction

Any connected client to backend. Backend to connected clients.

### Example Payload

```json
{
  "senderId": "user_player_001",
  "senderRole": "controller",
  "sequence": 33,
  "sentAt": "2026-06-17T04:00:04.000Z"
}
```

See `shared/schemas/Heartbeat.schema.json`.

## `error`

Reports a recoverable protocol or session error.

### Direction

Backend or client to the message sender.

### Example

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

## Versioning

The initial protocol version is implicit while the repository is pre-implementation.

When implementation begins, add an explicit protocol version strategy before breaking message compatibility.


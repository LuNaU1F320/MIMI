# Architecture

MIMI is a monorepo organized around domain ownership and shared integration contracts.

## System Overview

```text
Mobile Browser Controllers
        |
        | WebSocket: session.join, controller.input, heartbeat
        v
Backend Session Server
        |
        | WebSocket: session.state, user.joined, user.left, controller.input, content.command
        v
Unreal Engine Client

Optional AI Services
        ^
        | Async session events, snapshots, analytics exports
        |
Backend Session Server

Technical Art Assets and Guidelines
        |
        | Materials, motion graphics, UI references, naming rules
        v
Unreal Engine Content and Mobile UI
```

## Runtime Components

### Backend Session Server

The backend is the authoritative session coordinator. It owns room creation, join codes, QR Code join URLs, WebSocket connection management, user presence, input routing, and session state synchronization.

Initial backend development should prioritize local execution. The backend should avoid hard-coding cloud services into core session logic.

### Mobile Browser Controller

The mobile browser controller connects to the backend through a QR Code flow. It sends `session.join`, `controller.input`, and `heartbeat` messages.

The browser controller should stay thin. It captures input intent and delegates authoritative session behavior to the backend.

### Unreal Engine Client

The Unreal Engine client connects to the backend as a session participant with a runtime role. It receives state, user presence, controller input, and content commands.

Unreal maps raw shared protocol messages into gameplay events, presentation controls, visual feedback, and Blueprint extension points.

### Artificial Intelligence

AI components consume session data asynchronously. They can produce recommendations or analytics but do not own real-time authority.

AI integrations should be connected through explicit backend APIs, files, queues, or event streams introduced later. They should not depend on private Unreal Engine state.

### Technical Art

Technical Art defines visual systems that represent users, input energy, session states, and content reactions.

TA deliverables should include asset names, material parameters, effect triggers, and usage notes so implementation teams can integrate them consistently.

## Data Flow

1. A host creates a room on the backend.
2. The backend generates a session identifier and QR/mobile join URL.
3. Mobile clients open the join URL and send `session.join`.
4. The backend validates the join request and broadcasts `user.joined`.
5. The backend sends `session.state` snapshots to relevant clients.
6. Mobile clients send `controller.input`.
7. The backend validates and broadcasts input to Unreal and other authorized consumers.
8. Unreal converts input into content behavior.
9. Backend or Unreal may emit `content.command` depending on the final ownership model for a specific feature.
10. AI consumes events asynchronously when enabled.

## Authority Model

- Backend is authoritative for sessions, users, room state, and message routing.
- Unreal is authoritative for content runtime interpretation and visual execution.
- AI is advisory unless a future protocol message explicitly promotes an AI recommendation into a command.
- Technical Art is authoritative for visual style rules and asset naming, not runtime session state.

## Integration Boundary

The primary integration boundary is the WebSocket protocol in `shared/protocol/WebSocketMessages.md`.

All cross-domain runtime behavior must reference one of these message types:

- `session.create`
- `session.join`
- `session.state`
- `user.joined`
- `user.left`
- `controller.input`
- `content.command`
- `heartbeat`
- `error`

## Practical Constraints

- Real-time controller input should be lightweight and frequent.
- Session state snapshots should be complete enough for reconnects.
- Unknown fields should be ignored by consumers unless they violate schema validation at the backend edge.
- Message additions should be backward compatible when possible.
- Latency-sensitive paths must not wait on AI processing, asset generation, or cloud-only services.


# Development Flow

MIMI development is local-first and protocol-centered.

## Default Workflow

1. Read `AGENTS.md`.
2. Read the relevant domain README.
3. Check whether the change affects shared protocol.
4. If protocol changes are needed, update `shared/protocol/WebSocketMessages.md` first.
5. Add or update JSON Schema files in `shared/schemas/` when applicable.
6. Implement the domain change in the relevant folder.
7. Add a short integration note.
8. Run local validation or tests.
9. Commit a focused change.

## Local Development Phases

### Phase 1: Contract and Skeletons

- Establish protocol messages.
- Add schema validation targets.
- Create local backend skeleton.
- Create Unreal connection skeleton.
- Define TA visual rules.
- Define AI event export assumptions.

### Phase 2: Local Vertical Slice

- Create a local session.
- Join from a mobile browser.
- Connect Unreal.
- Broadcast controller input.
- Render simple Unreal feedback.
- Log session events for AI analysis.

### Phase 3: Feature Expansion

- Add presentation commands.
- Add recreation modes.
- Add richer mobile controller layouts.
- Add TA-driven feedback effects.
- Add asynchronous AI recommendations.

### Phase 4: Cloud Readiness

- Add persistent session storage.
- Add deployment infrastructure.
- Add observability.
- Add authentication.
- Add rate limits.
- Add production QR Code and routing support.

## Branch and Commit Guidance

Use focused branches and commits by domain.

Examples:

- `backend/session-registry`
- `shared/controller-input-schema`
- `unreal/websocket-adapter`
- `ta/input-feedback-style`
- `ai/session-engagement-metric`

## Naming Rules

- Message types use lowercase dot-separated names: `controller.input`.
- JSON Schema files use PascalCase and end with `.schema.json`.
- Documentation files use PascalCase when they describe a major concept.
- Runtime IDs use short prefixes:
  - `ses_` for sessions
  - `room_` for rooms
  - `user_` for users
  - `msg_` for messages
  - `cmd_` for content commands
- Environment variables use uppercase snake case.
- Backend source names should follow the selected backend runtime convention.
- Unreal asset names must follow `docs/TA.md`.

## Integration Rules

- Shared protocol changes must be reviewed as cross-domain changes.
- Backend and Unreal must not define different field names for the same concept.
- AI must consume documented events and must not require private backend state.
- TA assets must include runtime trigger notes.
- Feature documentation must state how the feature is tested locally.

## Definition of Done for Cross-Domain Work

A cross-domain feature is done when:

- Shared protocol impact is documented.
- Schemas are updated when applicable.
- Backend behavior is described or implemented.
- Unreal consumption behavior is described or implemented.
- AI and TA impacts are documented when relevant.
- A local verification path exists.


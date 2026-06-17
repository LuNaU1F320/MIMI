# Codex Agent Rules for MIMI

This repository is architecture-first. Codex agents must preserve the shared protocol and domain boundaries while making implementation progress.

## Operating Principles

1. Treat `shared/` as the system contract.
2. Keep domain work inside the relevant domain folder unless a shared contract change is required.
3. Do not create private network message formats in `backend/`, `unreal/`, `ai/`, or `ta/`.
4. Prefer small, documented changes over broad speculative abstractions.
5. Keep local-first development working before adding cloud-only assumptions.
6. Do not add production dependencies without documenting why they are needed.

## Domain Boundaries

- Backend work belongs in `backend/`.
- Unreal Engine work belongs in `unreal/`.
- Artificial Intelligence work belongs in `ai/`.
- Technical Artist work belongs in `ta/`.
- Shared WebSocket protocol, schemas, and cross-domain contracts belong in `shared/`.
- Repository utilities belong in `tools/`.
- Deployment and operations assets belong in `infra/`.
- Architecture and workflow documentation belongs in `docs/`.

Agents must not modify unrelated domain folders during feature work unless the task explicitly requires cross-domain integration.

## Shared Protocol Rules

All network messages must be documented in `shared/protocol/WebSocketMessages.md`.

When a message has a stable structure, it must also have a JSON Schema in `shared/schemas/`.

Protocol changes must include:

- Message type name
- Direction
- Required fields
- Example payload
- Compatibility notes
- Expected consumers

Backend and Unreal Engine must use the same message type names and field names. If a field is inconvenient in one runtime, adapt at the edge of that runtime instead of changing the protocol privately.

## Integration Notes

Every new feature must include a short integration note in the relevant README, implementation note, or feature document.

The note must state:

- Which domains are affected
- Which protocol messages are used or changed
- How the feature can be tested locally
- Any assumptions about latency, ordering, or failure behavior

## Documentation Standards

Write documentation in clear English.

Use practical language:

- Define what the system does.
- Define who owns it.
- Define how it integrates.
- Define what is intentionally out of scope.

Avoid vague promises and undocumented magic. If a decision is made, record the reason.

## Implementation Standards

- Validate inbound WebSocket messages against schemas whenever practical.
- Keep AI processing asynchronous to the live input loop.
- Keep Unreal runtime adapters tolerant of unknown future message fields.
- Prefer deterministic local development workflows.
- Keep generated files, caches, and build outputs out of Git.
- Add tests or verification scripts when behavior can break integration.

## Commit Discipline

Commits should be small enough to review by domain.

Commit messages should identify the affected area when possible:

- `backend: add session registry`
- `shared: extend controller input schema`
- `unreal: add websocket adapter skeleton`
- `ai: add engagement metric prototype`
- `ta: document material naming rules`

Do not mix unrelated domain work in one commit unless the change is explicitly an integration change.


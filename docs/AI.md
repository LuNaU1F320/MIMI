# Artificial Intelligence

AI is an optional MIMI domain. It can improve session quality, but it must never block the real-time input loop.

## Responsibilities

- Session analysis
- Engagement estimation
- Content recommendation
- Recreation difficulty adjustment
- Presentation flow assistance
- Post-session summaries

## Design Rule

AI is advisory and asynchronous by default.

The backend and Unreal Engine must remain usable with AI disabled. If an AI feature fails, real-time controller input and content execution must continue.

## Initial Technical Direction

Recommended initial shape:

```text
ai/
  README.md
  notebooks/
  src/
    ingestion/
    metrics/
    recommendations/
  tests/
```

The first AI work should use recorded or exported session events instead of live blocking calls.

## Data Inputs

AI may consume:

- `session.state` snapshots
- `controller.input` event streams
- `user.joined` and `user.left` events
- Content command history
- Optional host annotations introduced later

## AI Outputs

AI may produce:

- Engagement scores
- Suggested content commands
- Suggested pacing changes
- Suggested recreation difficulty changes
- Post-event summaries

Any AI output that affects live content must be promoted through a documented backend path and shared protocol message.

## AI Rules

- Do not add AI-only assumptions to the shared real-time protocol.
- Do not make backend routing wait for model inference.
- Keep model experiments reproducible.
- Document input data shape and privacy assumptions.
- Keep prototype notebooks separate from reusable source code.

## First AI Milestone

1. Define a session event export format.
2. Build a simple engagement metric from user count, input frequency, and session duration.
3. Produce a non-blocking recommendation report.
4. Document how the backend could consume the recommendation later.


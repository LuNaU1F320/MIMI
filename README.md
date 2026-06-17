# Project Material Reference Driver
Link : https://drive.google.com/drive/folders/1TBHWYKSurPqa-FkY8F3ChzGslxr0llcC?ths=true

# MIMI

MIMI stands for **Multi-Input Media Immersion**.

MIMI is a real-time immersive content platform that connects cloud-ready backend services, local multi-input controllers, mobile browser controllers, Unreal Engine content, optional artificial intelligence systems, and technical art pipelines into one coordinated experience.

The project is designed for:

- Interactive presentations
- Recreation and event games
- Audience participation content
- Multi-controller media and content control
- Immersive visual feedback driven by live users

This repository intentionally starts as an architecture-first monorepo. Production implementation should be added only after the shared protocol, domain boundaries, and integration rules are understood.

## Core Features

- Room and session creation
- QR Code based mobile browser joining
- WebSocket session management
- Mobile controller input ingestion
- Unreal Engine client connection
- Multi-user input broadcast
- Session state synchronization
- Content commands for presentation and recreation modes
- Optional AI analysis and recommendation loops
- Technical art guidelines for interaction visuals and assets
- Local-first development with a path to cloud deployment

## Repository Structure

```text
MIMI/
  README.md
  AGENTS.md
  .gitignore
  docs/
  shared/
    protocol/
    schemas/
  backend/
  unreal/
  ai/
  ta/
  tools/
  infra/
```

### Top-Level Areas

- `shared/` is the integration center. Network message contracts live here and must be treated as shared product API.
- `backend/` owns sessions, rooms, QR join flows, WebSocket routing, state synchronization, and future cloud deployment adapters.
- `unreal/` owns Unreal Engine connection code, content runtime behavior, visual feedback integration, and Blueprint extension points.
- `ai/` owns optional offline or asynchronous intelligence features. AI must not block real-time input loops.
- `ta/` owns visual style guides, controller visualization direction, materials, motion graphics, effects, and asset naming standards.
- `tools/` owns repository-level helper scripts and developer utilities.
- `infra/` owns deployment, environment, and operations definitions when they are introduced.

## First Development Milestone

The first implementation milestone is a local end-to-end loop:

1. Backend can create a room and expose a QR/mobile join URL.
2. A mobile browser controller can connect over WebSocket.
3. An Unreal Engine client can connect to the same session.
4. `controller.input` messages from mobile clients are validated against shared schemas.
5. The backend broadcasts input and `session.state` updates to Unreal.
6. Unreal maps raw input into at least one presentation action and one recreation action.
7. A short integration note is added for every feature that touches more than one domain.

## Domain Collaboration Model

MIMI is split by ownership, not by isolation.

- Backend defines runtime session behavior using only shared protocol messages.
- Unreal consumes the same shared messages and must not invent private network formats.
- AI reads session data and produces advisory results asynchronously.
- Technical Art defines how interaction should look and feel, then packages assets so backend and Unreal teams can consume them predictably.

All domains coordinate through `shared/protocol/WebSocketMessages.md`, `shared/schemas/`, and the architecture documents in `docs/`.

## Required Reading

Before adding implementation code, read:

- `AGENTS.md`
- `docs/Architecture.md`
- `docs/Protocol.md`
- `shared/protocol/WebSocketMessages.md`
- The README for the domain you are modifying


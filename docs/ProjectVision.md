# Project Vision

MIMI, **Multi-Input Media Immersion**, is a platform for turning many audience inputs into one coordinated immersive media experience.

The product connects mobile browser controllers, local and cloud-ready backend services, Unreal Engine content, optional AI systems, and technical art pipelines. It is intended for real-time event environments where presenters, facilitators, or audiences control live digital content together.

## Product Intent

MIMI should make it practical to build experiences where many people influence a shared screen, stage, installation, or Unreal Engine world without installing native controller apps.

The first-class use cases are:

- Interactive presentation control
- Recreation and event games
- Audience participation experiences
- Multi-user content selection
- Live visual feedback from collective input
- Hybrid local and cloud deployment for venues

## Design Decisions

### Local-First, Cloud-Ready

The first implementation target is local development and local event operation. This keeps latency, debugging, and venue setup manageable. The backend should still be structured so the same concepts can later run in cloud infrastructure.

### WebSocket as the Real-Time Transport

WebSocket is the baseline transport because it is available in mobile browsers, backend runtimes, and Unreal Engine plugins or adapters. It is simple enough for local development and reliable enough for the first production milestone.

### Shared Protocol as the Center

The shared protocol is the center of the repository. Backend and Unreal Engine code must integrate through the same documented messages and schemas.

This prevents each domain from becoming a separate product with incompatible assumptions.

### AI Is Advisory

AI features are optional and asynchronous. AI can analyze sessions, estimate engagement, recommend content, or adjust difficulty, but it must not block controller input, session state updates, or Unreal Engine content execution.

### Technical Art Is an Integration Partner

Technical Art is not a late-stage polish folder. It defines how interaction is seen, felt, named, packaged, and reused across the platform.

## Success Criteria

MIMI is successful when:

- A room can be created locally.
- Multiple mobile users can join with a QR Code.
- Unreal Engine can receive user input and session state in real time.
- The same protocol documentation is used by every domain.
- Artists can create visual feedback assets without reverse-engineering engineering code.
- AI features can be added without slowing the real-time loop.

## Non-Goals for the Initial Repository

This initial repository does not include production backend code, Unreal Engine gameplay code, model training code, or final visual assets.

It establishes the structure and contracts that make those implementations possible.


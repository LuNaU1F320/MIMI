# Showdown Live Spring Backend

Spring Boot version of the Showdown Live backend.

It keeps the original mobile and host UI, replaces Socket.IO with a native Spring WebSocket endpoint at `/ws`, and keeps the Unreal HTTP Polling APIs.

## Requirements

- JDK 17+
- Maven 3.9+
- Node/npm only if you want Cloudflare Quick Tunnel through `npx cloudflared`

## Run with Tunnel

```powershell
cd C:\Users\Admin\Documents\midnight\work\showdown-spring
$env:ENABLE_TUNNEL='true'
mvn spring-boot:run
```

Or run:

```text
start-spring-tunnel.bat
```

## Run Local Only

```powershell
cd C:\Users\Admin\Documents\midnight\work\showdown-spring
$env:ENABLE_TUNNEL='false'
mvn spring-boot:run
```

Or run:

```text
start-spring-local.bat
```

## URLs

- Mobile: `http://localhost:3000`
- Host Dashboard: `http://localhost:3000/host.html`
- WebSocket: `ws://localhost:3000/ws`
- Unreal players: `GET /api/unreal/players`
- Unreal inputs: `GET /api/unreal/inputs`

When the tunnel is enabled, the host dashboard polls `/api/host-info` and updates the QR code to the public `https://*.trycloudflare.com` URL after Cloudflare assigns it.

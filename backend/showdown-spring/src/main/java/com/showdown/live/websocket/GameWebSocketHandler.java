package com.showdown.live.websocket;

import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.showdown.live.model.Player;
import com.showdown.live.model.WsEnvelope;
import com.showdown.live.service.GameService;
import jakarta.annotation.PostConstruct;
import jakarta.annotation.PreDestroy;
import java.io.File;
import java.io.IOException;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;
import org.springframework.web.socket.CloseStatus;
import org.springframework.web.socket.TextMessage;
import org.springframework.web.socket.WebSocketSession;
import org.springframework.web.socket.handler.TextWebSocketHandler;

@Component
public class GameWebSocketHandler extends TextWebSocketHandler {
  private final GameService gameService;
  private final ObjectMapper objectMapper;
  private final Map<String, WebSocketSession> sessions = new ConcurrentHashMap<>();
  private final Map<String, WebSocketSession> unrealSessions = new ConcurrentHashMap<>();
  private final Map<String, String> sessionPlayerIds = new ConcurrentHashMap<>();
  private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

  @Value("${showdown.tunnel.enabled:true}")
  private boolean tunnelEnabled;

  @Value("${server.port:3000}")
  private String port;

  private ScheduledFuture<?> previewLoop;
  private Process tunnelProcess;
  private volatile String tunnelUrl;
  private volatile long lastPositionBroadcastAt = 0;
  private static final long POSITION_BROADCAST_INTERVAL_MS = 200;
  private static final double MOBILE_MINIMAP_RANGE = 25.0;

  public GameWebSocketHandler(GameService gameService, ObjectMapper objectMapper) {
    this.gameService = gameService;
    this.objectMapper = objectMapper;
  }

  @PostConstruct
  public void startTunnelIfEnabled() {
    if (!tunnelEnabled) {
      System.out.println("Cloudflare Tunnel disabled. Set ENABLE_TUNNEL=true to enable public remote access.");
      return;
    }

    try {
      ProcessBuilder builder = new ProcessBuilder(
          npxCommand(),
          "--yes",
          "cloudflared@latest",
          "tunnel",
          "--url",
          "http://localhost:" + port
      );
      builder.redirectErrorStream(true);
      tunnelProcess = builder.start();
      Thread logThread = new Thread(this::readTunnelLogs, "cloudflared-log-reader");
      logThread.setDaemon(true);
      logThread.start();
      System.out.println("Establishing public internet tunnel via Cloudflare Tunnel (cloudflared)...");
    } catch (IOException ex) {
      System.out.println("[Cloudflare] Failed to start tunnel: " + ex.getMessage());
    }
  }

  private String npxCommand() {
    String nodeHome = System.getenv("NODE_HOME");
    if (nodeHome != null && !nodeHome.isBlank()) {
      File npx = new File(nodeHome, "npx.cmd");
      if (npx.exists()) return npx.getAbsolutePath();
    }

    File nvmNpx = new File("C:\\nvm4w\\nodejs\\npx.cmd");
    if (nvmNpx.exists()) return nvmNpx.getAbsolutePath();

    return System.getProperty("os.name", "").toLowerCase().contains("win") ? "npx.cmd" : "npx";
  }

  @PreDestroy
  public void shutdown() {
    stopPreviewLoop();
    scheduler.shutdownNow();
    if (tunnelProcess != null) {
      tunnelProcess.destroy();
    }
  }

  public String tunnelUrl() {
    return tunnelUrl;
  }

  @Override
  public void afterConnectionEstablished(WebSocketSession session) throws Exception {
    if (isUnrealSession(session)) {
      unrealSessions.put(session.getId(), session);
      sendUnreal(session, unrealStatePayload());
      return;
    }

    sessions.put(session.getId(), session);
    send(session, "gameStateChanged", snapshotPayload(), null);
  }

  @Override
  protected void handleTextMessage(WebSocketSession session, TextMessage message) throws Exception {
    if (isUnrealSession(session)) {
      handleUnrealMessage(message);
      return;
    }

    WsEnvelope envelope = objectMapper.readValue(message.getPayload(), WsEnvelope.class);
    JsonNode payload = envelope.payload == null ? objectMapper.createObjectNode() : envelope.payload;

    switch (envelope.event) {
      case "join" -> handleJoin(session, payload, envelope.requestId);
      case "rejoin" -> handleRejoin(session, payload, envelope.requestId);
      case "moveInput" -> handleMoveInput(session, payload);
      case "buyItem" -> handleBuyItem(payload, envelope.requestId, session);
      case "voteEvent" -> handleVoteEvent(payload);
      case "cheer" -> handleCheer(payload);
      case "adminStartShop" -> handleAdminStartShop();
      case "adminStartGame" -> handleAdminStartGame();
      case "adminResetGame" -> handleAdminResetGame();
      case "adminAddBots" -> handleAdminAddBots(payload);
      case "unrealPlayerStateChanged" -> handleUnrealPlayerStateChanged(payload);
      default -> {
      }
    }
  }

  @Override
  public void afterConnectionClosed(WebSocketSession session, CloseStatus status) {
    unrealSessions.remove(session.getId());
    sessions.remove(session.getId());
    String playerId = sessionPlayerIds.remove(session.getId());
    if (playerId != null) {
      gameService.disconnect(playerId);
      broadcastState();
    }
  }

  public void broadcastState() {
    broadcast("gameStateChanged", snapshotPayload());
    broadcastUnreal(unrealStatePayload());
  }

  public void broadcastInput(Player player) {
    if (player == null) return;
    broadcast("inputsUpdated", Map.of(
        "participantId", player.participantId,
        "playerId", player.playerId,
        "moveX", player.moveX,
        "moveY", player.moveY,
        "jumpSeq", player.jumpSeq,
        "emoteSeq", player.emoteSeq,
        "timestamp", player.inputTimestamp,
        "state", player.state,
        "connected", player.connected,
        "isBot", player.isBot
    ));
    broadcastUnreal(Map.of(
        "type", "inputsUpdated",
        "inputs", List.of(inputPayload(player))
    ));
  }

  public void broadcastUnrealInputsSnapshot() {
    broadcastUnreal(Map.of(
        "type", "inputsUpdated",
        "inputs", gameService.unrealInputs()
    ));
  }

  public void broadcast(String event, Object payload) {
    for (WebSocketSession session : sessions.values()) {
      if (session.isOpen()) {
        try {
          send(session, event, payload, null);
        } catch (IOException ignored) {
        }
      }
    }
  }

  public synchronized void startPreviewLoop() {
    stopPreviewLoop();
    previewLoop = scheduler.scheduleAtFixedRate(() -> {
      gameService.tickPreviewPhysics();
      broadcastUnrealInputsSnapshot();
      broadcastPositionsNow();
    }, 200, 200, TimeUnit.MILLISECONDS);
  }

  public synchronized void stopPreviewLoop() {
    if (previewLoop != null) {
      previewLoop.cancel(false);
      previewLoop = null;
    }
  }

  private void handleJoin(WebSocketSession session, JsonNode payload, String requestId) throws IOException {
    String nickname = payload.path("nickname").asText("").trim();
    if (nickname.isEmpty()) {
      send(session, "ack", Map.of("success", false, "reason", "Nickname is required"), requestId);
      return;
    }

    Player player = gameService.createPlayer(nickname);
    sessionPlayerIds.put(session.getId(), player.playerId);
    broadcastState();
    send(session, "ack", Map.of(
        "success", true,
        "participantId", player.participantId,
        "playerId", player.playerId,
        "nickname", player.nickname,
        "state", player.state
    ), requestId);
  }

  private void handleRejoin(WebSocketSession session, JsonNode payload, String requestId) throws IOException {
    String playerId = idFromPayload(payload);
    Player player = gameService.rejoin(playerId);
    if (player == null) {
      send(session, "ack", Map.of("success", false, "reason", "Player session not found"), requestId);
      return;
    }

    sessionPlayerIds.put(session.getId(), player.playerId);
    send(session, "ack", Map.of(
        "success", true,
        "participantId", player.participantId,
        "playerId", player.playerId,
        "nickname", player.nickname,
        "state", player.state
    ), requestId);
  }

  private void handleMoveInput(WebSocketSession session, JsonNode payload) {
    String playerId = sessionPlayerIds.get(session.getId());
    if (playerId == null) return;
    Player player = gameService.updateInput(
        playerId,
        payload.path("moveX").asDouble(0),
        payload.path("moveY").asDouble(0),
        payload.has("jumpSeq") ? payload.path("jumpSeq").asLong() : null,
        payload.has("emoteSeq") ? payload.path("emoteSeq").asLong() : null
    );
    broadcastInput(player);
    gameService.tickPreviewPhysics();
    broadcastPositionsThrottled();
  }

  private void handleBuyItem(JsonNode payload, String requestId, WebSocketSession session) throws IOException {
    Map<String, Object> result = gameService.buyItem(
        idFromPayload(payload),
        payload.path("itemId").asText("")
    );
    send(session, "ack", result, requestId);
    broadcastState();
  }

  private void handleVoteEvent(JsonNode payload) {
    gameService.voteEvent(
        idFromPayload(payload),
        payload.path("eventType").asText("")
    );
    broadcastState();
  }

  private void handleCheer(JsonNode payload) {
    gameService.cheer(
        idFromPayload(payload),
        idFromPayload(payload, "targetId", "targetParticipantId")
    );
    broadcastState();
  }

  private void handleAdminStartShop() {
    if (gameService.startShop()) {
      broadcastState();
    }
  }

  private void handleAdminStartGame() {
    if (gameService.startGame()) {
      startPreviewLoop();
      broadcastState();
    }
  }

  private void handleAdminResetGame() {
    gameService.resetGame();
    stopPreviewLoop();
    broadcastState();
  }

  private void handleAdminAddBots(JsonNode payload) {
    int count = Math.max(1, payload.path("count").asInt(5));
    for (int i = 0; i < count; i++) {
      String suffix = Long.toString(System.nanoTime(), 36);
      gameService.createBot("[BOT] Bot" + i, "bot_" + suffix.substring(Math.max(0, suffix.length() - 5)));
    }
    broadcastState();
  }

  private void handleUnrealPlayerStateChanged(JsonNode payload) {
    String playerId = idFromPayload(payload);
    String state = payload.path("state").asText("");
    Player player = gameService.applyPlayerState(playerId, state);
    if (player != null && "Dead".equals(state)) {
      broadcast("playerDead", Map.of("participantId", player.participantId, "playerId", player.playerId, "nickname", player.nickname));
    }
    broadcastState();
  }

  private void handleUnrealMessage(TextMessage message) throws IOException {
    JsonNode root = objectMapper.readTree(message.getPayload());
    String type = root.path("type").asText("");

    switch (type) {
      case "worldState" -> handleUnrealWorldState(root);
      case "playerState" -> handleUnrealPlayerState(root);
      case "result" -> handleUnrealResult(root);
      default -> {
      }
    }
  }

  private void handleUnrealWorldState(JsonNode root) {
    JsonNode playersNode = root.path("players");
    if (!playersNode.isArray()) return;

    List<Map<String, Object>> updates = new java.util.ArrayList<>();
    for (JsonNode playerNode : playersNode) {
      Map<String, Object> update = new LinkedHashMap<>();
      update.put("playerId", idFromPayload(playerNode));
      if (playerNode.has("worldX")) update.put("worldX", playerNode.path("worldX").asDouble());
      if (playerNode.has("worldY")) update.put("worldY", playerNode.path("worldY").asDouble());
      if (playerNode.has("x")) update.put("x", playerNode.path("x").asDouble());
      if (playerNode.has("y")) update.put("y", playerNode.path("y").asDouble());
      if (playerNode.has("hp")) update.put("hp", playerNode.path("hp").asDouble());
      if (playerNode.has("maxHp")) update.put("maxHp", playerNode.path("maxHp").asDouble());
      if (playerNode.has("alive")) update.put("alive", playerNode.path("alive").asBoolean());
      if (playerNode.has("state")) update.put("state", playerNode.path("state").asText());
      updates.add(update);
    }

    int updated = gameService.updateUnrealPositions(updates);
    if (updated > 0) {
      broadcastPositionsThrottled();
      broadcastState();
    }
  }

  private void handleUnrealPlayerState(JsonNode root) {
    Player player = gameService.applyPlayerState(
        idFromPayload(root),
        root.path("state").asText("")
    );
    if (player != null && "Dead".equals(player.state)) {
      broadcast("playerDead", Map.of("participantId", player.participantId, "playerId", player.playerId, "nickname", player.nickname));
    }
    broadcastState();
  }

  private void handleUnrealResult(JsonNode root) {
    gameService.applyResult(idFromPayload(root, "winnerId", "winnerParticipantId"), null);
    stopPreviewLoop();
    broadcastState();
  }

  private Map<String, Object> snapshotPayload() {
    var snapshot = gameService.snapshot();
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("gameState", snapshot.gameState);
    payload.put("players", snapshot.players);
    payload.put("ranking", snapshot.ranking);
    payload.put("winner", snapshot.winner);
    payload.put("shopItems", gameService.shopItems());
    payload.put("voteCounts", gameService.voteCounts());
    payload.put("cheerCounts", gameService.cheerCounts());
    return payload;
  }

  private Map<String, Object> unrealStatePayload() {
    Map<String, Object> payload = snapshotPayload();
    payload.put("type", "gameStateChanged");
    return payload;
  }

  private Map<String, Object> inputPayload(Player player) {
    Map<String, Object> payload = new LinkedHashMap<>();
    payload.put("participantId", player.participantId);
    payload.put("playerId", player.playerId);
    payload.put("moveX", player.moveX);
    payload.put("moveY", player.moveY);
    payload.put("jumpSeq", player.jumpSeq);
    payload.put("emoteSeq", player.emoteSeq);
    payload.put("timestamp", player.inputTimestamp);
    payload.put("state", player.state);
    payload.put("connected", player.connected);
    payload.put("isBot", player.isBot);
    return payload;
  }

  public boolean broadcastPositionsThrottled() {
    long now = System.currentTimeMillis();
    if (now - lastPositionBroadcastAt < POSITION_BROADCAST_INTERVAL_MS) {
      return false;
    }

    lastPositionBroadcastAt = now;
    broadcastPositionsNow();
    return true;
  }

  public void broadcastPositionsNow() {
    for (WebSocketSession session : sessions.values()) {
      if (!session.isOpen()) continue;

      String playerId = sessionPlayerIds.get(session.getId());
      Object payload = playerId == null
          ? gameService.compactPositions()
          : gameService.compactPositionsNear(playerId, MOBILE_MINIMAP_RANGE);

      try {
        send(session, "positionsUpdated", payload, null);
      } catch (IOException ignored) {
      }
    }
  }

  private void send(WebSocketSession session, String event, Object payload, String requestId) throws IOException {
    Map<String, Object> envelope = new LinkedHashMap<>();
    envelope.put("event", event);
    envelope.put("payload", payload);
    if (requestId != null) envelope.put("requestId", requestId);
    session.sendMessage(new TextMessage(objectMapper.writeValueAsString(envelope)));
  }

  private boolean isUnrealSession(WebSocketSession session) {
    return session.getUri() != null && session.getUri().getPath().endsWith("/ws/unreal");
  }

  private void broadcastUnreal(Object payload) {
    for (WebSocketSession session : unrealSessions.values()) {
      if (!session.isOpen()) continue;
      try {
        sendUnreal(session, payload);
      } catch (IOException ignored) {
      }
    }
  }

  private void sendUnreal(WebSocketSession session, Object payload) throws IOException {
    session.sendMessage(new TextMessage(objectMapper.writeValueAsString(payload)));
  }

  private String idFromPayload(JsonNode payload) {
    return idFromPayload(payload, "playerId", "participantId");
  }

  private String idFromPayload(JsonNode payload, String stringField, String intField) {
    JsonNode stringValue = payload.path(stringField);
    if (!stringValue.isMissingNode() && !stringValue.asText("").isBlank()) {
      return stringValue.asText("");
    }
    JsonNode intValue = payload.path(intField);
    if (intValue.isNumber()) {
      return Integer.toString(intValue.asInt());
    }
    String text = intValue.asText("");
    return text.isBlank() ? "" : text;
  }

  private void readTunnelLogs() {
    try (var reader = new java.io.BufferedReader(new java.io.InputStreamReader(tunnelProcess.getInputStream()))) {
      String line;
      while ((line = reader.readLine()) != null) {
        java.util.regex.Matcher matcher = java.util.regex.Pattern
            .compile("https://[a-z0-9-]+\\.trycloudflare\\.com")
            .matcher(line);
        if (matcher.find()) {
          tunnelUrl = matcher.group();
          System.out.println("===================================================");
          System.out.println("[Cloudflare] PUBLIC REMOTE ACCESS ESTABLISHED!");
          System.out.println("- Public URL: " + tunnelUrl);
          System.out.println("- Host Dashboard: " + tunnelUrl + "/host.html");
          System.out.println("===================================================");
        }
      }
    } catch (IOException ignored) {
    }
  }
}

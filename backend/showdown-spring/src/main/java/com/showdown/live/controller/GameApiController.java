package com.showdown.live.controller;

import com.showdown.live.model.Player;
import com.showdown.live.service.GameService;
import com.showdown.live.websocket.GameWebSocketHandler;
import java.net.Inet4Address;
import java.net.NetworkInterface;
import java.util.Enumeration;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api")
public class GameApiController {
  private final GameService gameService;
  private final GameWebSocketHandler webSocketHandler;

  @Value("${server.port:3000}")
  private int port;

  public GameApiController(GameService gameService, GameWebSocketHandler webSocketHandler) {
    this.gameService = gameService;
    this.webSocketHandler = webSocketHandler;
  }

  @GetMapping("/host-info")
  public Map<String, Object> hostInfo() {
    Map<String, Object> response = new LinkedHashMap<>();
    response.put("localIp", localIpAddress());
    response.put("port", port);
    response.put("tunnelUrl", webSocketHandler.tunnelUrl());
    return response;
  }

  @GetMapping("/status")
  public Map<String, Object> status() {
    var snapshot = gameService.snapshot();
    Map<String, Object> response = new LinkedHashMap<>();
    response.put("gameState", snapshot.gameState);
    response.put("playerCount", snapshot.players.size());
    response.put("players", snapshot.players);
    response.put("ranking", snapshot.ranking);
    response.put("winner", snapshot.winner);
    response.put("shopItems", gameService.shopItems());
    response.put("voteCounts", gameService.voteCounts());
    response.put("cheerCounts", gameService.cheerCounts());
    return response;
  }

  @GetMapping("/debug/transport")
  public Map<String, Object> transportStatus() {
    Map<String, Object> response = new LinkedHashMap<>(webSocketHandler.transportStatus());
    response.put("gameState", gameService.gameState());
    response.put("serverTime", System.currentTimeMillis());
    return response;
  }

  @PostMapping("/join")
  public ResponseEntity<Map<String, Object>> join(@RequestBody Map<String, Object> body) {
    String nickname = String.valueOf(body.getOrDefault("nickname", "")).trim();
    if (nickname.isEmpty()) {
      return ResponseEntity.badRequest().body(Map.of("success", false, "reason", "Nickname is required"));
    }

    gameService.removePreviousDisconnectedPlayer(idFromBody(body, "previousPlayerId", "previousParticipantId"));
    Player player = gameService.createPlayer(nickname);
    webSocketHandler.broadcastState();
    return ResponseEntity.ok(Map.of(
        "success", true,
        "participantId", player.participantId,
        "playerId", player.playerId,
        "nickname", player.nickname,
        "state", player.state
    ));
  }

  @PostMapping("/input")
  public ResponseEntity<Map<String, Object>> input(@RequestBody Map<String, Object> body) {
    String playerId = idFromBody(body);
    Player player = gameService.getPlayer(playerId);
    if (player == null) {
      return ResponseEntity.status(404).body(Map.of("success", false, "reason", "Player not found"));
    }
    if (!gameService.gameState().equals("Playing")) {
      return ResponseEntity.badRequest().body(Map.of("success", false, "reason", "Game is not in Playing state"));
    }

    Player updated = gameService.updateInput(
        playerId,
        asDouble(body.get("moveX")),
        asDouble(body.get("moveY")),
        asLongOrNull(body.get("jumpSeq")),
        asLongOrNull(body.get("emoteSeq"))
    );
    if (updated == null) {
      return ResponseEntity.badRequest().body(Map.of("success", false, "reason", "Input was ignored"));
    }
    webSocketHandler.broadcastInput(updated);
    return ResponseEntity.ok(Map.of("success", true, "input", inputDto(updated)));
  }

  @GetMapping("/unreal/players")
  public Map<String, Object> unrealPlayers() {
    return Map.of(
        "gameState", gameService.gameState(),
        "players", gameService.unrealPlayers(),
        "shopItems", gameService.shopItems(),
        "voteCounts", gameService.voteCounts(),
        "cheerCounts", gameService.cheerCounts()
    );
  }

  @GetMapping("/shop")
  public Map<String, Object> shop() {
    return Map.of(
        "gameState", gameService.gameState(),
        "shopItems", gameService.shopItems()
    );
  }

  @PostMapping("/buy")
  public ResponseEntity<Map<String, Object>> buy(@RequestBody Map<String, Object> body) {
    Map<String, Object> result = gameService.buyItem(
        idFromBody(body),
        String.valueOf(body.getOrDefault("itemId", ""))
    );
    webSocketHandler.broadcastState();
    boolean success = Boolean.TRUE.equals(result.get("success"));
    return success ? ResponseEntity.ok(result) : ResponseEntity.badRequest().body(result);
  }

  @PostMapping("/vote")
  public ResponseEntity<Map<String, Object>> vote(@RequestBody Map<String, Object> body) {
    Map<String, Object> result = gameService.voteEvent(
        idFromBody(body),
        String.valueOf(body.getOrDefault("eventType", ""))
    );
    webSocketHandler.broadcastState();
    boolean success = Boolean.TRUE.equals(result.get("success"));
    return success ? ResponseEntity.ok(result) : ResponseEntity.badRequest().body(result);
  }

  @PostMapping("/cheer")
  public ResponseEntity<Map<String, Object>> cheer(@RequestBody Map<String, Object> body) {
    Map<String, Object> result = gameService.cheer(
        idFromBody(body),
        idFromBody(body, "targetId", "targetParticipantId")
    );
    webSocketHandler.broadcastState();
    boolean success = Boolean.TRUE.equals(result.get("success"));
    return success ? ResponseEntity.ok(result) : ResponseEntity.badRequest().body(result);
  }

  @GetMapping("/unreal/inputs")
  public Map<String, Object> unrealInputs() {
    return Map.of(
        "gameState", gameService.gameState(),
        "serverTime", System.currentTimeMillis(),
        "inputs", gameService.unrealInputs()
    );
  }

  @PostMapping("/unreal/player-state")
  public ResponseEntity<Map<String, Object>> playerState(@RequestBody Map<String, Object> body) {
    String playerId = idFromBody(body);
    String state = String.valueOf(body.getOrDefault("state", ""));
    Player player = gameService.applyPlayerState(playerId, state);
    if (player == null) {
      return ResponseEntity.status(404).body(Map.of("success", false, "reason", "Player not found"));
    }

    if ("Dead".equals(state)) {
      webSocketHandler.broadcast("playerDead", Map.of(
          "participantId", player.participantId,
          "playerId", player.playerId,
          "nickname", player.nickname
      ));
    }
    webSocketHandler.broadcastState();
    return ResponseEntity.ok(Map.of("success", true, "player", playerDto(player)));
  }

  @PostMapping("/unreal/positions")
  public Map<String, Object> unrealPositions(@RequestBody Map<String, Object> body) {
    @SuppressWarnings("unchecked")
    List<Map<String, Object>> positions = (List<Map<String, Object>>) body.get("positions");
    if (positions == null) {
      positions = parseCompactPositions(body.get("p"));
    }

    GameService.UnrealPositionUpdateResult result = gameService.updateUnrealPositions(positions);
    int updatedCount = result.updatedCount();
    boolean broadcasted = webSocketHandler.broadcastPositionsThrottled();
    for (Player player : result.newlyDeadPlayers()) {
      webSocketHandler.broadcast("playerDead", Map.of(
          "participantId", player.participantId,
          "playerId", player.playerId,
          "nickname", player.nickname
      ));
    }
    if (result.stateChanged()) {
      webSocketHandler.broadcastState();
    }

    return Map.of(
        "success", true,
        "updated", updatedCount,
        "broadcasted", broadcasted,
        "broadcastIntervalMs", 200
    );
  }

  @PostMapping("/unreal/result")
  public Map<String, Object> result(@RequestBody Map<String, Object> body) {
    String winnerId = idFromBody(body, "winnerId", "winnerParticipantId");
    @SuppressWarnings("unchecked")
    List<Map<String, Object>> rankingList = (List<Map<String, Object>>) body.get("rankingList");
    gameService.applyResult(winnerId, rankingList);
    webSocketHandler.broadcastState();
    return Map.of("success", true);
  }

  @PostMapping("/admin/start-game")
  public ResponseEntity<Map<String, Object>> startGame() {
    if (!gameService.startGame()) {
      return ResponseEntity.badRequest().body(Map.of("success", false, "reason", "Game must be in Lobby or Shop state"));
    }
    webSocketHandler.startPreviewLoop();
    webSocketHandler.broadcastState();
    return ResponseEntity.ok(Map.of("success", true, "gameState", gameService.gameState()));
  }

  @PostMapping("/admin/start-shop")
  public ResponseEntity<Map<String, Object>> startShop() {
    if (!gameService.startShop()) {
      return ResponseEntity.badRequest().body(Map.of("success", false, "reason", "Game must be in Lobby state"));
    }
    webSocketHandler.broadcastState();
    return ResponseEntity.ok(Map.of("success", true, "gameState", gameService.gameState(), "shopItems", gameService.shopItems()));
  }

  @PostMapping("/admin/reset")
  public Map<String, Object> reset() {
    gameService.resetGame();
    webSocketHandler.stopPreviewLoop();
    webSocketHandler.broadcastUnrealInputsSnapshot();
    webSocketHandler.broadcastUnrealReset("adminReset");
    webSocketHandler.broadcastPositionsNow();
    webSocketHandler.broadcastState();
    return Map.of("success", true, "gameState", gameService.gameState());
  }

  @PostMapping("/admin/add-bots")
  public Map<String, Object> addBots(@RequestBody Map<String, Object> body) {
    int count = Math.max(1, ((Number) body.getOrDefault("count", 5)).intValue());
    for (int i = 0; i < count; i++) {
      gameService.createRandomBot();
    }
    webSocketHandler.broadcastState();
    return Map.of("success", true);
  }

  private Map<String, Object> inputDto(Player player) {
    return Map.of(
        "playerId", player.playerId,
        "participantId", player.participantId,
        "moveX", player.moveX,
        "moveY", player.moveY,
        "jumpSeq", player.jumpSeq,
        "emoteSeq", player.emoteSeq,
        "timestamp", player.inputTimestamp,
        "state", player.state,
        "connected", player.connected
    );
  }

  private Map<String, Object> playerDto(Player player) {
    return Map.of(
        "playerId", player.playerId,
        "participantId", player.participantId,
        "nickname", player.nickname,
        "color", player.color,
        "state", player.state,
        "connected", player.connected,
        "joinedAt", player.joinedAt,
        "isBot", player.isBot,
        "gold", player.gold,
        "items", player.items
    );
  }

  private double asDouble(Object value) {
    if (value instanceof Number number) return number.doubleValue();
    try {
      return Double.parseDouble(String.valueOf(value));
    } catch (Exception ignored) {
      return 0;
    }
  }

  private Long asLongOrNull(Object value) {
    if (value == null) return null;
    if (value instanceof Number number) return number.longValue();
    try {
      return Long.parseLong(String.valueOf(value));
    } catch (Exception ignored) {
      return null;
    }
  }

  private List<Map<String, Object>> parseCompactPositions(Object compactValue) {
    if (!(compactValue instanceof List<?> compactRows)) {
      return List.of();
    }

    return compactRows.stream()
        .filter(List.class::isInstance)
        .map(List.class::cast)
        .filter(row -> row.size() >= 3)
        .map(row -> {
          Map<String, Object> position = new LinkedHashMap<>();
          position.put("playerId", row.get(0));
          position.put("x", row.get(1));
          position.put("y", row.get(2));
          if (row.size() >= 4) {
            Object alive = row.get(3);
            position.put("state", asDouble(alive) == 1 ? "Alive" : "Dead");
          }
          if (row.size() >= 5) {
            position.put("hp", row.get(4));
          }
          if (row.size() >= 6) {
            position.put("maxHp", row.get(5));
          }
          if (row.size() >= 8) {
            position.put("worldX", row.get(6));
            position.put("worldY", row.get(7));
          }
          return position;
        })
        .toList();
  }

  private String localIpAddress() {
    try {
      Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
      while (interfaces.hasMoreElements()) {
        NetworkInterface networkInterface = interfaces.nextElement();
        Enumeration<java.net.InetAddress> addresses = networkInterface.getInetAddresses();
        while (addresses.hasMoreElements()) {
          java.net.InetAddress address = addresses.nextElement();
          if (address instanceof Inet4Address && !address.isLoopbackAddress() && !address.isAnyLocalAddress()) {
            return address.getHostAddress();
          }
        }
      }
    } catch (Exception ignored) {
    }
    return "127.0.0.1";
  }

  private String idFromBody(Map<String, Object> body) {
    return idFromBody(body, "playerId", "participantId");
  }

  private String idFromBody(Map<String, Object> body, String stringField, String intField) {
    Object stringValue = body.get(stringField);
    if (stringValue != null && !String.valueOf(stringValue).isBlank()) {
      return String.valueOf(stringValue);
    }
    Object intValue = body.get(intField);
    return intValue == null ? "" : String.valueOf(intValue);
  }
}

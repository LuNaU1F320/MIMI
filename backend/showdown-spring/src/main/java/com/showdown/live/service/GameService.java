package com.showdown.live.service;

import com.showdown.live.model.GameSnapshot;
import com.showdown.live.model.Player;
import com.showdown.live.model.RankingEntry;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ThreadLocalRandom;
import java.util.function.Supplier;
import org.springframework.stereotype.Service;

@Service
public class GameService {
  private final PersistenceService persistenceService;
  private String gameState = "Lobby";
  private final Map<String, Player> players = new LinkedHashMap<>();
  private final List<RankingEntry> ranking = new ArrayList<>();
  private final Map<String, Map<String, Object>> shopItems = new LinkedHashMap<>();
  private final Map<String, Integer> voteCounts = new LinkedHashMap<>();
  private final Map<String, Integer> cheerCounts = new LinkedHashMap<>();
  private Player winner;
  private boolean resultSaved = false;
  private long lastUnrealPositionAt = 0;
  private long nextPlayerNumber = 1;
  private static final double UNREAL_MAP_HALF_SIZE = 3000.0;
  private static final String[] PLAYER_COLORS = {
      "#a8e6cf", "#a8d8ea", "#ffaaa6", "#ffd3b6", "#dcedc1", "#c7ceea",
      "#f6c1ff", "#f9f7a1", "#b5ead7", "#ffdac1", "#e2f0cb", "#b5b9ff"
  };

  public GameService(PersistenceService persistenceService) {
    this.persistenceService = persistenceService;
    resetShopItems();
    resetVoteAndCheerCounts();
  }

  public synchronized GameSnapshot snapshot() {
    return new GameSnapshot(gameState, new ArrayList<>(players.values()), new ArrayList<>(ranking), winner);
  }

  public synchronized String gameState() {
    return gameState;
  }

  public synchronized Player createPlayer(String nickname) {
    Player player = new Player();
    player.participantId = (int) nextPlayerNumber++;
    player.playerId = Integer.toString(player.participantId);
    player.nickname = nickname.trim().substring(0, Math.min(8, nickname.trim().length()));
    player.color = PLAYER_COLORS[(player.participantId - 1) % PLAYER_COLORS.length];
    player.state = (gameState.equals("Lobby") || gameState.equals("Shop")) ? "Joined" : "Spectator";
    player.connected = true;
    player.joinedAt = System.currentTimeMillis();
    player.inputTimestamp = player.joinedAt;
    player.posX = randomPosition();
    player.posY = randomPosition();
    players.put(player.playerId, player);
    persistQuietly("save player", () -> {
      persistenceService.savePlayer(player);
      return null;
    });
    return player;
  }

  public synchronized Player createBot(String nickname, String playerId) {
    Player player = createPlayer(nickname);
    player.isBot = true;
    persistQuietly("save bot player", () -> {
      persistenceService.savePlayer(player);
      return null;
    });
    return player;
  }

  public synchronized Player rejoin(String playerId) {
    Player player = players.get(playerId);
    if (player != null) {
      player.connected = true;
    }
    return player;
  }

  public synchronized Player getPlayer(String playerId) {
    return players.get(playerId);
  }

  public synchronized Player updateInput(String playerId, double moveX, double moveY) {
    return updateInput(playerId, moveX, moveY, null, null);
  }

  public synchronized Player updateInput(String playerId, double moveX, double moveY, Long jumpSeq, Long emoteSeq) {
    Player player = players.get(playerId);
    if (player == null || !gameState.equals("Playing")) {
      return player;
    }

    double normalizedX = clampAxis(moveX);
    double normalizedY = clampAxis(moveY);
    player.x = normalizedX;
    player.y = normalizedY;
    player.moveX = normalizedX;
    player.moveY = normalizedY;
    if (jumpSeq != null && jumpSeq > player.jumpSeq) {
      player.jumpSeq = jumpSeq;
    }
    if (emoteSeq != null && emoteSeq > player.emoteSeq) {
      player.emoteSeq = emoteSeq;
    }
    player.inputTimestamp = System.currentTimeMillis();
    return player;
  }

  public synchronized boolean startGame() {
    if (!gameState.equals("Lobby") && !gameState.equals("Shop")) return false;

    gameState = "Playing";
    ranking.clear();
    winner = null;
    resultSaved = false;

    for (Player player : players.values()) {
      if (player.state.equals("Joined") || player.state.equals("Ready")) {
        player.state = "Alive";
        applyItemEffects(player);
      } else {
        player.state = "Spectator";
      }
      resetInput(player);
      persistenceService.savePlayer(player);
    }
    return true;
  }

  public synchronized boolean startShop() {
    if (!gameState.equals("Lobby")) return false;
    gameState = "Shop";
    return true;
  }

  public synchronized void resetGame() {
    gameState = "Lobby";
    ranking.clear();
    winner = null;
    resultSaved = false;
    resetShopItems();
    resetVoteAndCheerCounts();

    List<String> botIds = players.values().stream()
        .filter(player -> player.isBot)
        .map(player -> player.playerId)
        .toList();
    botIds.forEach(players::remove);

    for (Player player : players.values()) {
      player.state = "Joined";
      player.connected = true;
      player.gold = 100;
      player.items.clear();
      player.vote = null;
      player.cheerTargetId = null;
      resetStats(player);
      resetInput(player);
      persistQuietly("save started player", () -> {
        persistenceService.savePlayer(player);
        return null;
      });
    }
  }

  public synchronized Player applyPlayerState(String playerId, String state) {
    Player player = players.get(playerId);
    if (player == null) return null;

    String oldState = player.state;
    player.state = state;
    if (!state.equals("Alive")) {
      resetInput(player);
    }

    if (state.equals("Dead") && !oldState.equals("Dead")) {
      ranking.add(new RankingEntry(playerId, player.nickname, System.currentTimeMillis()));
      checkGameEndingCondition();
    }

    persistQuietly("save player state", () -> {
      persistenceService.savePlayer(player);
      return null;
    });
    return player;
  }

  public synchronized void applyResult(String winnerId, List<Map<String, Object>> rankingList) {
    Player winningPlayer = players.get(winnerId);
    if (winningPlayer != null) {
      winningPlayer.state = "Winner";
      winner = winningPlayer;
    }

    if (rankingList != null) {
      ranking.clear();
      for (Map<String, Object> item : rankingList) {
        Object playerId = item.get("playerId");
        Player rankedPlayer = playerId == null ? null : players.get(String.valueOf(playerId));
        ranking.add(new RankingEntry(
            String.valueOf(playerId),
            rankedPlayer == null ? "" : rankedPlayer.nickname,
            System.currentTimeMillis()
        ));
      }
    }

    gameState = "Result";
    saveResultOnce();
  }

  public synchronized Map<String, Object> buyItem(String playerId, String itemId) {
    Player player = players.get(playerId);
    if (player == null) {
      return Map.of("success", false, "reason", "Player not found");
    }
    if (!gameState.equals("Shop") && !gameState.equals("Lobby")) {
      return Map.of("success", false, "reason", "Shop is closed");
    }
    if (!player.state.equals("Joined") && !player.state.equals("Ready")) {
      return Map.of("success", false, "reason", "Player cannot buy now");
    }
    if (!player.items.isEmpty()) {
      return Map.of("success", false, "reason", "Only one item can be equipped");
    }

    Map<String, Object> item = shopItems.get(itemId);
    if (item == null) {
      return Map.of("success", false, "reason", "Item not found");
    }

    int stock = ((Number) item.getOrDefault("stock", 0)).intValue();
    int price = ((Number) item.getOrDefault("price", 0)).intValue();
    if (stock <= 0) {
      return Map.of("success", false, "reason", "Sold out");
    }
    if (player.gold < price) {
      return Map.of("success", false, "reason", "Not enough gold");
    }

    player.gold -= price;
    player.items.add(itemId);
    item.put("stock", stock - 1);
    persistQuietly("save purchase", () -> {
      persistenceService.savePurchase(player.playerId, itemId, price);
      persistenceService.savePlayer(player);
      return null;
    });
    return Map.of("success", true, "player", player, "shopItems", shopItems());
  }

  public synchronized Map<String, Object> voteEvent(String playerId, String eventType) {
    Player player = players.get(playerId);
    if (player == null) {
      return Map.of("success", false, "reason", "Player not found");
    }
    if (!player.state.equals("Dead") && !player.state.equals("Spectator")) {
      return Map.of("success", false, "reason", "Player must be dead or spectator");
    }
    if (!voteCounts.containsKey(eventType)) {
      return Map.of("success", false, "reason", "Unknown event type");
    }

    player.vote = eventType;
    recalculateVoteCounts();
    persistQuietly("save vote", () -> {
      persistenceService.saveVote(player.playerId, eventType);
      persistenceService.savePlayer(player);
      return null;
    });
    return Map.of("success", true, "voteCounts", voteCounts());
  }

  public synchronized Map<String, Object> cheer(String playerId, String targetId) {
    Player player = players.get(playerId);
    Player target = players.get(targetId);
    if (player == null || target == null) {
      return Map.of("success", false, "reason", "Player not found");
    }
    if (!player.state.equals("Dead") && !player.state.equals("Spectator")) {
      return Map.of("success", false, "reason", "Player must be dead or spectator");
    }

    player.cheerTargetId = targetId;
    recalculateCheerCounts();
    persistQuietly("save cheer", () -> {
      persistenceService.saveCheer(player.playerId, targetId);
      persistenceService.savePlayer(player);
      return null;
    });
    return Map.of("success", true, "cheerCounts", cheerCounts());
  }

  public synchronized void disconnect(String playerId) {
    Player player = players.get(playerId);
    if (player == null) return;

    player.connected = false;
    resetInput(player);
    if (gameState.equals("Lobby")) {
      players.remove(playerId);
    }
  }

  public synchronized List<Map<String, Object>> unrealPlayers() {
    return players.values().stream()
        .map(player -> {
          Map<String, Object> dto = new LinkedHashMap<>();
          dto.put("participantId", player.participantId);
          dto.put("playerId", player.playerId);
          dto.put("nickname", player.nickname);
          dto.put("color", player.color);
          dto.put("state", player.state);
          dto.put("connected", player.connected);
          dto.put("joinedAt", player.joinedAt);
          dto.put("isBot", player.isBot);
          dto.put("hp", player.hp);
          dto.put("maxHp", player.maxHp);
          dto.put("attackPower", player.attackPower);
          dto.put("speedMultiplier", player.speedMultiplier);
          dto.put("rangeMultiplier", player.rangeMultiplier);
          dto.put("shield", player.shield);
          dto.put("gold", player.gold);
          dto.put("items", new ArrayList<>(player.items));
          return dto;
        })
        .toList();
  }

  public synchronized List<Map<String, Object>> shopItems() {
    return shopItems.values().stream()
        .map(LinkedHashMap::new)
        .map(item -> (Map<String, Object>) item)
        .toList();
  }

  public synchronized Map<String, Integer> voteCounts() {
    return new LinkedHashMap<>(voteCounts);
  }

  public synchronized Map<String, Integer> cheerCounts() {
    return new LinkedHashMap<>(cheerCounts);
  }

  public synchronized List<Map<String, Object>> unrealInputs() {
    return players.values().stream()
        .filter(player -> List.of("Alive", "Dead", "Spectator").contains(player.state))
        .map(player -> {
          Map<String, Object> dto = new LinkedHashMap<>();
          dto.put("participantId", player.participantId);
          dto.put("playerId", player.playerId);
          dto.put("moveX", player.moveX);
          dto.put("moveY", player.moveY);
          dto.put("jumpSeq", player.jumpSeq);
          dto.put("emoteSeq", player.emoteSeq);
          dto.put("timestamp", player.inputTimestamp);
          dto.put("state", player.state);
          dto.put("connected", player.connected);
          return dto;
        })
        .toList();
  }

  public synchronized Map<String, List<Double>> compactPositions() {
    Map<String, List<Double>> positions = new LinkedHashMap<>();
    for (Player player : players.values()) {
      positions.put(player.playerId, List.of(
          Math.round(player.posX * 10.0) / 10.0,
          Math.round(player.posY * 10.0) / 10.0,
          player.state.equals("Alive") ? 1.0 : 0.0,
          Math.round(player.hp * 10.0) / 10.0,
          Math.round(player.maxHp * 10.0) / 10.0
      ));
    }
    return positions;
  }

  public synchronized Map<String, List<Double>> compactPositionsNear(String playerId, double range) {
    Player center = players.get(playerId);
    if (center == null) {
      return compactPositions();
    }

    double maxDistanceSquared = range * range;
    Map<String, List<Double>> positions = new LinkedHashMap<>();
    for (Player player : players.values()) {
      double dx = player.posX - center.posX;
      double dy = player.posY - center.posY;
      if (player.playerId.equals(playerId) || (dx * dx + dy * dy <= maxDistanceSquared)) {
        positions.put(player.playerId, List.of(
            Math.round(player.posX * 10.0) / 10.0,
            Math.round(player.posY * 10.0) / 10.0,
            player.state.equals("Alive") ? 1.0 : 0.0,
            Math.round(player.hp * 10.0) / 10.0,
            Math.round(player.maxHp * 10.0) / 10.0
        ));
      }
    }

    return positions;
  }

  public synchronized int updateUnrealPositions(List<Map<String, Object>> positionUpdates) {
    if (positionUpdates == null) return 0;

    int updatedCount = 0;
    for (Map<String, Object> update : positionUpdates) {
      Object playerIdValue = update.get("playerId");
      if (playerIdValue == null) continue;

      Player player = players.get(String.valueOf(playerIdValue));
      if (player == null) continue;

      Object worldXValue = update.get("worldX");
      Object worldYValue = update.get("worldY");
      if (worldXValue != null && worldYValue != null) {
        player.worldX = asDouble(worldXValue, player.worldX);
        player.worldY = asDouble(worldYValue, player.worldY);
        Map<String, Double> percentPosition = worldToPercent(player.worldX, player.worldY);
        player.posX = percentPosition.get("x");
        player.posY = percentPosition.get("y");
      } else {
        player.posX = clampPercent(asDouble(update.get("x"), player.posX));
        player.posY = clampPercent(asDouble(update.get("y"), player.posY));
      }

      player.hp = Math.max(0, asDouble(update.get("hp"), player.hp));
      player.maxHp = Math.max(1, asDouble(update.get("maxHp"), player.maxHp));

      Object aliveValue = update.get("alive");
      if (aliveValue instanceof Boolean alive && !alive && player.state.equals("Alive")) {
        applyPlayerState(player.playerId, "Dead");
      }

      Object stateValue = update.get("state");
      if (stateValue != null && !String.valueOf(stateValue).isBlank()) {
        applyPlayerState(player.playerId, String.valueOf(stateValue));
      }

      updatedCount += 1;
    }

    if (updatedCount > 0) {
      lastUnrealPositionAt = System.currentTimeMillis();
    }

    return updatedCount;
  }

  public synchronized void tickPreviewPhysics() {
    if (!gameState.equals("Playing")) return;
    if (System.currentTimeMillis() - lastUnrealPositionAt < 1000) return;

    for (Player player : players.values()) {
      if (!player.state.equals("Alive")) continue;

      if (player.isBot && ThreadLocalRandom.current().nextDouble() < 0.05) {
        double angle = ThreadLocalRandom.current().nextDouble() * Math.PI * 2;
        boolean isMoving = ThreadLocalRandom.current().nextDouble() < 0.7;
        updateInput(player.playerId, isMoving ? Math.cos(angle) : 0, isMoving ? Math.sin(angle) : 0);
      }

      if (player.moveX != 0 || player.moveY != 0) {
        player.posX = Math.max(2, Math.min(98, player.posX + player.moveX * 0.8));
        player.posY = Math.max(2, Math.min(98, player.posY - player.moveY * 0.8));
      }
    }
  }

  private void checkGameEndingCondition() {
    if (!gameState.equals("Playing")) return;

    List<Player> alivePlayers = players.values().stream()
        .filter(player -> player.state.equals("Alive"))
        .toList();

    if (alivePlayers.size() == 1) {
      winner = alivePlayers.get(0);
      winner.state = "Winner";
      gameState = "Result";
      saveResultOnce();
    } else if (alivePlayers.isEmpty()) {
      gameState = "Result";
      saveResultOnce();
    }
  }

  private void saveResultOnce() {
    if (resultSaved) return;
    resultSaved = true;
    persistQuietly("save result", () -> {
      persistenceService.saveResult(winner, new ArrayList<>(ranking), players.size());
      return null;
    });
  }

  private void resetInput(Player player) {
    player.x = 0;
    player.y = 0;
    player.moveX = 0;
    player.moveY = 0;
    player.jumpSeq = 0;
    player.emoteSeq = 0;
    player.inputTimestamp = System.currentTimeMillis();
  }

  private void resetStats(Player player) {
    player.maxHp = 100;
    player.hp = 100;
    player.attackPower = 1.0;
    player.speedMultiplier = 1.0;
    player.rangeMultiplier = 1.0;
    player.shield = 0;
  }

  private void applyItemEffects(Player player) {
    resetStats(player);
    for (String itemId : player.items) {
      switch (itemId) {
        case "atk_boost" -> player.attackPower *= 1.2;
        case "speed_boost" -> player.speedMultiplier *= 1.2;
        case "range_boost" -> player.rangeMultiplier *= 1.3;
        case "shield" -> player.shield += 50;
        default -> {
        }
      }
    }
  }

  private void resetShopItems() {
    shopItems.clear();
    shopItems.put("atk_boost", item("atk_boost", "공격 강화", 40, 10, "AttackPower", 1.2));
    shopItems.put("speed_boost", item("speed_boost", "이동속도 증가", 35, 10, "Speed", 1.2));
    shopItems.put("range_boost", item("range_boost", "공격범위 증가", 45, 5, "Range", 1.3));
    shopItems.put("shield", item("shield", "시작 보호막", 50, 5, "Shield", 50));
  }

  private Map<String, Object> item(String itemId, String name, int price, int stock, String effectType, Object effectValue) {
    Map<String, Object> effect = new LinkedHashMap<>();
    effect.put("type", effectType);
    effect.put("value", effectValue);

    Map<String, Object> item = new LinkedHashMap<>();
    item.put("itemId", itemId);
    item.put("name", name);
    item.put("price", price);
    item.put("stock", stock);
    item.put("effect", effect);
    return item;
  }

  private void resetVoteAndCheerCounts() {
    voteCounts.clear();
    voteCounts.put("HealZone", 0);
    voteCounts.put("SpeedUp", 0);
    voteCounts.put("ShrinkZone", 0);
    voteCounts.put("SupplyBox", 0);
    cheerCounts.clear();
  }

  private void recalculateVoteCounts() {
    voteCounts.replaceAll((key, value) -> 0);
    for (Player player : players.values()) {
      if ((player.state.equals("Dead") || player.state.equals("Spectator")) && player.vote != null && voteCounts.containsKey(player.vote)) {
        voteCounts.put(player.vote, voteCounts.get(player.vote) + 1);
      }
    }
  }

  private void recalculateCheerCounts() {
    cheerCounts.clear();
    for (Player player : players.values()) {
      if ((player.state.equals("Dead") || player.state.equals("Spectator")) && player.cheerTargetId != null) {
        cheerCounts.put(player.cheerTargetId, cheerCounts.getOrDefault(player.cheerTargetId, 0) + 1);
      }
    }
  }

  private double clampAxis(double value) {
    if (!Double.isFinite(value)) return 0;
    return Math.max(-1, Math.min(1, value));
  }

  private double clampPercent(double value) {
    if (!Double.isFinite(value)) return 50;
    return Math.max(0, Math.min(100, value));
  }

  private Map<String, Double> worldToPercent(double worldX, double worldY) {
    return Map.of(
        "x", clampPercent(((worldY + UNREAL_MAP_HALF_SIZE) / (UNREAL_MAP_HALF_SIZE * 2)) * 100),
        "y", clampPercent(100 - (((worldX + UNREAL_MAP_HALF_SIZE) / (UNREAL_MAP_HALF_SIZE * 2)) * 100))
    );
  }

  private double asDouble(Object value, double fallback) {
    if (value instanceof Number number) return number.doubleValue();
    try {
      return Double.parseDouble(String.valueOf(value));
    } catch (Exception ignored) {
      return fallback;
    }
  }

  private double randomPosition() {
    return 15 + ThreadLocalRandom.current().nextDouble() * 70;
  }

  private <T> T persistQuietly(String action, Supplier<T> operation) {
    try {
      return operation.get();
    } catch (RuntimeException ex) {
      System.out.println("[Persistence] Failed to " + action + ": " + ex.getMessage());
      return null;
    }
  }

  public List<RankingEntry> emptyRanking() {
    return Collections.emptyList();
  }
}

package com.showdown.live.service;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.showdown.live.entity.CheerRecord;
import com.showdown.live.entity.GameResultRecord;
import com.showdown.live.entity.PlayerRecord;
import com.showdown.live.entity.PurchaseRecord;
import com.showdown.live.entity.VoteRecord;
import com.showdown.live.model.Player;
import com.showdown.live.model.RankingEntry;
import com.showdown.live.repository.CheerRecordRepository;
import com.showdown.live.repository.GameResultRecordRepository;
import com.showdown.live.repository.PlayerRecordRepository;
import com.showdown.live.repository.PurchaseRecordRepository;
import com.showdown.live.repository.VoteRecordRepository;
import java.util.ArrayList;
import java.util.List;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class PersistenceService {
  private final PlayerRecordRepository playerRecords;
  private final PurchaseRecordRepository purchaseRecords;
  private final VoteRecordRepository voteRecords;
  private final CheerRecordRepository cheerRecords;
  private final GameResultRecordRepository gameResultRecords;
  private final ObjectMapper objectMapper;

  public PersistenceService(
      PlayerRecordRepository playerRecords,
      PurchaseRecordRepository purchaseRecords,
      VoteRecordRepository voteRecords,
      CheerRecordRepository cheerRecords,
      GameResultRecordRepository gameResultRecords,
      ObjectMapper objectMapper
  ) {
    this.playerRecords = playerRecords;
    this.purchaseRecords = purchaseRecords;
    this.voteRecords = voteRecords;
    this.cheerRecords = cheerRecords;
    this.gameResultRecords = gameResultRecords;
    this.objectMapper = objectMapper;
  }

  @Transactional
  public void savePlayer(Player player) {
    if (player == null || player.playerId == null) return;

    PlayerRecord record = playerRecords.findById(player.playerId).orElseGet(PlayerRecord::new);
    record.playerId = player.playerId;
    record.participantId = player.participantId;
    record.nickname = player.nickname;
    record.color = player.color;
    record.state = player.state;
    record.gold = player.gold;
    record.bot = player.isBot;
    record.joinedAt = player.joinedAt;
    record.updatedAt = System.currentTimeMillis();
    record.attackPower = player.attackPower;
    record.speedMultiplier = player.speedMultiplier;
    record.rangeMultiplier = player.rangeMultiplier;
    record.shield = player.shield;
    record.items = new ArrayList<>(player.items);
    playerRecords.save(record);
  }

  @Transactional
  public void savePurchase(String playerId, String itemId, int price) {
    PurchaseRecord record = new PurchaseRecord();
    record.playerId = playerId;
    record.itemId = itemId;
    record.price = price;
    record.purchasedAt = System.currentTimeMillis();
    purchaseRecords.save(record);
  }

  @Transactional
  public void saveVote(String playerId, String eventType) {
    VoteRecord record = new VoteRecord();
    record.playerId = playerId;
    record.eventType = eventType;
    record.votedAt = System.currentTimeMillis();
    voteRecords.save(record);
  }

  @Transactional
  public void saveCheer(String playerId, String targetPlayerId) {
    CheerRecord record = new CheerRecord();
    record.playerId = playerId;
    record.targetPlayerId = targetPlayerId;
    record.cheeredAt = System.currentTimeMillis();
    cheerRecords.save(record);
  }

  @Transactional
  public void saveResult(Player winner, List<RankingEntry> ranking, int playerCount) {
    GameResultRecord record = new GameResultRecord();
    record.winnerId = winner == null ? null : winner.playerId;
    record.winnerNickname = winner == null ? null : winner.nickname;
    record.playerCount = playerCount;
    record.recordedAt = System.currentTimeMillis();
    try {
      record.rankingJson = objectMapper.writeValueAsString(ranking);
    } catch (JsonProcessingException ex) {
      record.rankingJson = "[]";
    }
    gameResultRecords.save(record);
  }
}

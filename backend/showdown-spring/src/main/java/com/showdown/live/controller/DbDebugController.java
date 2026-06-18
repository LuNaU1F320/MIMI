package com.showdown.live.controller;

import com.showdown.live.repository.CheerRecordRepository;
import com.showdown.live.repository.GameResultRecordRepository;
import com.showdown.live.repository.PlayerRecordRepository;
import com.showdown.live.repository.PurchaseRecordRepository;
import com.showdown.live.repository.VoteRecordRepository;
import java.util.Map;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/admin")
public class DbDebugController {
  private final PlayerRecordRepository playerRecords;
  private final PurchaseRecordRepository purchaseRecords;
  private final VoteRecordRepository voteRecords;
  private final CheerRecordRepository cheerRecords;
  private final GameResultRecordRepository gameResultRecords;

  public DbDebugController(
      PlayerRecordRepository playerRecords,
      PurchaseRecordRepository purchaseRecords,
      VoteRecordRepository voteRecords,
      CheerRecordRepository cheerRecords,
      GameResultRecordRepository gameResultRecords
  ) {
    this.playerRecords = playerRecords;
    this.purchaseRecords = purchaseRecords;
    this.voteRecords = voteRecords;
    this.cheerRecords = cheerRecords;
    this.gameResultRecords = gameResultRecords;
  }

  @GetMapping("/db-summary")
  public Map<String, Object> dbSummary() {
    return Map.of(
        "players", playerRecords.count(),
        "purchases", purchaseRecords.count(),
        "votes", voteRecords.count(),
        "cheers", cheerRecords.count(),
        "results", gameResultRecords.count(),
        "h2Console", "/h2-console",
        "jdbcUrl", "jdbc:h2:file:./data/showdown"
    );
  }
}

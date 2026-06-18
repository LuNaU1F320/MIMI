package com.showdown.live.model;

public class RankingEntry {
  public String playerId;
  public String nickname;
  public long timeOfDeath;

  public RankingEntry() {
  }

  public RankingEntry(String playerId, String nickname, long timeOfDeath) {
    this.playerId = playerId;
    this.nickname = nickname;
    this.timeOfDeath = timeOfDeath;
  }
}

package com.showdown.live.model;

import java.util.List;

public class GameSnapshot {
  public String gameState;
  public List<Player> players;
  public List<RankingEntry> ranking;
  public Player winner;

  public GameSnapshot(String gameState, List<Player> players, List<RankingEntry> ranking, Player winner) {
    this.gameState = gameState;
    this.players = players;
    this.ranking = ranking;
    this.winner = winner;
  }
}

package com.showdown.live.entity;

import jakarta.persistence.Entity;
import jakarta.persistence.GeneratedValue;
import jakarta.persistence.GenerationType;
import jakarta.persistence.Id;
import jakarta.persistence.Lob;

@Entity
public class GameResultRecord {
  @Id
  @GeneratedValue(strategy = GenerationType.IDENTITY)
  public Long id;
  public String winnerId;
  public String winnerNickname;
  public int playerCount;
  public long recordedAt;

  @Lob
  public String rankingJson;
}

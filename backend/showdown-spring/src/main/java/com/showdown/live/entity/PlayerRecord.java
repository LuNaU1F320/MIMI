package com.showdown.live.entity;

import jakarta.persistence.ElementCollection;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import java.util.ArrayList;
import java.util.List;

@Entity
public class PlayerRecord {
  @Id
  public String playerId;
  public int participantId;
  public String nickname;
  public String color;
  public String state;
  public int gold;
  public boolean bot;
  public long joinedAt;
  public long updatedAt;
  public double attackPower;
  public double speedMultiplier;
  public double rangeMultiplier;
  public double shield;

  @ElementCollection
  public List<String> items = new ArrayList<>();
}

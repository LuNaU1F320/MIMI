package com.showdown.live.model;

import java.util.ArrayList;
import java.util.List;

public class Player {
  public int participantId;
  public String playerId;
  public String nickname;
  public String color;
  public String state;
  public int gold = 100;
  public List<String> items = new ArrayList<>();
  public String vote;
  public String cheerTargetId;
  public double x;
  public double y;
  public double moveX;
  public double moveY;
  public long jumpSeq;
  public long emoteSeq;
  public double posX;
  public double posY;
  public double worldX;
  public double worldY;
  public double hp = 100;
  public double maxHp = 100;
  public double attackPower = 1.0;
  public double speedMultiplier = 1.0;
  public double rangeMultiplier = 1.0;
  public double shield = 0;
  public boolean connected;
  public long joinedAt;
  public long inputTimestamp;
  public boolean isBot;
}

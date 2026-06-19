package com.showdown.live.model;

import com.fasterxml.jackson.databind.JsonNode;

public class WsEnvelope {
  public String event;
  public String requestId;
  public JsonNode payload;
}

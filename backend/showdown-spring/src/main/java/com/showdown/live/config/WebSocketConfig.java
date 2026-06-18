package com.showdown.live.config;

import com.showdown.live.websocket.GameWebSocketHandler;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;

@Configuration
@EnableWebSocket
public class WebSocketConfig implements WebSocketConfigurer {
  private final GameWebSocketHandler gameWebSocketHandler;

  public WebSocketConfig(GameWebSocketHandler gameWebSocketHandler) {
    this.gameWebSocketHandler = gameWebSocketHandler;
  }

  @Override
  public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
    registry.addHandler(gameWebSocketHandler, "/ws").setAllowedOrigins("*");
    registry.addHandler(gameWebSocketHandler, "/ws/unreal").setAllowedOrigins("*");
  }
}

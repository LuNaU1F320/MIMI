package com.showdown.live.config;

import com.showdown.live.websocket.GameWebSocketHandler;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.socket.config.annotation.EnableWebSocket;
import org.springframework.web.socket.config.annotation.WebSocketConfigurer;
import org.springframework.web.socket.config.annotation.WebSocketHandlerRegistry;
import org.springframework.web.socket.server.standard.ServletServerContainerFactoryBean;

@Configuration
@EnableWebSocket
public class WebSocketConfig implements WebSocketConfigurer {
  private static final int WEBSOCKET_BUFFER_SIZE_BYTES = 2 * 1024 * 1024;
  private final GameWebSocketHandler gameWebSocketHandler;

  public WebSocketConfig(GameWebSocketHandler gameWebSocketHandler) {
    this.gameWebSocketHandler = gameWebSocketHandler;
  }

  @Override
  public void registerWebSocketHandlers(WebSocketHandlerRegistry registry) {
    registry.addHandler(gameWebSocketHandler, "/ws").setAllowedOrigins("*");
    registry.addHandler(gameWebSocketHandler, "/ws/unreal").setAllowedOrigins("*");
  }

  @Bean
  public ServletServerContainerFactoryBean webSocketContainer() {
    ServletServerContainerFactoryBean container = new ServletServerContainerFactoryBean();
    container.setMaxTextMessageBufferSize(WEBSOCKET_BUFFER_SIZE_BYTES);
    container.setMaxBinaryMessageBufferSize(WEBSOCKET_BUFFER_SIZE_BYTES);
    return container;
  }
}

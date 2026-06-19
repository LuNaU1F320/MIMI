package com.showdown.live.config;

import org.springframework.boot.ApplicationArguments;
import org.springframework.boot.ApplicationRunner;
import org.springframework.jdbc.core.JdbcTemplate;
import org.springframework.stereotype.Component;

@Component
public class H2SchemaMigration implements ApplicationRunner {
  private final JdbcTemplate jdbcTemplate;

  public H2SchemaMigration(JdbcTemplate jdbcTemplate) {
    this.jdbcTemplate = jdbcTemplate;
  }

  @Override
  public void run(ApplicationArguments args) {
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS participant_id INT DEFAULT 0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS attack_power DOUBLE DEFAULT 1.0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS speed_multiplier DOUBLE DEFAULT 1.0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS range_multiplier DOUBLE DEFAULT 1.0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS shield DOUBLE DEFAULT 0.0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS bot BOOLEAN DEFAULT FALSE");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS color VARCHAR(32)");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS gold INT DEFAULT 100");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS joined_at BIGINT DEFAULT 0");
    alter("ALTER TABLE player_record ADD COLUMN IF NOT EXISTS updated_at BIGINT DEFAULT 0");
  }

  private void alter(String sql) {
    try {
      jdbcTemplate.execute(sql);
    } catch (RuntimeException ex) {
      System.out.println("[DB Migration] Skipped: " + sql + " reason=" + ex.getMessage());
    }
  }
}

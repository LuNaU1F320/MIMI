package com.showdown.live.repository;

import com.showdown.live.entity.PlayerRecord;
import org.springframework.data.jpa.repository.JpaRepository;

public interface PlayerRecordRepository extends JpaRepository<PlayerRecord, String> {
}

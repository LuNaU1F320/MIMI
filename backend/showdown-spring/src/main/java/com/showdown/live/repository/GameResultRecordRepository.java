package com.showdown.live.repository;

import com.showdown.live.entity.GameResultRecord;
import org.springframework.data.jpa.repository.JpaRepository;

public interface GameResultRecordRepository extends JpaRepository<GameResultRecord, Long> {
}

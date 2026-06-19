package com.showdown.live.repository;

import com.showdown.live.entity.CheerRecord;
import org.springframework.data.jpa.repository.JpaRepository;

public interface CheerRecordRepository extends JpaRepository<CheerRecord, Long> {
}

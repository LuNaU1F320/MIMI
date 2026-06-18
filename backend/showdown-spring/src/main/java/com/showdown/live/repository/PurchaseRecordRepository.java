package com.showdown.live.repository;

import com.showdown.live.entity.PurchaseRecord;
import org.springframework.data.jpa.repository.JpaRepository;

public interface PurchaseRecordRepository extends JpaRepository<PurchaseRecord, Long> {
}

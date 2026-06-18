package com.showdown.live.repository;

import com.showdown.live.entity.VoteRecord;
import org.springframework.data.jpa.repository.JpaRepository;

public interface VoteRecordRepository extends JpaRepository<VoteRecord, Long> {
}

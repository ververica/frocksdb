// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <algorithm>
#include <iostream>
#include "utilities/flink/flink_compaction_filter.h"

namespace rocksdb {
namespace flink {

const char* FlinkCompactionFilter::Name() const {
  return "FlinkCompactionFilter";
}

CompactionFilter::Decision FlinkCompactionFilter::FilterV2(
    int /*level*/, const Slice& key, ValueType value_type,
    const Slice& existing_value, std::string* new_value,
    std::string* /*skip_until*/) const {
  const Config config = config_holder_->GetConfig();
  CreateListElementIterIfNull(config.list_element_iter_factory_);

  const char* data = existing_value.data();

  Debug(logger_.get(),
    "Call FlinkCompactionFilter::FilterV2 - Key: %s, Data: %s, Value type: %d, "
    "State type: %d, TTL: %d ms, useSystemTime: %d, timestamp_offset: %d",
    key.ToString().c_str(), existing_value.ToString(true).c_str(), value_type,
    config.state_type_, config.ttl_, config.useSystemTime_, config.timestamp_offset_);

  // too short value to have timestamp at all
  const bool tooShortValue = existing_value.size() < config.timestamp_offset_ + TIMESTAMP_BYTE_SIZE;

  const StateType state_type = config.state_type_;
  const bool value_or_merge = value_type == ValueType::kValue || value_type == ValueType::kMergeOperand;
  const bool value_state = state_type == StateType::Value && value_type == ValueType::kValue;
  const bool list_entry = state_type == StateType::List && value_or_merge;
  const bool toDecide = value_state || list_entry;
  const bool list_iter = list_entry && list_element_iter_;

  Decision decision = Decision::kKeep;
  if (!tooShortValue && toDecide) {
    decision = list_iter ?
            ListDecide(existing_value, config, new_value) :
            Decide(data, config, config.timestamp_offset_);
  }
  Debug(logger_.get(), "Decision: %d", decision);
  return decision;
}

CompactionFilter::Decision FlinkCompactionFilter::ListDecide(
        const Slice& existing_value, const Config& config, std::string* new_value) const {
  list_element_iter_->SetListBytes(existing_value);
  std::size_t offset = 0;
  while (offset < existing_value.size()) {
    Decision decision = Decide(existing_value.data(), config, offset + config.timestamp_offset_);
    if (decision != Decision::kKeep) {
      offset = ListNextOffset(offset);
      if (offset >= JAVA_MAX_SIZE) {
        return Decision::kKeep;
      }
    } else {
      break;
    }
  }
  if (offset >= existing_value.size()) {
    return Decision::kRemove;
  } else if (offset > 0) {
    SetUnexpiredListValue(existing_value, offset, new_value);
    return Decision::kChangeValue;
  }
  return Decision::kKeep;
}

std::size_t FlinkCompactionFilter::ListNextOffset(std::size_t offset) const {
  std::size_t new_offset = list_element_iter_->NextOffset(offset);
  if (new_offset >= JAVA_MAX_SIZE || new_offset < offset) {
    Error(logger_.get(), "Wrong next offset in list iterator: %d -> %d",
          offset, new_offset);
    new_offset = JAVA_MAX_SIZE;
  } else {
    Debug(logger_.get(), "Next offset: %d -> %d",
          offset, new_offset);
  }
  return new_offset;
}

void FlinkCompactionFilter::SetUnexpiredListValue(
        const Slice& existing_value, std::size_t offset, std::string* new_value) const {
  new_value->clear();
  Slice new_value_slice = Slice(existing_value.data() + offset, existing_value.size() - offset);
  new_value->assign(new_value_slice.data(), new_value_slice.size());
  Debug(logger_.get(), "New list value: %s", new_value_slice.ToString(true).c_str());
}

CompactionFilter::Decision FlinkCompactionFilter::Decide(
    const char* ts_bytes, const Config& config, const std::size_t timestamp_offset) const {
  int64_t timestamp = DeserializeTimestamp(ts_bytes, timestamp_offset);
  const int64_t ttlWithoutOverflow = timestamp > 0 ? std::min(JAVA_MAX_LONG - timestamp, config.ttl_) : config.ttl_;
  const int64_t currentTimestamp = CurrentTimestamp(config.useSystemTime_);
  Debug(logger_.get(), "Last access timestamp: %ld ms, ttlWithoutOverflow: %ld ms, Current timestamp: %ld ms",
    timestamp, ttlWithoutOverflow, currentTimestamp);
  return timestamp + ttlWithoutOverflow <= currentTimestamp ? Decision::kRemove : Decision::kKeep;
}

int64_t FlinkCompactionFilter::DeserializeTimestamp(const char *src, std::size_t offset) const {
  uint64_t result = 0;
  for (unsigned long i = 0; i < sizeof(uint64_t); i++) {
    result |= static_cast<uint64_t>(static_cast<unsigned char>(src[offset + i]))
            << ((sizeof(int64_t) - 1 - i) * BITS_PER_BYTE);
  }
  return static_cast<int64_t>(result);
}

int64_t FlinkCompactionFilter::CurrentTimestamp(bool useSystemTime) const {
  using namespace std::chrono;
  int64_t current_timestamp;
  if (useSystemTime) {
    current_timestamp = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
  } else {
    current_timestamp = config_holder_->GetCurrentTimestamp();
  }
  return current_timestamp;
}
}  // namespace flink
}  // namespace rocksdb

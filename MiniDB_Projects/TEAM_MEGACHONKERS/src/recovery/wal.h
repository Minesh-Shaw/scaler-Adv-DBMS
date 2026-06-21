#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include "common/config.h"

namespace minidb {

// Defines the type of operation in the log
enum class LogRecordType : uint8_t {
    PUT = 0,
    DELETE_TOMBSTONE = 1
};

class WAL {
private:
    std::string log_file_path_;
    std::ofstream log_stream_;
    std::mutex mutex_; // Protects concurrent appends to the log file
    lsn_t current_lsn_{0}; // Log Sequence Number

public:
    explicit WAL(const std::string& file_path);
    ~WAL();

    // Appends a record and forces a disk flush. Returns the assigned LSN.
    lsn_t Append(LogRecordType type, const std::string& key, const std::string& value);
    
    // Deletes the log file (called after a MemTable is successfully flushed to an SSTable)
    void Clear();
};

} // namespace minidb
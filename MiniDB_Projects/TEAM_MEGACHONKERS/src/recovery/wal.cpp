#include "recovery/wal.h"
#include <filesystem>

namespace minidb {

WAL::WAL(const std::string& file_path) : log_file_path_(file_path) {
    // Open in binary append mode
    log_stream_.open(log_file_path_, std::ios::app | std::ios::binary);
}

WAL::~WAL() {
    if (log_stream_.is_open()) {
        log_stream_.close();
    }
}

lsn_t WAL::Append(LogRecordType type, const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    current_lsn_++;
    
    // Binary Layout of a Log Record:
    // [LSN: 8 bytes] | [Type: 1 byte] | [Key Size: 4 bytes] | [Key] | [Value Size: 4 bytes] | [Value]
    
    uint32_t key_size = key.size();
    uint32_t val_size = value.size();

    log_stream_.write(reinterpret_cast<const char*>(&current_lsn_), sizeof(current_lsn_));
    log_stream_.write(reinterpret_cast<const char*>(&type), sizeof(type));
    log_stream_.write(reinterpret_cast<const char*>(&key_size), sizeof(key_size));
    log_stream_.write(key.data(), key_size);
    log_stream_.write(reinterpret_cast<const char*>(&val_size), sizeof(val_size));
    
    if (val_size > 0) {
        log_stream_.write(value.data(), val_size);
    }

    // Force the OS to write the buffer to disk. 
    // In a production system, you would use fsync() for true durability.
    log_stream_.flush(); 
    
    return current_lsn_;
}

void WAL::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    log_stream_.close();
    
    // Delete the file and recreate it fresh
    std::filesystem::remove(log_file_path_);
    log_stream_.open(log_file_path_, std::ios::app | std::ios::binary);
    current_lsn_ = 0;
}

} // namespace minidb
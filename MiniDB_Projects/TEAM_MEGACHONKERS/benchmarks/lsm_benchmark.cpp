#include <iostream>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>

#include "catalog/catalog.h"
#include "concurrency/lock_manager.h"
#include "concurrency/transaction_manager.h"
#include "execution/executor_context.h"
#include "execution/values_executor.h"
#include "execution/insert_executor.h"
#include "execution/seq_scan_executor.h"
#include "storage/btree/bplus_tree.h"

using namespace minidb;

void PrintMetrics(const std::string& engine, double write_ops, double read_ops) {
    std::cout << std::left << std::setw(15) << engine 
              << "| " << std::setw(15) << write_ops 
              << "| " << std::setw(15) << read_ops << "\n";
}

void RunBenchmark(int num_records) {
    std::cout << "\n=======================================================\n";
    std::cout << " MiniDB Track C Benchmark: LSM-Tree vs B+ Tree \n";
    std::cout << " Workload: " << num_records << " Records\n";
    std::cout << "=======================================================\n";
    std::cout << std::left << std::setw(15) << "Engine" 
              << "| " << std::setw(15) << "Writes (ops/s)" 
              << "| " << std::setw(15) << "Reads (ops/s)" << "\n";
    std::cout << "-------------------------------------------------------\n";

    // Prepare Dataset
    std::vector<std::string> keys;
    std::vector<std::string> values;
    for (int i = 0; i < num_records; ++i) {
        keys.push_back(std::to_string(i));
        values.push_back("Standard_Payload_Data_" + std::to_string(i));
    }

    // ==========================================
    // 1. Benchmark: LSM-Tree (Your implementation)
    // ==========================================
    Catalog catalog;
    LockManager lock_manager;
    TransactionManager txn_manager(&lock_manager);
    Schema schema({Column("id", TypeId::INTEGER, 4), Column("payload", TypeId::VARCHAR, 255)});
    TableMetadata* table = catalog.CreateTable("lsm_table", schema);

    // LSM Write Test
    auto start_lsm_write = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_records; ++i) {
        InternalKey ikey{table->oid, keys[i]};
        Row row{{keys[i], values[i]}};
        table->wal->Append(LogRecordType::PUT, ikey.Encode(), row.Serialize());
        table->memtable->Put(ikey, row);
    }
    auto end_lsm_write = std::chrono::high_resolution_clock::now();
    double lsm_write_time = std::chrono::duration<double, std::milli>(end_lsm_write - start_lsm_write).count();
    double lsm_writes_per_sec = (num_records / lsm_write_time) * 1000.0;

    // LSM Read Test
    auto start_lsm_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_records; ++i) {
        InternalKey ikey{table->oid, keys[i]};
        Row out_row;
        table->memtable->Get(ikey, &out_row);
    }
    auto end_lsm_read = std::chrono::high_resolution_clock::now();
    double lsm_read_time = std::chrono::duration<double, std::milli>(end_lsm_read - start_lsm_read).count();
    double lsm_reads_per_sec = (num_records / lsm_read_time) * 1000.0;

    PrintMetrics("LSM-Tree", lsm_writes_per_sec, lsm_reads_per_sec);

    // ==========================================
    // 2. Benchmark: B+ Tree Baseline
    // ==========================================
    BPlusTree btree(100);

    // B+ Tree Write Test
    auto start_btree_write = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_records; ++i) {
        btree.Insert(keys[i], values[i]);
    }
    auto end_btree_write = std::chrono::high_resolution_clock::now();
    double btree_write_time = std::chrono::duration<double, std::milli>(end_btree_write - start_btree_write).count();
    double btree_writes_per_sec = (num_records / btree_write_time) * 1000.0;

    // B+ Tree Read Test
    auto start_btree_read = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < num_records; ++i) {
        btree.Search(keys[i]);
    }
    auto end_btree_read = std::chrono::high_resolution_clock::now();
    double btree_read_time = std::chrono::duration<double, std::milli>(end_btree_read - start_btree_read).count();
    double btree_reads_per_sec = (num_records / btree_read_time) * 1000.0;

    PrintMetrics("B+ Tree", btree_writes_per_sec, btree_reads_per_sec);
    std::cout << "=======================================================\n";
    
    // Quick analytical summary
    std::cout << "\n[Analysis]\n";
    if (lsm_writes_per_sec > btree_writes_per_sec) {
        std::cout << "-> LSM-Tree won Write Throughput (Expected: Append-only bypasses node splitting overhead).\n";
    }
    if (btree_reads_per_sec > lsm_reads_per_sec) {
        std::cout << "-> B+ Tree won Read Latency (Expected: Direct point lookups bypass MemTable/SSTable scanning).\n";
    }
}

int main() {
    RunBenchmark(50000); 
    return 0;
}
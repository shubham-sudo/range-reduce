#include "run_workload.h"

#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <tuple>

#include "config_options.h"
#include "perf_counter.h"
#include "utils.h"

std::string buffer_file = "workload.log";
std::string rqstats_file = "range_queries.csv";

int runWorkload(std::unique_ptr<DBEnv> &env) {
  DB *db;
  Options options;
  WriteOptions write_options;
  ReadOptions read_options;
  BlockBasedTableOptions table_options;
  FlushOptions flush_options;
  bool db_snapshot_exist = false;

  configOptions(env, &options, &table_options, &write_options, &read_options,
                &flush_options);

  // Add custom listners
  std::shared_ptr<CompactionsListner> compaction_listener =
      std::make_shared<CompactionsListner>();
  options.listeners.emplace_back(compaction_listener);

  std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>(buffer_file);

  if (env->IsDestroyDatabaseEnabled()) {
    DestroyDB(env->kDBPath, options);
    std::cerr << "Destroying database ... done" << std::endl;
    if (env->IsUseSavedDBEnabled()) {
      db_snapshot_exist = CheckSavedDBExistence(env, buffer);
      if (db_snapshot_exist) {
        assert(env->GetSnapshotTillOp() > 0);
        MakeCopyOfSnapshot(env, buffer);
      }
    }
  }

  PrintExperimentalSetup(env, buffer);

  Status s = DB::Open(options, env->kDBPath, &db);
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());
  Iterator *it = db->NewIterator(read_options);

#ifdef DOSTO
  if (env->debugging) {
    tree->SetDebugMode(env->debugging);
    tree->PrintFluidLSM(db);
  }
#endif // DOSTO

  // Clearing the system cache
  if (env->clear_system_cache) {
#ifdef __linux__
    std::cerr << "Clearing system cache ...";
    std::cerr << system("sudo sh -c 'echo 3 >/proc/sys/vm/drop_caches'")
              << "done" << std::endl;
#endif
  }

  std::ifstream workload_file;
  workload_file.open("workload.txt");
  assert(workload_file);

  size_t total_operations = 0;
  if (env->IsShowProgressEnabled()) {
    std::string line;
    while (std::getline(workload_file, line)) {
      ++total_operations;
    }
  }

  workload_file.clear();
  workload_file.seekg(0, std::ios::beg);

#ifdef PROFILE
  unsigned long num_epochs = 8;
  unsigned long op_in_epoch =
      (env->num_updates / num_epochs) + (env->num_range_queries / num_epochs);
  unsigned long op_done_in_epoch = 0;
  std::cout << op_in_epoch << std::endl;
#endif // PROFILE

#ifdef TIMER
  unsigned long inserts_exec_time = 0, updates_exec_time = 0, pq_exec_time = 0,
                pdelete_exec_time = 0, rq_exec_time = 0, rdelete_exec_time = 0;
  std::unique_ptr<Buffer> rqstats = std::make_unique<Buffer>(rqstats_file);
  (*rqstats) // adds a header
      << "RQ Number, RQ Total Time, "
         //  "Data uBytes Written Back, Total "
         //  "uBytes Written Back, uEntries Count Written Back, "
         "Total Entries Read, "
         //  "Data unBytes Written Back, Total unBytes "
         //  "Written Back, unEntries Count Written Back, "
         "Total Entries Returned, "
         "RQ Refresh Time, RQ Reset Time, Actual RQ Time, Did Run RR, "
         "Instructions, Cycles, L1D Cache Misses"
      << std::endl;
  unsigned long rqnumber = 0;
#endif // TIMER
  auto exec_start = std::chrono::high_resolution_clock::now();

  std::string line;
  unsigned long ith_op = 0;
  while (std::getline(workload_file, line)) {
    if (line.empty())
      break;
    bool is_last_line = (workload_file.peek() == EOF);

    if (env->IsUseSavedDBEnabled() && env->GetSnapshotTillOp() > 0 &&
        ith_op + 1 > env->GetSnapshotTillOp() && !db_snapshot_exist) {
      {
        std::vector<std::string> live_files;
        uint64_t manifest_size;
        db->GetLiveFiles(live_files, &manifest_size, true /*flush_memtable*/);
        WaitForCompactions(db);
      }
      SnapshotDB(env, buffer);
      db_snapshot_exist = true;
    }

    std::istringstream stream(line);
    char operation;
    stream >> operation;

    switch (operation) {
      // [Insert]
    case 'I': {
      std::string key, value;
      stream >> key >> value;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }

#ifdef TIMER
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      s = db->Put(write_options, key, value);
#ifdef TIMER
      auto stop = std::chrono::high_resolution_clock::now();
      inserts_exec_time +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
              .count();
#endif // TIMER
      break;
    }
      // [Update]
    case 'U': {
      std::string key, value;
      stream >> key >> value;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }
#ifdef PROFILE
      op_done_in_epoch += 1;
#endif // PROFILE

#ifdef TIMER
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      s = db->Put(write_options, key, value);
#ifdef TIMER
      auto stop = std::chrono::high_resolution_clock::now();
      updates_exec_time +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
              .count();
#endif // TIMER
      break;
    }
      // [PointDelete]
    case 'D': {
      std::string key;
      stream >> key;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }

#ifdef TIMER
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      s = db->Delete(write_options, key);
#ifdef TIMER
      auto stop = std::chrono::high_resolution_clock::now();
      pdelete_exec_time +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
              .count();
#endif // TIMER
      break;
    }
      // [ProbePointQuery]
    case 'Q': {
      std::string key, value;
      stream >> key;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }
#ifdef PROFILE
      op_done_in_epoch += 1;
#endif // PROFILE

#ifdef TIMER
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      s = db->Get(read_options, key, &value);
#ifdef TIMER
      auto stop = std::chrono::high_resolution_clock::now();
      pq_exec_time +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
              .count();
#endif // TIMER
      break;
    }
      // [ScanRangeQuery]
    case 'S': {
      std::string start_key, end_key;
      stream >> start_key >> end_key;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }
#ifdef PROFILE
      op_done_in_epoch += 1;
#endif // PROFILE

      uint64_t keys_returned = 0, keys_read = 0;
      bool did_run_RR = false;
#ifdef TIMER
      // PerfCounter counter;
      // counter.start();
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER

      it->Refresh(start_key, end_key, keys_read,
                  env->enable_range_query_compaction,
                  env->min_entries_shld_be_read_per_lvl);
#ifdef TIMER
      auto refresh_time = std::chrono::high_resolution_clock::now();
      auto refresh_duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(refresh_time -
                                                               start)
              .count();
#endif // TIMER
      assert(it->status().ok());
      for (it->Seek(start_key); it->Valid(); it->Next()) {
        if (it->key().ToString() >= end_key) {
          break;
        }
        keys_returned++;
      }
      if (!it->status().ok()) {
        (*buffer) << it->status().ToString() << std::endl << std::flush;
      }
#ifdef TIMER
      auto reset_start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      it->Reset(keys_read, did_run_RR);
#ifdef TIMER
      auto reset_end = std::chrono::high_resolution_clock::now();
      auto actual_range_time =
          std::chrono::duration_cast<std::chrono::nanoseconds>(reset_start -
                                                               refresh_time)
              .count();
      auto reset_duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(reset_end -
                                                               reset_start)
              .count();
      auto stop = std::chrono::high_resolution_clock::now();
      auto duration =
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start);
      rq_exec_time += duration.count();
      // counter.stop();
      // counter.read();
      (*rqstats)
          << ++rqnumber << ", " << duration.count()
          << ", "
          // << range_reduce_listener->useful_data_blocks_size_ << ", "
          // << range_reduce_listener->useful_file_size_ << ", "
          // << range_reduce_listener->useful_entries_ << ", "
          << keys_read
          << ", "
          // << range_reduce_listener->un_useful_data_blocks_size_
          // << ", " << range_reduce_listener->un_useful_file_size_ << ", "
          // << range_reduce_listener->un_useful_entries_ << ", "
          << keys_returned << ", " << refresh_duration << ", " << reset_duration
          << ", " << actual_range_time << ", " << did_run_RR;
      // counter.reportCSV(rqstats);
      (*rqstats) << std::endl;
#endif // TIMER
      break;
    }
      // [RangeDelete]
    case 'R': {
      std::string start_key, end_key;
      stream >> start_key >> end_key;
      if (env->GetSnapshotTillOp() >= ith_op + 1 && db_snapshot_exist) {
        ith_op += 1;
        continue;
      }
#ifdef TIMER
      auto start = std::chrono::high_resolution_clock::now();
#endif // TIMER
      s = db->DeleteRange(write_options, start_key, end_key);
#ifdef TIMER
      auto stop = std::chrono::high_resolution_clock::now();
      rdelete_exec_time +=
          std::chrono::duration_cast<std::chrono::nanoseconds>(stop - start)
              .count();
#endif // TIMER
      break;
    }
    default:
      (*buffer) << "ERROR: Case match NOT found !!" << std::endl;
      break;
    }

    ith_op += 1;
    UpdateProgressBar(env, ith_op, total_operations,
                      (int)total_operations * 0.02);
#ifdef PROFILE
    if (ith_op == env->num_inserts) {
      (*buffer) << "=====================" << std::endl;
      (*buffer) << "Inserts are completed ..." << std::endl;
      LogTreeState(db, buffer);
      LogRocksDBStatistics(db, options, buffer);
#ifdef TIMER
      (*buffer) << "Inserts Execution Time: " << inserts_exec_time << std::endl;
      (*buffer) << "Updates Execution Time: " << updates_exec_time << std::endl;
      (*buffer) << "PointQuery Execution Time: " << pq_exec_time << std::endl;
      (*buffer) << "PointDelete Execution Time: " << pdelete_exec_time
                << std::endl;
      (*buffer) << "RangeQuery Execution Time: " << rq_exec_time << std::endl;
      (*buffer) << "RangeDelete Execution Time: " << rdelete_exec_time
                << std::endl;
#endif // TIMER
    } else if (op_done_in_epoch == op_in_epoch && ith_op > env->num_inserts) {
      (*buffer) << "=====================" << std::endl;
      (*buffer) << "One Epoch done ... " << ith_op << " operation" << std::endl;
      LogTreeState(db, buffer);
      LogRocksDBStatistics(db, options, buffer);
      op_done_in_epoch = 0;
#ifdef TIMER
      (*buffer) << "Inserts Execution Time: " << inserts_exec_time << std::endl;
      (*buffer) << "Updates Execution Time: " << updates_exec_time << std::endl;
      (*buffer) << "PointQuery Execution Time: " << pq_exec_time << std::endl;
      (*buffer) << "PointDelete Execution Time: " << pdelete_exec_time
                << std::endl;
      (*buffer) << "RangeQuery Execution Time: " << rq_exec_time << std::endl;
      (*buffer) << "RangeDelete Execution Time: " << rdelete_exec_time
                << std::endl;
#endif // TIMER
    }
#endif // PROFILE

    if (is_last_line)
      break;
  }

#ifdef PROFILE
  (*buffer) << "=====================" << std::endl;
  LogTreeState(db, buffer);
  LogRocksDBStatistics(db, options, buffer);
#endif // PROFILE

  auto total_exec_time =
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::high_resolution_clock::now() - exec_start)
          .count();
#ifdef TIMER
  (*buffer) << "=====================" << std::endl;
  (*buffer) << "Workload Execution Time: " << total_exec_time << std::endl;
  (*buffer) << "Inserts Execution Time: " << inserts_exec_time << std::endl;
  (*buffer) << "Updates Execution Time: " << updates_exec_time << std::endl;
  (*buffer) << "PointQuery Execution Time: " << pq_exec_time << std::endl;
  (*buffer) << "PointDelete Execution Time: " << pdelete_exec_time << std::endl;
  (*buffer) << "RangeQuery Execution Time: " << rq_exec_time << std::endl;
  (*buffer) << "RangeDelete Execution Time: " << rdelete_exec_time << std::endl;
#endif // TIMER

  // delete iterator and close db
  delete it;
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());
  s = db->Close();
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());

  PrintRocksDBPerfStats(env, buffer, options);
  table_options.block_cache.reset();
  options.table_factory.reset();
  (*buffer) << "===========END HERE=========\n";

  // flush final stats and delete ptr
  buffer->flush();
  long long total_seconds = total_exec_time / 1e9;
  std::cout << "Experiment completed in " << total_seconds / 3600 << "h "
            << (total_seconds % 3600) / 60 << "m " << total_seconds % 60 << "s "
            << std::endl;

  if (env->IsSanityCheckEnabled()) {
    std::cout << std::endl << std::endl;
    std::cout << "Running Sanity Check ... " << std::endl;
    runSanityCheck(env, total_operations);
  }
  return 0;
}

void runSanityCheck(std::unique_ptr<DBEnv> &env, size_t total_operations) {
  std::unordered_set<std::string> seen_keys;
  std::ifstream file("workload.txt", std::ios::ate);
  assert(file);

  DB *db;
  Options options;
  WriteOptions write_options;
  ReadOptions read_options;
  BlockBasedTableOptions table_options;
  FlushOptions flush_options;

  configOptions(env, &options, &table_options, &write_options, &read_options,
                &flush_options);

  Status s = DB::Open(options, env->kDBPath, &db);
  if (!s.ok())
    std::cerr << s.ToString() << std::endl;
  assert(s.ok());

  std::streampos pos = file.tellg();
  std::string line;
  std::string buffer;
  int ith_op = 0;
  std::unordered_map<char, int> successful_ops, failed_ops, skipped_ops;
  std::unordered_set<char> operations;

  while (pos > 0) {
    pos -= 1;
    file.seekg(pos, std::ios::beg);

    char ch;
    file.get(ch);

    if (ch == '\n' || pos == 0) {
      if (!buffer.empty()) {
        std::reverse(buffer.begin(), buffer.end());
        std::istringstream stream(buffer);
        char operation;
        std::string key, value;
        stream >> operation >> key;

        if (operations.find(operation) == operations.end()) {
          operations.insert(operation);
          if (successful_ops.find(operation) == successful_ops.end()) {
            successful_ops[operation] = 0;
          }
          if (failed_ops.find(operation) == failed_ops.end()) {
            failed_ops[operation] = 0;
          }
          if (skipped_ops.find(operation) == skipped_ops.end()) {
            skipped_ops[operation] = 0;
          }
        }

        if (seen_keys.find(key) != seen_keys.end()) {
          skipped_ops[operation]++;
        } else {
          seen_keys.insert(key);

          if (operation == 'I' || operation == 'U') {
            stream >> value;
            std::string actual_value;
            Status db_status = db->Get(ReadOptions(), key, &actual_value);

            if (!db_status.ok()) {
              std::cerr << db_status.ToString() << std::endl;
              failed_ops[operation]++;
            } else if (actual_value != value) {
              std::cerr << "ERROR: Key " << key
                        << " mismatch! Expected: " << value
                        << ", Found: " << actual_value << std::endl;
              failed_ops[operation]++;
            } else {
              successful_ops[operation]++;
            }
          } else {
            skipped_ops[operation]++;
          }
        }
      }
      buffer.clear();
      ith_op += 1;
      UpdateProgressBar(env, ith_op, total_operations,
                        (int)total_operations * 0.02);
    } else {
      buffer += ch;
    }
  }

  constexpr int colWidth = 10;

  std::cout << "Sanity check completed!" << std::endl;
  std::cout << "\n========== Sanity Check Report ==========\n";
  std::cout << std::setfill(' ') << std::setw(colWidth) << "Operation |"
            << std::setfill(' ') << std::setw(colWidth) << "Success | "
            << std::setfill(' ') << std::setw(colWidth) << "Failed | "
            << std::setfill(' ') << std::setw(colWidth) << "Skipped"
            << std::endl;
  std::cout << "-----------------------------------------\n";
  for (char op : operations) {
    std::cout << std::setfill(' ') << std::setw(colWidth) << op << "|"
              << std::setfill(' ') << std::setw(colWidth) << successful_ops[op]
              << "|" << std::setfill(' ') << std::setw(colWidth)
              << failed_ops[op] << "|" << std::setfill(' ')
              << std::setw(colWidth) << skipped_ops[op] << std::endl;
  }
  std::cout << "=========================================\n";
  db->Close();
}
#include <filesystem>
#include <iomanip>
#include <iostream>

#include <rocksdb/db.h>
#include <rocksdb/iostats_context.h>
#include <rocksdb/perf_context.h>
#include <rocksdb/statistics.h>

#include "db_env.h"
#include "event_listners.h"

using namespace rocksdb;

std::string GetSavedDBName(std::unique_ptr<DBEnv> &env) {
  std::stringstream saved_db_dir;
  saved_db_dir << "db_config_" << "M_" << env->GetBufferSize() << "P_"
               << env->buffer_size_in_pages << "B_" << env->entries_per_page
               << "E_" << env->entry_size << "T_" << env->size_ratio << "OP_"
               << env->GetSnapshotTillOp();
  return saved_db_dir.str();
}

bool CheckSavedDBExistence(std::unique_ptr<DBEnv> &env,
                           std::unique_ptr<Buffer> &buffer) {
  namespace fs = std::filesystem;
  fs::path db_dir = fs::relative(DBEnv::kSavedDBPath) / GetSavedDBName(env);

  if (!fs::exists(db_dir) || !fs::is_directory(db_dir)) {
    (*buffer) << "Saved db " << db_dir << "does not exists" << std::endl;
    return false;
  }

  for (const auto &entry : fs::directory_iterator(db_dir)) {
    if (fs::is_regular_file(entry)) {
      return true;
    }
  }

  (*buffer) << "Saved db " << db_dir << " is empty!" << std::endl;
  fs::remove(db_dir);
  return false;
}

bool MakeCopyOfSnapshot(std::unique_ptr<DBEnv> &env,
                        std::unique_ptr<Buffer> &buffer) {
  namespace fs = std::filesystem;
  fs::path source_dir = fs::relative(DBEnv::kSavedDBPath) / GetSavedDBName(env);
  fs::path destination_dir = fs::relative(DBEnv::kDBPath);

  assert(fs::exists(source_dir) && fs::is_directory(source_dir));
  if (!fs::exists(destination_dir)) {
    fs::create_directories(destination_dir);
  }

  for (const auto &entry : fs::directory_iterator(source_dir)) {
    if (fs::is_regular_file(entry)) {
      fs::path dest_file = destination_dir / entry.path().filename();
      fs::copy_file(entry.path(), dest_file,
                    fs::copy_options::overwrite_existing);
    }
  }
  (*buffer) << "using snapshot db from: " << destination_dir.string()
            << std::endl;
  return true;
}

void SnapshotDB(std::unique_ptr<DBEnv> &env, std::unique_ptr<Buffer> &buffer) {
  namespace fs = std::filesystem;
  fs::path source_dir = fs::relative(DBEnv::kDBPath);
  fs::path destination_dir =
      fs::relative(DBEnv::kSavedDBPath) / GetSavedDBName(env);

  assert(fs::exists(source_dir) && fs::is_directory(source_dir));
  if (!fs::exists(destination_dir)) {
    fs::create_directories(destination_dir);
  }

  for (const auto &entry : fs::directory_iterator(source_dir)) {
    if (fs::is_regular_file(entry)) {
      fs::path dest_file = destination_dir / entry.path().filename();
      fs::copy_file(entry.path(), dest_file,
                    fs::copy_options::overwrite_existing);
    }
  }

  (*buffer) << "snapshot created at: " << destination_dir.string() << std::endl;
}

template <typename T>
void PrintColumn(T value, int width, std::unique_ptr<Buffer> &buffer) {
  (*buffer) << std::setfill(' ') << std::setw(width) << value;
}

void PrintExperimentalSetup(std::unique_ptr<DBEnv> &env,
                            std::unique_ptr<Buffer> &buffer) {
  constexpr int colWidth = 10;
  constexpr int smallColWidth = 4;

  PrintColumn("cmpt_sty", colWidth, buffer);  // Compaction style
  PrintColumn("cmpt_pri", colWidth, buffer);  // Compaction priority
  PrintColumn("T", smallColWidth, buffer);    // Size ratio
  PrintColumn("P", colWidth, buffer);         // Buffer size in pages
  PrintColumn("B", colWidth, buffer);         // Entries per page
  PrintColumn("E", colWidth, buffer);         // Entry size
  PrintColumn("M", colWidth, buffer);         // Buffer size
  PrintColumn("file_size", colWidth, buffer); // Target file size base
  PrintColumn("L1_size", colWidth, buffer);   // Max bytes for level base
  PrintColumn("blk_cch", colWidth, buffer);   // Block cache
  PrintColumn("BPK", colWidth, buffer);       // Bits per key
  (*buffer) << std::endl;

  PrintColumn(env->compaction_style, colWidth, buffer);
  PrintColumn(env->compaction_pri, colWidth, buffer);
  PrintColumn(env->size_ratio, smallColWidth, buffer);
  PrintColumn(env->buffer_size_in_pages, colWidth, buffer);
  PrintColumn(env->entries_per_page, colWidth, buffer);
  PrintColumn(env->entry_size, colWidth, buffer);
  PrintColumn(env->GetBufferSize(), colWidth, buffer);
  PrintColumn(env->GetTargetFileSizeBase(), colWidth, buffer);
  PrintColumn(env->GetMaxBytesForLevelBase(), colWidth, buffer);
  PrintColumn(env->block_cache, colWidth, buffer);
  PrintColumn(env->bits_per_key, colWidth, buffer);
  (*buffer) << std::endl;
}

void PrintRocksDBPerfStats(std::unique_ptr<DBEnv> &env,
                           std::unique_ptr<Buffer> &buffer, Options options) {
  if (env->IsPerfIOStatEnabled()) {
    rocksdb::SetPerfLevel(rocksdb::PerfLevel::kDisable);

    (*buffer) << "\n\n===============================\n";
    (*buffer) << "     RocksDB Performance Stats \n";
    (*buffer) << "===============================\n";

    (*buffer) << "[Perf Context]\n";
    (*buffer) << rocksdb::get_perf_context()->ToString() << "\n";

    (*buffer) << "\n[IO Stats Context]\n";
    (*buffer) << rocksdb::get_iostats_context()->ToString() << "\n";

    (*buffer) << "\n[Rocksdb Stats]\n";
    (*buffer) << options.statistics->ToString();
    options.statistics.reset();
  }
#ifdef PROFILE
  options.statistics.reset();
#endif // PROFILE
}

void UpdateProgressBar(std::unique_ptr<DBEnv> &env, size_t current,
                       size_t total, size_t update_interval, size_t bar_width) {
  if (env->IsShowProgressEnabled() &&
      (current % update_interval == 0 || current == total)) {
    double progress = static_cast<double>(current) / total;
    size_t pos = static_cast<size_t>(bar_width * progress);

    std::cerr << "[";
    for (size_t i = 0; i < bar_width; ++i) {
      if (i < pos)
        std::cerr << "=";
      else if (i == pos)
        std::cerr << ">";
      else
        std::cerr << " ";
    }
    std::cerr << "] " << std::fixed << std::setprecision(2)
              << (progress * 100.0) << "%\r";
    std::cerr.flush();
  }
}

#ifdef PROFILE
void LogTreeState(rocksdb::DB *db, std::unique_ptr<Buffer> &buffer) {
  // Wait for compactions and get live files
  {
    std::vector<std::string> live_files;
    uint64_t manifest_size;
    db->GetLiveFiles(live_files, &manifest_size, true /*flush_memtable*/);
    WaitForCompactions(db);
  }

  ColumnFamilyMetaData metadata;
  db->GetColumnFamilyMetaData(&metadata);
  std::stringstream cfd_details;

  cfd_details << "Column Family Name: " << metadata.name
              << ", Size: " << metadata.size
              << " bytes, Files Count: " << metadata.file_count << std::endl;

  cfd_details << "Level Stats:" << std::endl;
  for (const auto &level : metadata.levels) {
    cfd_details << "Level: " << level.level << ", Files: " << level.files.size()
                << ", Size: " << level.size << " bytes" << std::endl;
  }

  // std::tuple<unsigned long long, std::string> details =
  //   db->GetTreeState();

  // unsigned long long total_entries_in_cfd = std::get<0>(details);
  // std::string all_level_details = std::get<1>(details);

  (*buffer) << cfd_details.str() << std::endl;
  // << ", Entries Count: " << total_entries_in_cfd
  // << ", Invalid Entries Count: "
  // << total_entries_in_cfd - env->num_inserts << std::endl
  // << all_level_details << std::endl;
}

void LogRocksDBStatistics(rocksdb::DB *db, const rocksdb::Options &options,
                          std::unique_ptr<Buffer> &buffer) {
  auto printProperty = [&](const std::string &propertyName) {
    std::string value;
    bool status = db->GetProperty(propertyName, &value);
    if (status) {
      (*buffer) << propertyName << ": " << value << std::endl;
    } else {
      (*buffer) << "Error getting property " << propertyName << ": " << status
                << std::endl;
    }
  };

  // Print RocksDB Ticker statistics
  (*buffer) << "RocksDB Statistics: " << std::endl;
  (*buffer) << "rocksdb.compact.read.bytes: "
            << options.statistics->getTickerCount(COMPACT_READ_BYTES)
            << std::endl;
  (*buffer) << "rocksdb.compact.write.bytes: "
            << options.statistics->getTickerCount(COMPACT_WRITE_BYTES)
            << std::endl;
  (*buffer) << "rocksdb.flush.write.bytes: "
            << options.statistics->getTickerCount(FLUSH_WRITE_BYTES)
            << std::endl;
  (*buffer) << "rocksdb.rangereduce.file.count: "
            << options.statistics->getTickerCount(RANGEREDUCE_FILE_FLUSH_COUNT)
            << std::endl;
  (*buffer) << "rocksdb.rangereduce.write.bytes: "
            << options.statistics->getTickerCount(RANGEREDUCE_FILE_WRITE_BYTES)
            << std::endl;
  (*buffer) << "rocksdb.compaction.times.micros: "
            << options.statistics->getTickerCount(COMPACTION_TIME) << std::endl
            << std::endl;
}
#endif // PROFILE
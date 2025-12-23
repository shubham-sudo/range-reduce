#include <iostream>

#include "args.hxx"
#include "db_env.h"

int parse_arguments(int argc, char *argv[], std::unique_ptr<DBEnv> &env) {
  args::ArgumentParser parser("RocksDB_parser.", "");
  args::Group group1(parser, "This group is all exclusive:",
                     args::Group::Validators::DontCare);

  args::ValueFlag<int> destroy_database_cmd(
      group1, "d", "Destroy and recreate the database [def: 1]",
      {'d', "destroy"});
  args::ValueFlag<int> clear_system_cache_cmd(
      group1, "cc", "Clear system cache [def: 1]", {"cc"});

  args::ValueFlag<int> size_ratio_cmd(
      group1, "T",
      "The number of unique inserts to issue in the experiment [def: 10]",
      {'T', "size_ratio"});
  args::ValueFlag<int> buffer_size_in_pages_cmd(
      group1, "P",
      "The number of unique inserts to issue in the experiment [def: 512]",
      {'P', "buffer_size_in_pages"});
  args::ValueFlag<int> entries_per_page_cmd(
      group1, "B",
      "The number of unique inserts to issue in the experiment [def: 4]",
      {'B', "entries_per_page"});
  args::ValueFlag<int> entry_size_cmd(
      group1, "E",
      "The number of unique inserts to issue in the experiment [def: 1024 B]",
      {'E', "entry_size"});
  args::ValueFlag<long> buffer_size_cmd(
      group1, "M",
      "The number of unique inserts to issue in the experiment [def: 16 MB]",
      {'M', "memory_size"});
  args::ValueFlag<int> file_to_memtable_size_ratio_cmd(
      group1, "file_to_memtable_size_ratio",
      "The number of unique inserts to issue in the experiment [def: 1]",
      {'f', "file_to_memtable_size_ratio"});
  args::ValueFlag<long> file_size_cmd(
      group1, "file_size",
      "The number of unique inserts to issue in the experiment [def: 256 KB]",
      {'F', "file_size"});
  args::ValueFlag<int> verbosity_cmd(
      group1, "verbosity", "The verbosity level of execution [0,1,2; def: 0]",
      {'V', "verbosity"});
  args::ValueFlag<int> succinct_kv_trigger_cmd(
      group1, "succinct_kv_trigger",
      "Trigger RQ-based compaction in SuccinctKV way", {"succinctkv"});
  args::ValueFlag<int> compaction_pri_cmd(
      group1, "compaction_pri",
      "[Compaction priority: 1 for kMinOverlappingRatio, 2 for "
      "kByCompensatedSize, 3 for kOldestLargestSeqFirst, 4 for "
      "kOldestSmallestSeqFirst; def: 1]",
      {'c', "compaction_pri"});
  args::ValueFlag<int> compaction_style_cmd(
      group1, "compaction_style",
      "[Compaction priority: 1 for kCompactionStyleLevel, 2 for "
      "kCompactionStyleUniversal, 3 for kCompactionStyleFIFO, 4 for "
      "kCompactionStyleNone; def: 1]",
      {'C', "compaction_style"});
  args::ValueFlag<int> bits_per_key_cmd(
      group1, "bits_per_key",
      "The number of bits per key assigned to Bloom filter [def: 10]",
      {'b', "bits_per_key"});
  args::ValueFlag<int> block_cache_cmd(
      group1, "bb", "Block cache size in MB [def: 8 MB]", {"bb"});
  args::ValueFlag<int> enable_perf_iostat_cmd(
      group1, "enable_perf_iostat",
      "Enable RocksDB's internal Perf and IOstat [def: 0]", {"stat"});
  args::ValueFlag<int> show_progress_cmd(
      group1, "show_progress_bar", "Shows progress bar [def: 0]", {"progress"});
  args::ValueFlag<int> enable_sanity_check_cmd(
      group1, "enable_sanity_check",
      "Enable Sanity check to verify Database after experiment [def: 0]",
      {"sanity"});
  args::ValueFlag<int> use_saved_db_cmd(
      group1, "use_saved_db",
      "Enable Using Saved DB from last execution (if available) [def: 0]",
      {"usedb"});
  args::ValueFlag<long> snapshot_till_cmd(
      group1, "snapshot_till",
      "Snapshot satabase till this operation, 0 indexing [def: -1]", {"snap"});

  // Range Query Driven Compaction Options
  args::ValueFlag<long> num_inserts_cmd(
      group1, "inserts",
      "The number of unique inserts to issue in the experiment [def: 1]",
      {'I', "inserts"});
  args::ValueFlag<long> num_updates_cmd(
      group1, "updates",
      "The number of unique updates to issue in the experiment [def: 0]",
      {'U', "updates"});
  args::ValueFlag<long> num_range_queries_cmd(
      group1, "range_queries",
      "The number of unique range queries to issue in the experiment [def: 0]",
      {'S', "range_queries"});
  args::ValueFlag<bool> level_compaction_dynamic_level_bytes_cmd(
      group1, "level_compaction_dynamic_level_bytes",
      "If true, levels will be formed bottom up",
      {"lcd", "level_compaction_dynamic"});

  args::ValueFlag<int> enable_range_query_compaction_cmd(
      group1, "enable_range_query_compaction",
      "Enable range query comapaction [def: 0]",
      {"rq", "range_query_compaction"});

  args::ValueFlag<int> max_multi_trivial_move_cmd(
      group1, "max_multi_trivial_move",
      "Maximum file it can move trivially [def: 4]",
      {"tmv", "multi_trivial_move"});
  args::ValueFlag<int> level_renaming_enabled_cmd(
      group1, "enable_level_renaming",
      "Enable level renaming when to add new level", {"re", "renaming_level"});

  args::ValueFlag<float> upper_threshold_cmd(
      group1, "higher_bound",
      "Higher threshold between adjancent levels to perform compaction [def: "
      "inf]",
      {"ub", "upper_threshold"});
  args::ValueFlag<float> lower_threshold_cmd(
      group1, "lower_bound",
      "Lower threshold between adjacent levels to perform compaction [def: 0]",
      {"lb", "lower_threshold"});
  args::ValueFlag<long> min_entries_shld_be_read_per_lvl_cmd(
      group1, "min_entries_shld_be_read_per_lvl",
      "Minimum entries that must be read from a level to qualify for "
      "RangeReduce compaction [def: (P x B) / 2]",
      {"epl", "min_entries_read_per_level"});
  args::ValueFlag<float> range_query_selectivity_cmd(
      group1, "Y", "Range query selectivity [def: 0]",
      {'Y', "range_query_selectivity"});

  // Fluid LSM parameters
  args::ValueFlag<float> smaller_lvl_runs_count_cmd(
      group1, "K", "Number of run in smaller levels", {'K', "k"});
  args::ValueFlag<float> larger_lvl_runs_count_cmd(
      group1, "Z", "Number of run in larger levels", {'Z', "z"});

  try {
    parser.ParseCLI(argc, argv);
  } catch (args::Help &) {
    std::cout << parser;
    exit(0);
  } catch (args::ParseError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  } catch (args::ValidationError &e) {
    std::cerr << e.what() << std::endl;
    std::cerr << parser;
    return 1;
  }

  env->SetDestroyDatabase(destroy_database_cmd
                              ? args::get(destroy_database_cmd)
                              : env->IsDestroyDatabaseEnabled());
  env->clear_system_cache = clear_system_cache_cmd
                                ? args::get(clear_system_cache_cmd)
                                : env->clear_system_cache;
  env->size_ratio =
      size_ratio_cmd ? args::get(size_ratio_cmd) : env->size_ratio;
  env->buffer_size_in_pages = buffer_size_in_pages_cmd
                                  ? args::get(buffer_size_in_pages_cmd)
                                  : env->buffer_size_in_pages;
  env->entries_per_page = entries_per_page_cmd ? args::get(entries_per_page_cmd)
                                               : env->entries_per_page;
  env->entry_size =
      entry_size_cmd ? args::get(entry_size_cmd) : env->entry_size;
  env->SetBufferSize(buffer_size_cmd ? args::get(buffer_size_cmd) : 0);
  env->file_to_memtable_size_ratio =
      file_to_memtable_size_ratio_cmd
          ? args::get(file_to_memtable_size_ratio_cmd)
          : env->file_to_memtable_size_ratio;
  env->verbosity = verbosity_cmd ? args::get(verbosity_cmd) : env->verbosity;
  env->succinct_kv_trigger = succinct_kv_trigger_cmd
                                 ? args::get(succinct_kv_trigger_cmd)
                                 : env->succinct_kv_trigger;
  env->compaction_pri =
      compaction_pri_cmd ? args::get(compaction_pri_cmd) : env->compaction_pri;
  env->compaction_style = compaction_style_cmd ? args::get(compaction_style_cmd)
                                               : env->compaction_style;
  env->bits_per_key =
      bits_per_key_cmd ? args::get(bits_per_key_cmd) : env->bits_per_key;
  env->block_cache =
      block_cache_cmd ? args::get(block_cache_cmd) : env->block_cache;
  env->SetPerfIOStat(enable_perf_iostat_cmd ? args::get(enable_perf_iostat_cmd)
                                            : env->IsPerfIOStatEnabled());
  env->SetShowProgress(show_progress_cmd ? args::get(show_progress_cmd)
                                         : env->IsShowProgressEnabled());
  env->SetSanityCheck(enable_sanity_check_cmd
                          ? args::get(enable_sanity_check_cmd)
                          : env->IsSanityCheckEnabled());
  env->SetUseSavedDB(use_saved_db_cmd ? args::get(use_saved_db_cmd)
                                      : env->IsUseSavedDBEnabled());
  env->SetSnapshotTill(snapshot_till_cmd ? args::get(snapshot_till_cmd)
                                         : env->GetSnapshotTillOp());

  // Range Query Driven Compaction Options
  env->num_inserts =
      num_inserts_cmd ? args::get(num_inserts_cmd) : env->num_inserts;
  env->num_updates =
      num_updates_cmd ? args::get(num_updates_cmd) : env->num_updates;
  env->num_range_queries = num_range_queries_cmd
                               ? args::get(num_range_queries_cmd)
                               : env->num_range_queries;
  env->enable_range_query_compaction =
      enable_range_query_compaction_cmd
          ? args::get(enable_range_query_compaction_cmd)
          : env->enable_range_query_compaction;
  env->enable_level_renaming = level_renaming_enabled_cmd
                                   ? args::get(level_renaming_enabled_cmd)
                                   : env->enable_level_renaming;

  env->lower_threshold = lower_threshold_cmd ? args::get(lower_threshold_cmd)
                                             : env->lower_threshold;
  env->upper_threshold = upper_threshold_cmd ? args::get(upper_threshold_cmd)
                                             : env->upper_threshold;
  env->min_entries_shld_be_read_per_lvl =
      min_entries_shld_be_read_per_lvl_cmd
          ? args::get(min_entries_shld_be_read_per_lvl_cmd)
          : (env->entries_per_page * env->buffer_size_in_pages) / 2;
  env->max_multi_trivial_move = max_multi_trivial_move_cmd
                                    ? args::get(max_multi_trivial_move_cmd)
                                    : env->max_multi_trivial_move;
  env->level_compaction_dynamic_level_bytes =
      level_compaction_dynamic_level_bytes_cmd
          ? args::get(level_compaction_dynamic_level_bytes_cmd)
          : env->level_compaction_dynamic_level_bytes;

  // Fluid LSM parameters
  env->smaller_lvl_runs_count = smaller_lvl_runs_count_cmd
                                    ? args::get(smaller_lvl_runs_count_cmd)
                                    : env->smaller_lvl_runs_count;
  env->larger_lvl_runs_count = larger_lvl_runs_count_cmd
                                   ? args::get(larger_lvl_runs_count_cmd)
                                   : env->larger_lvl_runs_count;

  return 0;
}

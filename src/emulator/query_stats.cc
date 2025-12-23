#include "query_stats.h"

namespace emulator
{
    std::string getQueryType(QueryType qtype);
    QueryStats *QueryStats::_instance = nullptr;

    void QueryStats::reset()
    {
        query_number = -1;
        query_type = QueryType::INVALID;
        key = "";
        total_num_compaction = 0;
        total_num_files_written_while_compaction = 0;
        total_num_pages_written_while_compaction = 0;
        num_trivial_file_moves = 0;
        num_trivial_file_move_from_buffer = 0;
        num_buffer_flush = 0;
        num_page_flushed_from_buffer = 0;
        num_files_written_during_range_query = 0;
        num_pages_written_during_range_query = 0;
        num_entries_written_during_range_query = 0;
        total_entries_written_buffer_flush = 0;
        total_vanilla_compaction = 0;
        total_rqd_compaction = 0;

        entries_count_before_query = 0;
        entries_count_after_query = 0;
        total_time_taken = 0;
    }

    QueryStats *QueryStats::getQueryStats()
    {
        if (QueryStats::_instance == nullptr)
        {
            QueryStats::_instance = new QueryStats();
        }
        return QueryStats::_instance;
    }

    void QueryStats::print()
    {
        std::cout << query_number << ", "
           << getQueryType(query_type) << ", "
           << key << ", "

           << total_num_compaction << ", "
           << total_num_files_written_while_compaction << ", "
           << total_num_pages_written_while_compaction << ", "
           << total_entries_written_while_compaction << ", "

           << num_trivial_file_moves << ", "

           << num_buffer_flush << ", "
           << num_page_flushed_from_buffer << ", "
           << num_trivial_file_move_from_buffer << ", "
           << total_entries_written_buffer_flush << ", "

           << num_files_written_during_range_query << ", "
           << num_pages_written_during_range_query << ", "
           << num_entries_written_during_range_query << ", "

           << total_vanilla_compaction << ", "
           << total_rqd_compaction << ", "

           << entries_count_before_query << ", "
           << entries_count_after_query << ", "
           << total_time_taken << ", "
           << std::endl;
    }

    std::ostream &operator<<(std::ostream &os, const QueryStats qstats)
    {
        os << qstats.query_number << ", "
           << getQueryType(qstats.query_type) << ", "
           << qstats.key << ", "

           << qstats.total_num_compaction << ", "
           << qstats.total_num_files_written_while_compaction << ", "
           << qstats.total_num_pages_written_while_compaction << ", "
           << qstats.total_entries_written_while_compaction << ", "

           << qstats.num_trivial_file_moves << ", "

           << qstats.num_buffer_flush << ", "
           << qstats.num_page_flushed_from_buffer << ", "
           << qstats.num_trivial_file_move_from_buffer << ", "
           << qstats.total_entries_written_buffer_flush << ", "

           << qstats.num_files_written_during_range_query << ", "
           << qstats.num_pages_written_during_range_query << ", "
           << qstats.num_entries_written_during_range_query << ", "

           << qstats.total_vanilla_compaction << ", "
           << qstats.total_rqd_compaction << ", "

           << qstats.entries_count_before_query << ", "
           << qstats.entries_count_after_query << ", "
           << qstats.total_time_taken << std::endl;
        return os;
    }

    std::string getQueryType(QueryType qtype)
    {
        switch (qtype)
        {
        case POINT:
            return "Point";
            break;
        case RANGE:
            return "Range";
            break;
        case INSERT:
            return "Insert";
            break;
        case DELETE:
            return "Delete";
            break;
        case UPDATE:
            return "Update";
            break;
        default:
            return "Invalid";
            break;
        }
    }
}

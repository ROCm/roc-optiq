#pragma once

#include "profiler_hub_interface_types.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <variant>


namespace profiler_hub::missing_types
{
    /// A column descriptor: carries the name and the uniform type for all non-null cells in
    /// this column. Positional index within table_t::columns is the column's identity.
    struct table_column_t
    {
        std::string               name{};  ///< Column identifier.
        profiler_hub_value_type_t type{};  ///< Declared type; all non-null cells must conform.
    };

    using table_column_list_t = std::vector<table_column_t>;

    /// A single cell value. nullptr_t = null (absent); otherwise the held alternative must
    /// conform to the type declared by the corresponding table_column_t. monostate is not
    /// used here — a cell is either a typed value or null, never present-but-empty.
    using table_cell_value_t = std::variant<std::nullptr_t, size_t, double, std::string>;

    struct table_cell_t
    {
        table_cell_value_t value{ nullptr };  ///< Null by default; set to a typed value when populated.
    };

    using table_cell_list_t = std::vector<table_cell_t>;

    /// A single row in a table_t. cells[i] corresponds to columns[i] in the parent table.
    /// A null cell (nullptr_t) is valid for any column type.
    struct table_row_t
    {
        size_t            row_index{};  ///< Zero-based position of this row in the table.
        table_cell_list_t cells{};      ///< One entry per column, positionally aligned.
    };

    using table_row_ptr_t  = std::shared_ptr<table_row_t>;
    using table_row_list_t = std::vector<table_row_t>;

    /// A rectangular, typed table. Column identity (name + type) lives in columns;
    /// cell values in each row are positionally aligned to columns and must conform
    /// to the declared column type unless null.
    struct table_t
    {
        std::string         name{};     ///< Optional display name for the table.
        table_column_list_t columns{};  ///< Ordered column descriptors (name + type).
        table_row_list_t    rows{};     ///< Ordered row set, each row aligned to columns.
        std::string         extdata{};
    };

    using table_ptr_t  = std::shared_ptr<table_t>;
    using table_list_t = std::vector<table_ptr_t>;

    struct event_filter_t
    {
        reader_types::time_window_t         time_window;           ///< Time range filter
        reader_types::pagination_t          pagination;            ///< Limit/offset for chunking
        std::optional<reader_types::sort_t> sort{ std::nullopt };  ///< Sort order

        /// Which event types to include (empty = all)
        std::vector<reader_types::event_type_t> types;
        std::vector<std::string> search_strings;
    };
}

namespace profiler_hub
{

    class trace_instance_t;
    class track_histogram_bucket_t;

// Exception type raised by missing_t methods.
// Carries a human-readable description of the unimplemented feature.
class missing_error_t : public std::runtime_error
{
public:
    explicit missing_error_t(std::string_view description)
    : std::runtime_error(std::string(description))
    {}
};

// Placeholder class whose methods throw missing_error_t.
// Add one method per unimplemented feature; call throw_missing() with a
// clear description of what is absent so callers get an actionable message.
class missing_t
{
public:

    static std::vector<trace_instance_t> get_trace_instances() 
    { 
        throw_missing("multi-trace instances");
        return {};
    }

    static std::unordered_map<size_t, std::string> get_trace_string_table()
    {
        // string table may have trings from different sqlite tables, e.g. rocpd_string and rocpd_info_kernel_symbol
        // so, there are two options for profiler hub:
        // 1) put all the strings into single string table, then, when string id is among requested fields, do proper remapping
        // 2) have multiple methods get_trace_string_table(), get_trace_kernel_symbols_table() to provide original mapping and rely on caller to do remapping
        throw_missing("string table");
        return {};
    }

    static profiler_hub_db_type_t detect_trace(std::string& file_name)
    {
        throw_missing("trace detection");
        return profiler_hub_db_type_t::kDbNotSupported;
    }

    static std::vector<track_histogram_bucket_t> get_track_histogram(profiler_hub::reader_types::track_info_ptr_t & track, size_t histogram_bucket_count)
    {
        throw_missing("track histogram");
        return {};
    }

    static profiler_hub_result_t trim_trace_database(uint64_t timestamp_start, uint64_t timestamp_end) {
        throw_missing("trimming trace database");
    }

    static missing_types::table_ptr_t get_event_table(
        missing_types::event_filter_t& filter,
        size_t start,
        size_t stop
    ) {
        throw_missing("event table");
        return {};
    }

    static missing_types::table_ptr_t get_event_table_for_track(
        profiler_hub::reader_types::track_info_ptr_t& track,
        missing_types::event_filter_t& filter,
        size_t start,
        size_t stop
    ) {
        throw_missing("event table for track");
        return {};
    }

    static void function(std::string_view feature)
    {
        throw_missing(feature);
    }
    static uint64_t integer(std::string_view feature)
    {
        throw_missing(feature);
        return 0;
    }
    static const char * string(std::string_view feature)
    {
        throw_missing(feature);
        return "";
    }

    [[noreturn]] static void check_missing_client(void * client)
    {
        if (client == nullptr)
        {
            throw missing_error_t("fatal error: client trace cannot be null!");
        }
    }

protected:
    // Throws missing_error_t with a standardised message.
    // @param feature – short description of the missing feature or method.
    [[noreturn]] static void throw_missing(std::string_view feature)
    {
        throw missing_error_t("not implemented: " + std::string(feature));
    }


};

class trace_instance_t
{
public:
    trace_instance_t(std::string& file, std::string& guid, uint32_t index) :
        m_file(file),
        m_guid(guid),
        m_index(index) {
    }
    const char* get_file() { return m_file.c_str(); }
    const char* get_guid() { return m_guid.c_str(); }
    uint32_t get_index() { return m_index; }
private:
    std::string m_file;
    std::string m_guid;
    uint32_t m_index;

};

class track_histogram_bucket_t 
{
public:
    track_histogram_bucket_t(uint32_t bucket_number, uint32_t events_count, double bucket_value) :
        m_bucket_number(bucket_number), m_events_count(events_count), m_bucket_value(bucket_value) {
    };
    uint32_t get_bucket_number() { return m_bucket_number; }
    uint32_t get_events_count() { return m_events_count; }
    double get_bucket_value() { return m_bucket_value; }
private:
    uint32_t m_bucket_number;
    uint32_t m_events_count;
    double m_bucket_value;
};

}  // namespace profiler_hub

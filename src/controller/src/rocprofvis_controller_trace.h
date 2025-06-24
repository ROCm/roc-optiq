// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class Track;
class Graph;
class Timeline;
class Event;
class Table;
class SystemTable;
class Plot;

class ComputeTrace;

class Process : public Handle
{
public:
    Process();
    virtual ~Process();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) final;

private:
    std::string m_command;
    std::string m_environment;
    std::string m_ext_data;
    double      m_init_time;
    double      m_finish_time;
    double      m_start_time;
    double      m_end_time;
    uint32_t    m_id;
    uint32_t    m_node_id;
};

class Processor : public Handle
{
public:
    Processor();
    virtual ~Processor();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) final;

private:
    std::string m_name;
    std::string m_model_name;
    std::string m_user_name;
    std::string m_product_name;
    std::string m_vendor_name;
    std::string m_ext_data;
    std::string m_uuid;
    std::string m_type;
    uint32_t    m_id;
    uint32_t    m_node_id;
    uint32_t    m_type_index;
};

class Node : public Handle
{
public:
    Node();
    virtual ~Node();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    std::vector<Processor*> m_processors;
    std::vector<Process*> m_processes;
    std::string m_machine_id;
    std::string m_host_name;
    std::string m_domain_name;
    std::string m_os_name;
    std::string m_os_release;
    std::string m_os_version;
    std::string m_hardware_name;
    uint32_t m_id;
};

class Trace : public Handle
{
public:
    Trace();

    virtual ~Trace();

    rocprofvis_result_t Load(char const* const filename, Future& future);

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& graph, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_result_t AsyncFetch(Event& event, Future& future, Array& array,
                                   rocprofvis_property_t property);

    rocprofvis_result_t AsyncFetch(Table& table, Future& future, Array& array,
                                   uint64_t index, uint64_t count);

    rocprofvis_result_t AsyncFetch(Table& table, Arguments& args, Future& future,
                                   Array& array);

    rocprofvis_result_t AsyncFetch(Plot& plot, Arguments& args, Future& future,
                                   Array& array);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    std::vector<Track*> m_tracks;
    std::vector<Node*> m_nodes;
    uint64_t m_id;
    Timeline* m_timeline;
    SystemTable* m_event_table;
    SystemTable* m_sample_table;
    rocprofvis_dm_trace_t m_dm_handle;

    ComputeTrace* m_compute_trace;

private:
#ifdef JSON_SUPPORT
    rocprofvis_result_t LoadJson(char const* const filename);
#endif
    rocprofvis_result_t LoadRocpd(char const* const filename);
};

}
}

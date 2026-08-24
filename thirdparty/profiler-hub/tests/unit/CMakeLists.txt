# Copyright (c) Advanced Micro Devices, Inc.
# SPDX-License-Identifier: MIT

include(gtest)

set(UNIT_TEST_SOURCES
    writer_test.cpp
    reader_test.cpp
    insert_query_builders_test.cpp
    table_insert_query_test.cpp
    table_select_query_test.cpp
    database_test.cpp
    entity_utility_test.cpp
    json_serializer_test.cpp
    mock_sqlite3_test.cpp
    backend_over_mock_test.cpp
)

add_executable(profiler-hub_unit_tests ${UNIT_TEST_SOURCES})

target_link_libraries(
    profiler-hub_unit_tests
    PRIVATE
        profiler-hub
        profiler-hub_queries
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
        profiler-hub-sqlite3
        spdlog::spdlog
        nlohmann_json::nlohmann_json
)

# v3 fixture (bundled).
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/rocpd.db
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd.db
    COPYONLY
)
set(ROCPD_DB_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd.db)

# Real v4.0 fixture (schema-verified genuine v4.0; committed as rocpd_v4.db).
# Covers cpu_thread/gpu_queue/dma interval tracks + region->kernel_dispatch/
# memory_copy flows. It has no rocpd_sample rows, so it does NOT exercise the
# scalar/counter path -- that gap is filled by the synthetic fixture below.
configure_file(
    ${CMAKE_CURRENT_LIST_DIR}/rocpd_v4.db
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4.db
    COPYONLY
)
set(ROCPD_DB_V4_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4.db)

# Synthetic v4.0 counter/scalar fixture, generated at build time from committed
# SQL (reproducible, reviewable in diff, no opaque binary). The canonical v4.0
# schema DDL and the hand-authored counter data live in fixtures/; the {{uuid}}/
# {{guid}} placeholders are substituted exactly as get_schema_query() does, then
# fed through the sqlite3 CLI. Regenerate manually with, e.g.:
#   uuid=00000000000000000000000000c00001
#   sed -e "s/{{uuid}}/_$uuid/g" -e "s/{{guid}}/$uuid/g" \
#       fixtures/rocpd_v4.0_tables.sql fixtures/rocpd_v4_counter_data.sql \
#     | sqlite3 rocpd_v4_counter.db
find_program(SQLITE3_EXECUTABLE sqlite3)
if(NOT SQLITE3_EXECUTABLE)
    message(
        FATAL_ERROR
        "sqlite3 CLI not found; it is required to build the synthetic v4.0 "
        "counter test fixture (tests/unit/fixtures/rocpd_v4_counter_data.sql)."
    )
endif()

set(_v4c_uuid "00000000000000000000000000c00001")
set(_v4c_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_counter_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4c_schema)
file(
    READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_counter_data.sql
    _v4c_data
)
string(REPLACE "{{uuid}}" "_${_v4c_uuid}" _v4c_schema "${_v4c_schema}")
string(REPLACE "{{guid}}" "${_v4c_uuid}" _v4c_schema "${_v4c_schema}")
string(REPLACE "{{uuid}}" "_${_v4c_uuid}" _v4c_data "${_v4c_data}")
string(REPLACE "{{guid}}" "${_v4c_uuid}" _v4c_data "${_v4c_data}")
file(WRITE ${_v4c_gen_sql} "${_v4c_schema}\n${_v4c_data}\n")

set(ROCPD_DB_V4_COUNTER_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_counter.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_COUNTER_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_COUNTER_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_COUNTER_PATH}
        ".read ${_v4c_gen_sql}"
    DEPENDS
        ${_v4c_gen_sql}
        ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
        ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_counter_data.sql
    COMMENT "Generating synthetic v4.0 counter/scalar test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_counter_fixture
    DEPENDS ${ROCPD_DB_V4_COUNTER_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_counter_fixture)

# Synthetic v3 edge-matrix fixture, generated at build time from committed SQL.
# Unlike the v4.0 counter fixture (which had to bundle its own schema copy because
# no canonical v4.0 DDL lives in-tree), the v3 schema IS canonical at
# source/data_storage/schema/rocpd_tables.sql, so we reference it directly to
# avoid a drifting duplicate. The {{uuid}}/{{guid}} placeholders are substituted
# exactly as get_schema_query() does, then fed through the sqlite3 CLI. The db has
# no rocpd_timestamp table, so the reader selects the v3 backend. Regenerate with:
#   uuid=00000000000000000000000000e30003
#   sed -e "s/{{uuid}}/_$uuid/g" -e "s/{{guid}}/$uuid/g" \
#       ../../source/data_storage/schema/rocpd_tables.sql \
#       fixtures/rocpd_v3_edge_data.sql | sqlite3 rocpd_v3_edge.db
set(_v3e_uuid "00000000000000000000000000e30003")
set(_v3e_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3e_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_edge_data.sql)
set(_v3e_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_edge_gen.sql)
file(READ ${_v3e_schema_src} _v3e_schema)
file(READ ${_v3e_data_src} _v3e_data)
# The canonical schema ends with an INSERT INTO rocpd_metadata that uses
# double-quoted string literals. The modern sqlite3 CLI rejects double-quoted
# strings in DML (only DDL keeps the legacy behavior), and the reader never reads
# metadata anyway (v3 is detected by the ABSENCE of rocpd_timestamp). Strip that
# single trailing DML statement so the CLI is fed pure DDL, matching how the v4
# fixture schema is pure DDL. [^;] spans newlines in CMake regex.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3e_schema
    "${_v3e_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3e_uuid}" _v3e_schema "${_v3e_schema}")
string(REPLACE "{{guid}}" "${_v3e_uuid}" _v3e_schema "${_v3e_schema}")
string(REPLACE "{{uuid}}" "_${_v3e_uuid}" _v3e_data "${_v3e_data}")
string(REPLACE "{{guid}}" "${_v3e_uuid}" _v3e_data "${_v3e_data}")
file(WRITE ${_v3e_gen_sql} "${_v3e_schema}\n${_v3e_data}\n")

set(ROCPD_DB_V3_EDGE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_edge.db)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_EDGE_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_EDGE_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_EDGE_PATH} ".read ${_v3e_gen_sql}"
    DEPENDS ${_v3e_gen_sql} ${_v3e_schema_src} ${_v3e_data_src}
    COMMENT "Generating synthetic v3 edge-matrix test fixture"
    VERBATIM
)
add_custom_target(profiler-hub_v3_edge_fixture DEPENDS ${ROCPD_DB_V3_EDGE_PATH})
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_edge_fixture)

# Synthetic v3 dma-by-destination-agent fixture, generated at build time from
# committed SQL against the canonical v3 schema (same mechanism as the edge
# fixture above). Encodes the crossed 2-agent x 2-stream x 12 = 48 memory_copy
# pattern that proves dma tracks partition by dst_agent_id, not stream_id.
set(_v3d_uuid "00000000000000000000000000d30003")
set(_v3d_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3d_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_dma_agent_data.sql)
set(_v3d_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_dma_agent_gen.sql)
file(READ ${_v3d_schema_src} _v3d_schema)
file(READ ${_v3d_data_src} _v3d_data)
# Strip the trailing rocpd_metadata DML (double-quoted string literals the modern
# sqlite3 CLI rejects in DML); the reader never reads metadata. See the edge block.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3d_schema
    "${_v3d_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3d_uuid}" _v3d_schema "${_v3d_schema}")
string(REPLACE "{{guid}}" "${_v3d_uuid}" _v3d_schema "${_v3d_schema}")
string(REPLACE "{{uuid}}" "_${_v3d_uuid}" _v3d_data "${_v3d_data}")
string(REPLACE "{{guid}}" "${_v3d_uuid}" _v3d_data "${_v3d_data}")
file(WRITE ${_v3d_gen_sql} "${_v3d_schema}\n${_v3d_data}\n")

set(ROCPD_DB_V3_DMA_AGENT_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_dma_agent.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_DMA_AGENT_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_DMA_AGENT_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_DMA_AGENT_PATH}
        ".read ${_v3d_gen_sql}"
    DEPENDS ${_v3d_gen_sql} ${_v3d_schema_src} ${_v3d_data_src}
    COMMENT "Generating synthetic v3 dma-by-destination-agent test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_dma_agent_fixture
    DEPENDS ${ROCPD_DB_V3_DMA_AGENT_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_dma_agent_fixture)

# Synthetic v3 track-shapes fixture (task 044), generated at build time from
# committed SQL against the canonical v3 schema (same mechanism as the edge
# fixture above). Carries one track of each per-track_type x schema switch arm
# the other v3 fixtures leave dark in get_track_stats / get_interval_track: dma
# queue-only / agent-only / queue+agent, memory queue+agent / queue-only /
# neither, and a cpu_thread SAMPLE track. See fixtures/rocpd_v3_track_shapes_data.sql.
set(_v3s_uuid "00000000000000000000000000730003")
set(_v3s_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3s_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_track_shapes_data.sql)
set(_v3s_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_track_shapes_gen.sql)
file(READ ${_v3s_schema_src} _v3s_schema)
file(READ ${_v3s_data_src} _v3s_data)
# Strip the trailing rocpd_metadata DML (double-quoted string literals the modern
# sqlite3 CLI rejects in DML); the reader never reads metadata. See the edge block.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3s_schema
    "${_v3s_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3s_uuid}" _v3s_schema "${_v3s_schema}")
string(REPLACE "{{guid}}" "${_v3s_uuid}" _v3s_schema "${_v3s_schema}")
string(REPLACE "{{uuid}}" "_${_v3s_uuid}" _v3s_data "${_v3s_data}")
string(REPLACE "{{guid}}" "${_v3s_uuid}" _v3s_data "${_v3s_data}")
file(WRITE ${_v3s_gen_sql} "${_v3s_schema}\n${_v3s_data}\n")

set(ROCPD_DB_V3_TRACK_SHAPES_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_track_shapes.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_TRACK_SHAPES_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_TRACK_SHAPES_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_TRACK_SHAPES_PATH}
        ".read ${_v3s_gen_sql}"
    DEPENDS ${_v3s_gen_sql} ${_v3s_schema_src} ${_v3s_data_src}
    COMMENT "Generating synthetic v3 track-shapes test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_track_shapes_fixture
    DEPENDS ${ROCPD_DB_V3_TRACK_SHAPES_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_track_shapes_fixture)

# Synthetic v3 missing-metadata naming-fallback fixture (task 048), generated at
# build time from committed SQL against the canonical v3 schema (same mechanism as
# the edge fixture above). Its stream has no name, one region's thread is entirely
# absent, one region's thread has a NULL name, and one agent has a NULL type_index,
# so the Tier-3 naming fallbacks ("Stream <id>" reader_impl.cpp:715, "Thread"
# :772, "Thread <tid>" :768) and the corrupted-agent drop (:252) all fire. See
# fixtures/rocpd_v3_missing_meta_data.sql.
set(_v3mm_uuid "00000000000000000000000000730048")
set(_v3mm_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3mm_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_missing_meta_data.sql)
set(_v3mm_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_missing_meta_gen.sql)
file(READ ${_v3mm_schema_src} _v3mm_schema)
file(READ ${_v3mm_data_src} _v3mm_data)
# Strip the trailing rocpd_metadata DML (double-quoted string literals the modern
# sqlite3 CLI rejects in DML); the reader never reads metadata. See the edge block.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3mm_schema
    "${_v3mm_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3mm_uuid}" _v3mm_schema "${_v3mm_schema}")
string(REPLACE "{{guid}}" "${_v3mm_uuid}" _v3mm_schema "${_v3mm_schema}")
string(REPLACE "{{uuid}}" "_${_v3mm_uuid}" _v3mm_data "${_v3mm_data}")
string(REPLACE "{{guid}}" "${_v3mm_uuid}" _v3mm_data "${_v3mm_data}")
file(WRITE ${_v3mm_gen_sql} "${_v3mm_schema}\n${_v3mm_data}\n")

set(ROCPD_DB_V3_MISSING_META_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_missing_meta.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_MISSING_META_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_MISSING_META_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_MISSING_META_PATH}
        ".read ${_v3mm_gen_sql}"
    DEPENDS ${_v3mm_gen_sql} ${_v3mm_schema_src} ${_v3mm_data_src}
    COMMENT "Generating synthetic v3 missing-metadata test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_missing_meta_fixture
    DEPENDS ${ROCPD_DB_V3_MISSING_META_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_missing_meta_fixture)

# Synthetic v3 flow-ordering / tie-break fixture (task 049). Same mechanism as the
# missing-meta block above. Cliques crafted so two flow-builder ordering branches
# fire that no committed fixture reaches: get_flows() equal-start deterministic
# tie-break (reader_impl.cpp:2841-2842) and the get_flows_in_window decimation-sort
# final tie-break `a.dest < b.dest` (:3033). See fixtures/rocpd_v3_flow_order_data.sql.
set(_v3fo_uuid "00000000000000000000000000730049")
set(_v3fo_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3fo_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_flow_order_data.sql)
set(_v3fo_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_flow_order_gen.sql)
file(READ ${_v3fo_schema_src} _v3fo_schema)
file(READ ${_v3fo_data_src} _v3fo_data)
# Strip the trailing rocpd_metadata DML (double-quoted string literals the modern
# sqlite3 CLI rejects in DML); the reader never reads metadata. See the edge block.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3fo_schema
    "${_v3fo_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3fo_uuid}" _v3fo_schema "${_v3fo_schema}")
string(REPLACE "{{guid}}" "${_v3fo_uuid}" _v3fo_schema "${_v3fo_schema}")
string(REPLACE "{{uuid}}" "_${_v3fo_uuid}" _v3fo_data "${_v3fo_data}")
string(REPLACE "{{guid}}" "${_v3fo_uuid}" _v3fo_data "${_v3fo_data}")
file(WRITE ${_v3fo_gen_sql} "${_v3fo_schema}\n${_v3fo_data}\n")

set(ROCPD_DB_V3_FLOW_ORDER_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_flow_order.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_FLOW_ORDER_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_FLOW_ORDER_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_FLOW_ORDER_PATH}
        ".read ${_v3fo_gen_sql}"
    DEPENDS ${_v3fo_gen_sql} ${_v3fo_schema_src} ${_v3fo_data_src}
    COMMENT "Generating synthetic v3 flow-ordering test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_flow_order_fixture
    DEPENDS ${ROCPD_DB_V3_FLOW_ORDER_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_flow_order_fixture)

# Synthetic v3 summary fixture (task 050). Same mechanism as the missing-meta /
# flow-order blocks above. Two distinct kernel names + two distinct region names with
# hand-chosen durations so get_kernel_summary / get_region_summary GROUP-BY-name
# aggregates are by-construction oracles. See fixtures/rocpd_v3_summary_data.sql.
set(_v3sum_uuid "00000000000000000000000000730050")
set(_v3sum_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3sum_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_summary_data.sql)
set(_v3sum_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_summary_gen.sql)
file(READ ${_v3sum_schema_src} _v3sum_schema)
file(READ ${_v3sum_data_src} _v3sum_data)
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3sum_schema
    "${_v3sum_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3sum_uuid}" _v3sum_schema "${_v3sum_schema}")
string(REPLACE "{{guid}}" "${_v3sum_uuid}" _v3sum_schema "${_v3sum_schema}")
string(REPLACE "{{uuid}}" "_${_v3sum_uuid}" _v3sum_data "${_v3sum_data}")
string(REPLACE "{{guid}}" "${_v3sum_uuid}" _v3sum_data "${_v3sum_data}")
file(WRITE ${_v3sum_gen_sql} "${_v3sum_schema}\n${_v3sum_data}\n")

set(ROCPD_DB_V3_SUMMARY_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_summary.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_SUMMARY_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_SUMMARY_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_SUMMARY_PATH}
        ".read ${_v3sum_gen_sql}"
    DEPENDS ${_v3sum_gen_sql} ${_v3sum_schema_src} ${_v3sum_data_src}
    COMMENT "Generating synthetic v3 summary test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_summary_fixture
    DEPENDS ${ROCPD_DB_V3_SUMMARY_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_summary_fixture)

# Synthetic v4.0 summary fixture (task 050). Same oracle as the v3 summary fixture but
# durations come through the rocpd_timestamp spine, exercising the v4 backend of
# get_kernel_summary / get_region_summary. See fixtures/rocpd_v4_summary_data.sql.
set(_v4sum_uuid "00000000000000000000000000740050")
set(_v4sum_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_summary_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4sum_schema)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_summary_data.sql _v4sum_data)
string(REPLACE "{{uuid}}" "_${_v4sum_uuid}" _v4sum_schema "${_v4sum_schema}")
string(REPLACE "{{guid}}" "${_v4sum_uuid}" _v4sum_schema "${_v4sum_schema}")
string(REPLACE "{{uuid}}" "_${_v4sum_uuid}" _v4sum_data "${_v4sum_data}")
string(REPLACE "{{guid}}" "${_v4sum_uuid}" _v4sum_data "${_v4sum_data}")
file(WRITE ${_v4sum_gen_sql} "${_v4sum_schema}\n${_v4sum_data}\n")

set(ROCPD_DB_V4_SUMMARY_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_summary.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_SUMMARY_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_SUMMARY_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_SUMMARY_PATH}
        ".read ${_v4sum_gen_sql}"
    DEPENDS
        ${_v4sum_gen_sql}
        ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
        ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_summary_data.sql
    COMMENT "Generating synthetic v4.0 summary test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_summary_fixture
    DEPENDS ${ROCPD_DB_V4_SUMMARY_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_summary_fixture)

# Synthetic v3 flow-clique fixture, generated at build time from committed SQL
# against the canonical v3 schema (same mechanism as the edge fixture above).
# Encodes non-flat stack cliques so get_flows() emits region->region + same-type
# sibling edges, with colliding endpoint ids that only the event_type tags
# disambiguate. See fixtures/rocpd_v3_clique_data.sql for the flow oracle.
set(_v3c_uuid "00000000000000000000000000c30003")
set(_v3c_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3c_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_clique_data.sql)
set(_v3c_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_clique_gen.sql)
file(READ ${_v3c_schema_src} _v3c_schema)
file(READ ${_v3c_data_src} _v3c_data)
# Strip the trailing rocpd_metadata DML (double-quoted string literals the modern
# sqlite3 CLI rejects in DML); the reader never reads metadata. See the edge block.
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3c_schema
    "${_v3c_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3c_uuid}" _v3c_schema "${_v3c_schema}")
string(REPLACE "{{guid}}" "${_v3c_uuid}" _v3c_schema "${_v3c_schema}")
string(REPLACE "{{uuid}}" "_${_v3c_uuid}" _v3c_data "${_v3c_data}")
string(REPLACE "{{guid}}" "${_v3c_uuid}" _v3c_data "${_v3c_data}")
file(WRITE ${_v3c_gen_sql} "${_v3c_schema}\n${_v3c_data}\n")

set(ROCPD_DB_V3_CLIQUE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_clique.db)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_CLIQUE_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_CLIQUE_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_CLIQUE_PATH} ".read ${_v3c_gen_sql}"
    DEPENDS ${_v3c_gen_sql} ${_v3c_schema_src} ${_v3c_data_src}
    COMMENT "Generating synthetic v3 flow-clique test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_clique_fixture
    DEPENDS ${ROCPD_DB_V3_CLIQUE_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_clique_fixture)

# Synthetic v3 kernel_dispatch_pmc fixture, generated at build time from
# committed SQL against the canonical v3 schema (same mechanism as the edge
# fixture above). Encodes two PMC types across three kernel_dispatch rows so
# both track discovery and get_interval_track() can be asserted on real values.
set(_v3p_uuid "0000000000000000000000000b030003")
set(_v3p_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3p_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_kd_pmc_data.sql)
set(_v3p_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_kd_pmc_gen.sql)
file(READ ${_v3p_schema_src} _v3p_schema)
file(READ ${_v3p_data_src} _v3p_data)
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3p_schema
    "${_v3p_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3p_uuid}" _v3p_schema "${_v3p_schema}")
string(REPLACE "{{guid}}" "${_v3p_uuid}" _v3p_schema "${_v3p_schema}")
string(REPLACE "{{uuid}}" "_${_v3p_uuid}" _v3p_data "${_v3p_data}")
string(REPLACE "{{guid}}" "${_v3p_uuid}" _v3p_data "${_v3p_data}")
file(WRITE ${_v3p_gen_sql} "${_v3p_schema}\n${_v3p_data}\n")

set(ROCPD_DB_V3_KD_PMC_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_kd_pmc.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_KD_PMC_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_KD_PMC_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_KD_PMC_PATH}
        ".read ${_v3p_gen_sql}"
    DEPENDS ${_v3p_gen_sql} ${_v3p_schema_src} ${_v3p_data_src}
    COMMENT "Generating synthetic v3 kernel_dispatch_pmc test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_kd_pmc_fixture
    DEPENDS ${ROCPD_DB_V3_KD_PMC_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_kd_pmc_fixture)

# Synthetic v4.0 kernel_dispatch_pmc fixture, generated at build time from
# committed SQL against the canonical v4.0 schema. Mirrors the v3 fixture
# data shape so both backends can be regression-tested on the same assertions.
set(_v4p_uuid "0000000000000000000000000b040004")
set(_v4p_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_kd_pmc_data.sql)
set(_v4p_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_kd_pmc_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4p_schema)
file(READ ${_v4p_data_src} _v4p_data)
string(REPLACE "{{uuid}}" "_${_v4p_uuid}" _v4p_schema "${_v4p_schema}")
string(REPLACE "{{guid}}" "${_v4p_uuid}" _v4p_schema "${_v4p_schema}")
string(REPLACE "{{uuid}}" "_${_v4p_uuid}" _v4p_data "${_v4p_data}")
string(REPLACE "{{guid}}" "${_v4p_uuid}" _v4p_data "${_v4p_data}")
file(WRITE ${_v4p_gen_sql} "${_v4p_schema}\n${_v4p_data}\n")

set(ROCPD_DB_V4_KD_PMC_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_kd_pmc.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_KD_PMC_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_KD_PMC_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_KD_PMC_PATH}
        ".read ${_v4p_gen_sql}"
    DEPENDS ${_v4p_gen_sql}
            ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
            ${_v4p_data_src}
    COMMENT "Generating synthetic v4.0 kernel_dispatch_pmc test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_kd_pmc_fixture
    DEPENDS ${ROCPD_DB_V4_KD_PMC_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_kd_pmc_fixture)

# Synthetic v3 memory_activity fixture (task 012B).
set(_v3ma_uuid "0000000000000000000000000c010001")
set(_v3ma_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3ma_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_mem_activity_data.sql)
set(_v3ma_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_mem_activity_gen.sql)
file(READ ${_v3ma_schema_src} _v3ma_schema)
file(READ ${_v3ma_data_src} _v3ma_data)
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3ma_schema
    "${_v3ma_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3ma_uuid}" _v3ma_schema "${_v3ma_schema}")
string(REPLACE "{{guid}}" "${_v3ma_uuid}" _v3ma_schema "${_v3ma_schema}")
string(REPLACE "{{uuid}}" "_${_v3ma_uuid}" _v3ma_data "${_v3ma_data}")
string(REPLACE "{{guid}}" "${_v3ma_uuid}" _v3ma_data "${_v3ma_data}")
file(WRITE ${_v3ma_gen_sql} "${_v3ma_schema}\n${_v3ma_data}\n")

set(ROCPD_DB_V3_MEM_ACTIVITY_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_mem_activity.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_MEM_ACTIVITY_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_MEM_ACTIVITY_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_MEM_ACTIVITY_PATH}
        ".read ${_v3ma_gen_sql}"
    DEPENDS ${_v3ma_gen_sql} ${_v3ma_schema_src} ${_v3ma_data_src}
    COMMENT "Generating synthetic v3 memory_activity test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_mem_activity_fixture
    DEPENDS ${ROCPD_DB_V3_MEM_ACTIVITY_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_mem_activity_fixture)

# Synthetic v3 memory_activity time-window straddle fixture (task 045).
set(_v3maw_uuid "00000000000000000000000000730045")
set(_v3maw_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3maw_data_src
    ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_mem_activity_window_data.sql
)
set(_v3maw_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_mem_activity_window_gen.sql)
file(READ ${_v3maw_schema_src} _v3maw_schema)
file(READ ${_v3maw_data_src} _v3maw_data)
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3maw_schema
    "${_v3maw_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3maw_uuid}" _v3maw_schema "${_v3maw_schema}")
string(REPLACE "{{guid}}" "${_v3maw_uuid}" _v3maw_schema "${_v3maw_schema}")
string(REPLACE "{{uuid}}" "_${_v3maw_uuid}" _v3maw_data "${_v3maw_data}")
string(REPLACE "{{guid}}" "${_v3maw_uuid}" _v3maw_data "${_v3maw_data}")
file(WRITE ${_v3maw_gen_sql} "${_v3maw_schema}\n${_v3maw_data}\n")

set(ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_mem_activity_window.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH}
        ".read ${_v3maw_gen_sql}"
    DEPENDS ${_v3maw_gen_sql} ${_v3maw_schema_src} ${_v3maw_data_src}
    COMMENT "Generating synthetic v3 memory_activity time-window test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_mem_activity_window_fixture
    DEPENDS ${ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH}
)
add_dependencies(
    profiler-hub_unit_tests profiler-hub_v3_mem_activity_window_fixture
)

# Synthetic v4.0 memory_activity fixture (task 012B).
set(_v4ma_uuid "0000000000000000000000000c020002")
set(_v4ma_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_mem_activity_data.sql)
set(_v4ma_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_mem_activity_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4ma_schema)
file(READ ${_v4ma_data_src} _v4ma_data)
string(REPLACE "{{uuid}}" "_${_v4ma_uuid}" _v4ma_schema "${_v4ma_schema}")
string(REPLACE "{{guid}}" "${_v4ma_uuid}" _v4ma_schema "${_v4ma_schema}")
string(REPLACE "{{uuid}}" "_${_v4ma_uuid}" _v4ma_data "${_v4ma_data}")
string(REPLACE "{{guid}}" "${_v4ma_uuid}" _v4ma_data "${_v4ma_data}")
file(WRITE ${_v4ma_gen_sql} "${_v4ma_schema}\n${_v4ma_data}\n")

set(ROCPD_DB_V4_MEM_ACTIVITY_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_mem_activity.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_MEM_ACTIVITY_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_MEM_ACTIVITY_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_MEM_ACTIVITY_PATH}
        ".read ${_v4ma_gen_sql}"
    DEPENDS ${_v4ma_gen_sql}
            ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
            ${_v4ma_data_src}
    COMMENT "Generating synthetic v4.0 memory_activity test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_mem_activity_fixture
    DEPENDS ${ROCPD_DB_V4_MEM_ACTIVITY_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_mem_activity_fixture)

# Synthetic v3 ambiguous-pmc fixture (task 014): one ambiguous pmc_id (2 rows per
# event_id) and one clean pmc_id, so both branches of pmc_info_t::ambiguous can
# be asserted on the v3 backend.
set(_v3ap_uuid "0000000000000000000000000a014003")
set(_v3ap_schema_src
    ${CMAKE_SOURCE_DIR}/source/data_storage/schema/rocpd_tables.sql
)
set(_v3ap_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v3_amb_pmc_data.sql)
set(_v3ap_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v3_amb_pmc_gen.sql)
file(READ ${_v3ap_schema_src} _v3ap_schema)
file(READ ${_v3ap_data_src} _v3ap_data)
string(
    REGEX REPLACE
    "INSERT INTO[^;]*rocpd_metadata[^;]*;"
    ""
    _v3ap_schema
    "${_v3ap_schema}"
)
string(REPLACE "{{uuid}}" "_${_v3ap_uuid}" _v3ap_schema "${_v3ap_schema}")
string(REPLACE "{{guid}}" "${_v3ap_uuid}" _v3ap_schema "${_v3ap_schema}")
string(REPLACE "{{uuid}}" "_${_v3ap_uuid}" _v3ap_data "${_v3ap_data}")
string(REPLACE "{{guid}}" "${_v3ap_uuid}" _v3ap_data "${_v3ap_data}")
file(WRITE ${_v3ap_gen_sql} "${_v3ap_schema}\n${_v3ap_data}\n")

set(ROCPD_DB_V3_AMB_PMC_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v3_amb_pmc.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V3_AMB_PMC_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V3_AMB_PMC_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V3_AMB_PMC_PATH}
        ".read ${_v3ap_gen_sql}"
    DEPENDS ${_v3ap_gen_sql} ${_v3ap_schema_src} ${_v3ap_data_src}
    COMMENT "Generating synthetic v3 ambiguous-pmc test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v3_amb_pmc_fixture
    DEPENDS ${ROCPD_DB_V3_AMB_PMC_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v3_amb_pmc_fixture)

# Synthetic v4.0 ambiguous-pmc fixture (task 014): mirrors the v3 fixture shape
# on the v4 backend (rocpd_timestamp spine present for backend detection).
set(_v4ap_uuid "0000000000000000000000000a014004")
set(_v4ap_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_amb_pmc_data.sql)
set(_v4ap_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_amb_pmc_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4ap_schema)
file(READ ${_v4ap_data_src} _v4ap_data)
string(REPLACE "{{uuid}}" "_${_v4ap_uuid}" _v4ap_schema "${_v4ap_schema}")
string(REPLACE "{{guid}}" "${_v4ap_uuid}" _v4ap_schema "${_v4ap_schema}")
string(REPLACE "{{uuid}}" "_${_v4ap_uuid}" _v4ap_data "${_v4ap_data}")
string(REPLACE "{{guid}}" "${_v4ap_uuid}" _v4ap_data "${_v4ap_data}")
file(WRITE ${_v4ap_gen_sql} "${_v4ap_schema}\n${_v4ap_data}\n")

set(ROCPD_DB_V4_AMB_PMC_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_amb_pmc.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_AMB_PMC_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_AMB_PMC_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_AMB_PMC_PATH}
        ".read ${_v4ap_gen_sql}"
    DEPENDS ${_v4ap_gen_sql}
            ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
            ${_v4ap_data_src}
    COMMENT "Generating synthetic v4.0 ambiguous-pmc test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_amb_pmc_fixture
    DEPENDS ${ROCPD_DB_V4_AMB_PMC_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_amb_pmc_fixture)

# Synthetic v4.0 ambiguous-classification fixture (task 018): a single rocpd_track
# row referenced by both rocpd_sample/rocpd_pmc_event (counter set) and
# rocpd_memory_allocate (memory set), forcing the overlap condition in build_v4_tracks().
set(_v4ac_uuid "0000000000000000000000000a018001")
set(_v4ac_data_src ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4_amb_cls_data.sql)
set(_v4ac_gen_sql ${CMAKE_CURRENT_BINARY_DIR}/rocpd_v4_amb_cls_gen.sql)
file(READ ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql _v4ac_schema)
file(READ ${_v4ac_data_src} _v4ac_data)
string(REPLACE "{{uuid}}" "_${_v4ac_uuid}" _v4ac_schema "${_v4ac_schema}")
string(REPLACE "{{guid}}" "${_v4ac_uuid}" _v4ac_schema "${_v4ac_schema}")
string(REPLACE "{{uuid}}" "_${_v4ac_uuid}" _v4ac_data "${_v4ac_data}")
string(REPLACE "{{guid}}" "${_v4ac_uuid}" _v4ac_data "${_v4ac_data}")
file(WRITE ${_v4ac_gen_sql} "${_v4ac_schema}\n${_v4ac_data}\n")

set(ROCPD_DB_V4_AMB_CLS_PATH
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/rocpd_v4_amb_cls.db
)
add_custom_command(
    OUTPUT ${ROCPD_DB_V4_AMB_CLS_PATH}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    COMMAND ${CMAKE_COMMAND} -E rm -f ${ROCPD_DB_V4_AMB_CLS_PATH}
    COMMAND
        ${SQLITE3_EXECUTABLE} ${ROCPD_DB_V4_AMB_CLS_PATH}
        ".read ${_v4ac_gen_sql}"
    DEPENDS ${_v4ac_gen_sql}
            ${CMAKE_CURRENT_LIST_DIR}/fixtures/rocpd_v4.0_tables.sql
            ${_v4ac_data_src}
    COMMENT "Generating synthetic v4.0 ambiguous-classification test fixture"
    VERBATIM
)
add_custom_target(
    profiler-hub_v4_amb_cls_fixture
    DEPENDS ${ROCPD_DB_V4_AMB_CLS_PATH}
)
add_dependencies(profiler-hub_unit_tests profiler-hub_v4_amb_cls_fixture)

target_compile_definitions(
    profiler-hub_unit_tests
    PUBLIC
        ROCPD_DB_PATH="${ROCPD_DB_PATH}"
        ROCPD_DB_V4_PATH="${ROCPD_DB_V4_PATH}"
        ROCPD_DB_V4_COUNTER_PATH="${ROCPD_DB_V4_COUNTER_PATH}"
        ROCPD_DB_V3_EDGE_PATH="${ROCPD_DB_V3_EDGE_PATH}"
        ROCPD_DB_V3_DMA_AGENT_PATH="${ROCPD_DB_V3_DMA_AGENT_PATH}"
        ROCPD_DB_V3_TRACK_SHAPES_PATH="${ROCPD_DB_V3_TRACK_SHAPES_PATH}"
        ROCPD_DB_V3_MISSING_META_PATH="${ROCPD_DB_V3_MISSING_META_PATH}"
        ROCPD_DB_V3_FLOW_ORDER_PATH="${ROCPD_DB_V3_FLOW_ORDER_PATH}"
        ROCPD_DB_V3_CLIQUE_PATH="${ROCPD_DB_V3_CLIQUE_PATH}"
        ROCPD_DB_V3_KD_PMC_PATH="${ROCPD_DB_V3_KD_PMC_PATH}"
        ROCPD_DB_V4_KD_PMC_PATH="${ROCPD_DB_V4_KD_PMC_PATH}"
        ROCPD_DB_V3_MEM_ACTIVITY_PATH="${ROCPD_DB_V3_MEM_ACTIVITY_PATH}"
        ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH="${ROCPD_DB_V3_MEM_ACTIVITY_WINDOW_PATH}"
        ROCPD_DB_V4_MEM_ACTIVITY_PATH="${ROCPD_DB_V4_MEM_ACTIVITY_PATH}"
        ROCPD_DB_V3_AMB_PMC_PATH="${ROCPD_DB_V3_AMB_PMC_PATH}"
        ROCPD_DB_V4_AMB_PMC_PATH="${ROCPD_DB_V4_AMB_PMC_PATH}"
        ROCPD_DB_V4_AMB_CLS_PATH="${ROCPD_DB_V4_AMB_CLS_PATH}"
        ROCPD_DB_V3_SUMMARY_PATH="${ROCPD_DB_V3_SUMMARY_PATH}"
        ROCPD_DB_V4_SUMMARY_PATH="${ROCPD_DB_V4_SUMMARY_PATH}"
)

target_include_directories(
    profiler-hub_unit_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/source
        ${CMAKE_SOURCE_DIR}/source/common
        ${CMAKE_SOURCE_DIR}/source/data_storage
        ${CMAKE_SOURCE_DIR}/source/queries
)

# backend_over_mock_test.cpp includes sqlite_backend_impl.hpp directly to
# instantiate database_backend over the mock policy, so this target needs
# the same schema configuration as profiler-hub-objects (see top-level
# CMakeLists.txt) for the impl header to resolve the correct branch.
if(USE_SCHEMA_FROM_ROCPROFILER_SDK_ROCPD)
    target_link_libraries(
        profiler-hub_unit_tests
        PRIVATE rocprofiler-sdk-rocpd::rocprofiler-sdk-rocpd
    )

    target_compile_definitions(
        profiler-hub_unit_tests
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:USE_SCHEMA_FROM_ROCPROFILER_SDK_ROCPD=1>
    )
else()
    target_include_directories(
        profiler-hub_unit_tests
        PRIVATE ${SQL_SCHEMA_BINARY_DIR}
    )

    target_compile_definitions(
        profiler-hub_unit_tests
        PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:USE_SCHEMA_FROM_ROCPROFILER_SDK_ROCPD=0>
    )
endif()

target_compile_features(profiler-hub_unit_tests PRIVATE cxx_std_20)

if(PROFILER_HUB_ENABLE_COVERAGE)
    profiler_hub_add_coverage_flags(profiler-hub_unit_tests)
endif()

gtest_discover_tests(
    profiler-hub_unit_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTIES LABELS "unit"
)

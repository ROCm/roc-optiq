// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Unit tests for the header-only ID/packing helpers used across the view:
//  - RequestIdBuilder / IdGenerator (rocprofvis_requests.h): packed request
//    IDs and the monotonic client-ID source. AGENTS.md flags "inventing your
//    own request ID" as a pitfall, so the encoding is worth pinning down.
//  - TopologyId / TraceEventId bitfield unions (rocprofvis_model_types.h).
//  - MetricId / MetricIdHash (compute model types): string form, ordering and
//    the category/table key packing.

#include "rocprofvis_model_types.h"
#include "rocprofvis_requests.h"
#ifdef COMPUTE_UI_SUPPORT
#include "compute/rocprofvis_compute_model_types.h"
#endif

#include <catch2/catch_test_macros.hpp>

#include <cstdint>

using namespace RocProfVis::View;

TEST_CASE("RequestIdBuilder packs request type and track fields", "[ids][requests]")
{
    const uint32_t    track_id    = 0x01234567u;
    const uint16_t    chunk_index = 0xABCDu;
    const uint8_t     group_id    = 0x12u;
    const RequestType type        = RequestType::kFetchTrack;

    const uint64_t id =
        RequestIdBuilder::MakeTrackDataRequestId(track_id, chunk_index, group_id, type);

    // The low 32 bits are the track id.
    CHECK(static_cast<uint32_t>(id & 0xFFFFFFFFull) == track_id);

    // The next 16 bits are the chunk index.
    const uint16_t decoded_chunk = static_cast<uint16_t>(
        (id >> RequestIdBuilder::TRACK_CHUNK_OFFSET_BITS) & 0xFFFFull);
    CHECK(decoded_chunk == chunk_index);

    // The next 8 bits are the group id.
    const uint8_t decoded_group = static_cast<uint8_t>(
        (id >> RequestIdBuilder::TRACK_GROUP_OFFSET_BITS) & 0xFFull);
    CHECK(decoded_group == group_id);

    // The top bits are the request type.
    const uint64_t decoded_type = id >> RequestIdBuilder::REQUEST_TYPE_OFFSET_BITS;
    CHECK(decoded_type == static_cast<uint64_t>(type));
}

TEST_CASE("RequestIdBuilder::MakeRequestId only encodes the request type",
          "[ids][requests]")
{
    const uint64_t id = RequestIdBuilder::MakeRequestId(RequestType::kFetchSummary);
    CHECK((id >> RequestIdBuilder::REQUEST_TYPE_OFFSET_BITS) ==
          static_cast<uint64_t>(RequestType::kFetchSummary));
    // No track/chunk/group payload.
    CHECK((id & ((1ull << RequestIdBuilder::REQUEST_TYPE_OFFSET_BITS) - 1)) == 0ull);
}

TEST_CASE("RequestIdBuilder::MakeClientRequestId round-trips the client id",
          "[ids][requests]")
{
    const uint64_t client_id = 0xDEAD;
    const uint64_t id =
        RequestIdBuilder::MakeClientRequestId(RequestType::kFetchGraph, client_id);

    CHECK((id >> RequestIdBuilder::REQUEST_TYPE_OFFSET_BITS) ==
          static_cast<uint64_t>(RequestType::kFetchGraph));
    CHECK((id & ((1ull << RequestIdBuilder::REQUEST_TYPE_OFFSET_BITS) - 1)) == client_id);
}

TEST_CASE("IdGenerator produces strictly increasing unique ids", "[ids][requests]")
{
    IdGenerator& gen = IdGenerator::GetInstance();
    const uint64_t a = gen.GenerateId();
    const uint64_t b = gen.GenerateId();
    const uint64_t c = gen.GenerateId();
    CHECK(b == a + 1);
    CHECK(c == b + 1);
}

TEST_CASE("TopologyId packs id and instance into 54/10 bits", "[ids][topology]")
{
    TopologyId topo{};
    topo.fields.id       = 0x3FFFFFFFFFFFFFull;  // 54 bits set
    topo.fields.instance = 0x3FFu;               // 10 bits set

    // Round-trips through the bitfields.
    CHECK(topo.fields.id == 0x3FFFFFFFFFFFFFull);
    CHECK(topo.fields.instance == 0x3FFu);

    // instance occupies the top 10 bits of the 64-bit value.
    CHECK((topo.value >> 54) == 0x3FFull);
}

TEST_CASE("TraceEventId packs event id, node and op", "[ids][event]")
{
    TraceEventId ev{};
    ev.bitfield.event_id   = 0xABCDEFull;
    ev.bitfield.event_node = 0x7Fu;
    ev.bitfield.event_op   = 0x0Fu;

    CHECK(ev.bitfield.event_id == 0xABCDEFull);
    CHECK(ev.bitfield.event_node == 0x7Fu);
    CHECK(ev.bitfield.event_op == 0x0Fu);

    // The same 64-bit storage is reachable as uuid.
    TraceEventId mirror{};
    mirror.uuid = ev.uuid;
    CHECK(mirror.bitfield.event_id == 0xABCDEFull);
    CHECK(mirror.bitfield.event_op == 0x0Fu);
}

#ifdef COMPUTE_UI_SUPPORT
TEST_CASE("MetricId string form and equality", "[ids][metric]")
{
    MetricId id;
    id.category_id = 3;
    id.table_id    = 1;
    id.entry_id    = 7;
    CHECK(id.ToString() == "3.1.7");

    MetricId same{3, 1, 7};
    MetricId diff{3, 1, 8};
    CHECK(id == same);
    CHECK_FALSE(id == diff);
}

TEST_CASE("MetricId ordering is lexicographic over (category, table, entry)",
          "[ids][metric]")
{
    CHECK(MetricId{1, 0, 0} < MetricId{2, 0, 0});
    CHECK(MetricId{1, 1, 0} < MetricId{1, 2, 0});
    CHECK(MetricId{1, 1, 1} < MetricId{1, 1, 2});
    CHECK_FALSE(MetricId{1, 1, 1} < MetricId{1, 1, 1});
    CHECK_FALSE(MetricId{2, 0, 0} < MetricId{1, 9, 9});
}

TEST_CASE("MetricId table key packs and extracts category/table", "[ids][metric]")
{
    const uint32_t category = 0xAABBCCDDu;
    const uint32_t table    = 0x11223344u;

    const uint64_t key = MetricId::GetTableKey(category, table);
    CHECK(MetricId::ExtractCategoryId(key) == category);
    CHECK(MetricId::ExtractTableId(key) == table);

    MetricId id{category, table, 0};
    CHECK(id.GetTableKey() == key);
}

TEST_CASE("MetricIdHash is stable and distinguishes distinct ids", "[ids][metric]")
{
    MetricIdHash hash;
    MetricId     a{1, 2, 3};
    MetricId     b{1, 2, 3};
    MetricId     c{3, 2, 1};
    CHECK(hash(a) == hash(b));
    CHECK(hash(a) != hash(c));
}
#endif  // COMPUTE_UI_SUPPORT

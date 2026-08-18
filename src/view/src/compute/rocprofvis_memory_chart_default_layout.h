// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
//
// Embedded default Memory Chart layout (relational model). Keeps the chart
// populated when a workload has no memory_chart_extdata blob and no runtime
// override exists. Mirrors resources/memory_chart/memory_chart_default.json.

#pragma once

namespace RocProfVis
{
namespace View
{

inline constexpr const char* kDefaultMemoryChartLayout = R"JSON(
{
    "version": 1,
    "blocks": [
        { "id": 1, "column": 0, "title": "Instr Buff", "content": [
            { "metric": "3.1.0", "title": "Occupancy" },
            { "metric": "3.1.1", "title": "Wave Life" }
        ]},
        { "id": 2, "column": 1, "title": "Exec", "content": [
            { "metric": "3.1.11", "title": "Active CUs" },
            { "metric": "3.1.12", "title": "VGPRs" },
            { "metric": "3.1.13", "title": "SGPRs" },
            { "metric": "3.1.14", "title": "LDS Alloc" },
            { "metric": "3.1.15", "title": "Scratch" },
            { "metric": "3.1.16", "title": "Wavefronts" },
            { "metric": "3.1.17", "title": "Workgroups" }
        ]},
        { "id": 10, "column": 2, "order": 0, "title": "LDS", "content": [
            { "metric": "3.1.19", "title": "Util" },
            { "metric": "3.1.20", "title": "Latency" }
        ]},
        { "id": 11, "column": 2, "order": 1, "title": "Vector L1 Cache", "content": [
            { "metric": "3.1.24", "title": "Hit" },
            { "metric": "3.1.25", "title": "Latency" },
            { "metric": "3.1.26", "title": "Coalescing" },
            { "metric": "3.1.27", "title": "Stall" }
        ]},
        { "id": 12, "column": 2, "order": 2, "title": "Scalar L1D Cache", "content": [
            { "metric": "3.1.32", "title": "Hit" },
            { "metric": "3.1.33", "title": "Latency" }
        ]},
        { "id": 13, "column": 2, "order": 3, "title": "Instr L1 Cache", "content": [
            { "metric": "3.1.38", "title": "Hit" },
            { "metric": "3.1.39", "title": "Latency" }
        ]},
        { "id": 20, "column": 3, "title": "L2 Cache", "content": [
            { "metric": "3.1.41", "title": "Rd" },
            { "metric": "3.1.42", "title": "Wr" },
            { "metric": "3.1.43", "title": "Atomic" },
            { "metric": "3.1.44", "title": "Hit" },
            { "metric": "3.1.45", "title": "Rd Latency" },
            { "metric": "3.1.46", "title": "Wr Latency" }
        ]},
        { "id": 30, "column": 4, "order": 0, "title": "xGMI / PCIe", "content": [] },
        { "id": 31, "column": 4, "order": 1, "title": "Fabric", "content": [
            { "metric": "3.1.50", "title": "Rd" },
            { "metric": "3.1.51", "title": "Wr" },
            { "metric": "3.1.52", "title": "Atomic" }
        ]},
        { "id": 32, "column": 4, "order": 2, "title": "GMI", "content": [] },
        { "id": 40, "column": 5, "title": "HBM", "content": [
            { "metric": "3.1.53", "title": "Rd" },
            { "metric": "3.1.54", "title": "Wr" }
        ]}
    ],
    "arrows": [
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.2", "title": "SALU" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.3", "title": "SMEM" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.4", "title": "VALU" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.5", "title": "Matrix" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.6", "title": "VMEM" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.7", "title": "LDS" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.8", "title": "GWS" },
        { "from": 1, "to": 2, "direction": "forward", "metric": "3.1.9", "title": "Br" },
        { "from": 2,  "to": 10, "direction": "backward", "metric": "3.1.18", "title": "Req" },
        { "from": 2,  "to": 11, "direction": "backward", "metric": "3.1.21", "title": "Rd" },
        { "from": 2,  "to": 11, "direction": "forward",  "metric": "3.1.22", "title": "Wr" },
        { "from": 2,  "to": 11, "direction": "both",     "metric": "3.1.23", "title": "Atomic" },
        { "from": 2,  "to": 12, "direction": "backward", "metric": "3.1.31", "title": "Rd" },
        { "from": 13, "to": 1,  "direction": "forward",  "metric": "3.1.37", "title": "Fetch" },
        { "from": 11, "to": 20, "direction": "backward", "metric": "3.1.28", "title": "Rd" },
        { "from": 11, "to": 20, "direction": "forward",  "metric": "3.1.29", "title": "Wr" },
        { "from": 11, "to": 20, "direction": "both",     "metric": "3.1.30", "title": "Atomic" },
        { "from": 12, "to": 20, "direction": "backward", "metric": "3.1.34", "title": "Rd" },
        { "from": 12, "to": 20, "direction": "forward",  "metric": "3.1.35", "title": "Wr" },
        { "from": 12, "to": 20, "direction": "both",     "metric": "3.1.36", "title": "Atomic" },
        { "from": 13, "to": 20, "direction": "backward", "metric": "3.1.40", "title": "Req" },
        { "from": 20, "to": 31, "direction": "backward", "metric": "3.1.47", "title": "Rd" },
        { "from": 20, "to": 31, "direction": "forward",  "metric": "3.1.48", "title": "Wr" },
        { "from": 20, "to": 31, "direction": "both",     "metric": "3.1.49", "title": "Atomic" },
        { "from": 31, "to": 40, "direction": "backward", "metric": "3.1.53", "title": "Rd" },
        { "from": 31, "to": 40, "direction": "forward",  "metric": "3.1.54", "title": "Wr" }
    ]
}
)JSON";

}  // namespace View
}  // namespace RocProfVis

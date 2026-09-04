// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Wrapper DLL for Perfetto trace processor.
// Exports a clean C interface: no name mangling, complete encapsulation
// of all Perfetto internals (SQLite, jsoncpp, zlib) behind the DLL boundary.

#ifdef _WIN32
#  define NOMINMAX
#  define WIN32_LEAN_AND_MEAN
#  include <Windows.h>

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
#  define PERFETTO_DLL_EXPORT __declspec(dllexport)
#else
#  define PERFETTO_DLL_EXPORT __attribute__((visibility("default")))
#endif

#include "perfetto/trace_processor/trace_processor.h"
#include "perfetto/trace_processor/read_trace.h"

#include <cstdint>
#include <string>

using namespace perfetto::trace_processor;

struct PerfettoHandle {
    std::unique_ptr<TraceProcessor> tp;
};

struct PerfettoQuery {
    TraceProcessor::Iterator it;
    uint32_t col_count = 0;
    std::vector<std::string> col_names; 
};


extern "C" {


    PERFETTO_DLL_EXPORT
        PerfettoHandle* Perfetto_Create() {
        Config config;
        auto* h = new PerfettoHandle();
        h->tp = TraceProcessor::CreateInstance(config);
        if (!h->tp) {
            delete h;
            return nullptr;
        }
        return h;
    }

    PERFETTO_DLL_EXPORT
        void Perfetto_Destroy(PerfettoHandle* h) {
        delete h;
    }


    PERFETTO_DLL_EXPORT
        bool Perfetto_LoadModules(PerfettoHandle* h) {
        if (!h || !h->tp) return false;
        auto it = h->tp->ExecuteQuery("INCLUDE PERFETTO MODULE *;");
        while (it.Next()) {}
        return it.Status().ok();
    }

    // ReadTrace calls NotifyEndOfFile internally.
    // Do NOT call Perfetto_NotifyEndOfFile after this.
    PERFETTO_DLL_EXPORT
        bool Perfetto_ReadTrace(PerfettoHandle* h, const char* path) {
        if (!h || !h->tp || !path) return false;

        // Manual chunked read - avoids std::function parameter entirely
        FILE* f = fopen(path, "rb");
        if (!f) return false;

        const size_t CHUNK = 32 * 1024 * 1024;
        bool ok = true;

        while (!feof(f)) {
            auto blob = perfetto::trace_processor::TraceBlob::Allocate(CHUNK);
            size_t n = fread(blob.data(), 1, CHUNK, f);
            if (n == 0) break;
            perfetto::trace_processor::TraceBlobView view(std::move(blob), 0, n);
            if (!h->tp->Parse(std::move(view)).ok()) {
                ok = false;
                break;
            }
        }
        fclose(f);

        if (ok)
            ok = h->tp->NotifyEndOfFile().ok();

        return ok;
    }

    PERFETTO_DLL_EXPORT
        bool Perfetto_Flush(PerfettoHandle* h) {
        if (!h || !h->tp) return false;
        h->tp->Flush();
        return true;
    }

    // Only call this if you used Perfetto_ParseTrace.
    // Do NOT call after Perfetto_ReadTrace - it already called this internally.
    PERFETTO_DLL_EXPORT
        bool Perfetto_NotifyEndOfFile(PerfettoHandle* h) {
        if (!h || !h->tp) return false;
        return h->tp->NotifyEndOfFile().ok();
    }

    PERFETTO_DLL_EXPORT
        PerfettoQuery* Perfetto_Query(PerfettoHandle* h, const char* sql) {
        if (!h || !h->tp || !sql) return nullptr;

        auto* q = new PerfettoQuery{h->tp->ExecuteQuery(sql)};
        q->col_count = q->it.ColumnCount();

        // Cache column names - GetColumnName returns std::string by value
        // so we must store them before returning their c_str()
        q->col_names.reserve(q->col_count);
        for (uint32_t i = 0; i < q->col_count; i++) {
            q->col_names.push_back(q->it.GetColumnName(i));
        }

        return q;
    }

    PERFETTO_DLL_EXPORT
        bool Perfetto_QueryNext(PerfettoQuery* q) {
        if (!q) return false;
        return q->it.Next();
    }

    PERFETTO_DLL_EXPORT
        bool Perfetto_QueryOk(PerfettoQuery* q) {
        if (!q) return false;
        return q->it.Status().ok();
    }

    PERFETTO_DLL_EXPORT
        uint32_t Perfetto_QueryColCount(PerfettoQuery* q) {
        if (!q) return 0;
        return q->col_count;
    }

    PERFETTO_DLL_EXPORT
        const char* Perfetto_QueryColName(PerfettoQuery* q, uint32_t col) {
        if (!q || col >= q->col_count) return "";
        return q->col_names[col].c_str(); 
    }

    // Value types: 0=null 1=long 2=double 3=string 4=bytes
    PERFETTO_DLL_EXPORT
        int Perfetto_QueryValueType(PerfettoQuery* q, uint32_t col) {
        if (!q || col >= q->col_count) return 0;
        using Type = SqlValue::Type;
        switch (q->it.Get(col).type) {
        case Type::kNull:   return 0;
        case Type::kLong:   return 1;
        case Type::kDouble: return 2;
        case Type::kString: return 3;
        case Type::kBytes:  return 4;
        }
        return 0;
    }

    PERFETTO_DLL_EXPORT
        int64_t Perfetto_QueryGetLong(PerfettoQuery* q, uint32_t col) {
        if (!q || col >= q->col_count) return 0;
        return q->it.Get(col).AsLong();
    }

    PERFETTO_DLL_EXPORT
        double Perfetto_QueryGetDouble(PerfettoQuery* q, uint32_t col) {
        if (!q || col >= q->col_count) return 0.0;
        return q->it.Get(col).AsDouble();
    }

    PERFETTO_DLL_EXPORT
        const char* Perfetto_QueryGetString(PerfettoQuery* q, uint32_t col) {
        if (!q || col >= q->col_count) return "";
        const char* s = q->it.Get(col).AsString();
        return s ? s : "";
    }

    PERFETTO_DLL_EXPORT
        const void* Perfetto_QueryGetBytes(PerfettoQuery* q, uint32_t col,
            uint32_t* size_out) {
        if (!q || col >= q->col_count) {
            if (size_out) *size_out = 0;
            return nullptr;
        }
        auto val = q->it.Get(col);
        if (size_out) *size_out = static_cast<uint32_t>(val.bytes_count);
        return val.AsBytes();
    }

    PERFETTO_DLL_EXPORT
        void Perfetto_QueryDestroy(PerfettoQuery* q) {
        delete q;
    }

    // Returns comma-separated list of all table names.
    // Pointer is valid until next call to Perfetto_TableList.
    PERFETTO_DLL_EXPORT
        const char* Perfetto_TableList(PerfettoHandle* h) {
        if (!h || !h->tp) return "";
        static std::string result;
        result.clear();
        auto it = h->tp->ExecuteQuery(
            "SELECT name FROM perfetto_tables ORDER BY name");
        while (it.Next()) {
            if (!it.Get(0).is_null()) {
                if (!result.empty()) result += ',';
                result += it.Get(0).AsString();
            }
        }
        return result.c_str();
    }

} // extern "C"

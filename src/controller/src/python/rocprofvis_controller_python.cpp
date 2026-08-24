// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "python/rocprofvis_controller_python.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_script_engine.h"

#define PY_SSIZE_T_CLEAN
#include "rocprofvis_python.h"

namespace RocProfVis
{
namespace Controller
{

namespace
{

float const    kSliceWaitSeconds         = 0.05f;
uint64_t const kDefaultTableFetchCount   = 10000;
char const* const kSessionCapsuleName    = "rocprofvis.script_session";
char const* const kResultCapsuleName     = "rocprofvis.script_result";

struct EventObject
{
    PyObject_HEAD
    uint64_t  id;
    double    start;
    double    end;
    uint64_t  level;
    PyObject* name;
    PyObject* category;
    // Counter reading for samples, Py_None for interval events.
    PyObject* value;
};

struct TrackObject
{
    PyObject_HEAD
    rocprofvis_handle_t*     track;
    rocprofvis_controller_t* controller;
    ScriptEngine::Session*   session;
};

struct TraceObject
{
    PyObject_HEAD
    rocprofvis_controller_t* controller;
    ScriptEngine::Session*   session;
    PyObject*                tracks;
};

struct SelectionObject
{
    PyObject_HEAD
    PyObject* tracks;
    double    start;
    double    end;
};

struct TableObject
{
    PyObject_HEAD
    rocprofvis_controller_table_t* table;
    rocprofvis_controller_t*       controller;
    ScriptEngine::Session*         session;
    PyObject*                      rows;
};

PyObject* g_event_type     = nullptr;
PyObject* g_track_type     = nullptr;
PyObject* g_trace_type     = nullptr;
PyObject* g_selection_type = nullptr;
PyObject* g_table_type     = nullptr;

ScriptEngine::Session*
session_from_module(PyObject* module)
{
    ScriptEngine::Session* session = nullptr;
    if(module)
    {
        PyObject* capsule = PyObject_GetAttrString(module, "_session");
        if(capsule)
        {
            session = static_cast<ScriptEngine::Session*>(
                PyCapsule_GetPointer(capsule, kSessionCapsuleName));
            Py_DECREF(capsule);
        }
        else
        {
            PyErr_Clear();
        }
    }
    return session;
}

ScriptResult*
result_from_module(PyObject* module)
{
    ScriptResult* result = nullptr;
    if(module)
    {
        PyObject* capsule = PyObject_GetAttrString(module, "_result");
        if(capsule)
        {
            result = static_cast<ScriptResult*>(
                PyCapsule_GetPointer(capsule, kResultCapsuleName));
            Py_DECREF(capsule);
        }
        else
        {
            PyErr_Clear();
        }
    }
    return result;
}

PyObject*
raise_result(char const* what, rocprofvis_result_t result)
{
    return PyErr_Format(PyExc_RuntimeError, "%s failed (%d)", what,
                        static_cast<int>(result));
}

int
add_uint_constant(PyObject* module, char const* name, uint64_t value)
{
    PyObject* obj = PyLong_FromUnsignedLongLong(value);
    int       ok  = -1;
    if(obj)
    {
        ok = PyModule_AddObject(module, name, obj);
        if(ok != 0)
        {
            Py_DECREF(obj);
        }
    }
    return ok;
}

std::string
handle_string(rocprofvis_handle_t* object, rocprofvis_property_t property)
{
    std::string text;
    uint32_t    length = 0;
    rocprofvis_result_t result =
        rocprofvis_controller_get_string(object, property, 0, nullptr, &length);
    if(result == kRocProfVisResultSuccess && length > 0)
    {
        text.resize(length);
        result = rocprofvis_controller_get_string(object, property, 0, text.data(),
                                                  &length);
        if(result != kRocProfVisResultSuccess)
        {
            text.clear();
        }
    }
    return text;
}

PyObject*
py_handle_string(rocprofvis_handle_t* object, rocprofvis_property_t property)
{
    std::string text = handle_string(object, property);
    return PyUnicode_FromStringAndSize(text.data(),
                                       static_cast<Py_ssize_t>(text.size()));
}

void
copy_progress(ScriptEngine::Session* session, rocprofvis_controller_future_t* inner)
{
    if(!session || !session->future || !inner)
    {
        return;
    }
    rocprofvis_controller_future_t* outer =
        reinterpret_cast<rocprofvis_controller_future_t*>(session->future);
    uint64_t percent = 0;
    if(rocprofvis_controller_get_uint64(inner, kRPVControllerFutureProgressPercentage, 0,
                                        &percent) == kRocProfVisResultSuccess)
    {
        rocprofvis_controller_set_uint64(outer, kRPVControllerFutureProgressPercentage, 0,
                                         percent);
    }
    uint32_t length = 0;
    if(rocprofvis_controller_get_string(inner, kRPVControllerFutureProgressMessage, 0,
                                        nullptr, &length) == kRocProfVisResultSuccess)
    {
        std::string message;
        if(length > 0)
        {
            message.resize(length);
            rocprofvis_controller_get_string(inner, kRPVControllerFutureProgressMessage,
                                             0, message.data(), &length);
        }
        rocprofvis_controller_set_string(outer, kRPVControllerFutureProgressMessage, 0,
                                         message.c_str());
    }
}

int
wait_inner(ScriptEngine::Session* session, rocprofvis_controller_future_t* inner)
{
    int                 ok          = 0;
    rocprofvis_result_t wait_result = kRocProfVisResultTimeout;
    while(wait_result == kRocProfVisResultTimeout)
    {
        Py_BEGIN_ALLOW_THREADS
        wait_result = rocprofvis_controller_future_wait(inner, kSliceWaitSeconds);
        Py_END_ALLOW_THREADS
        if(PyErr_CheckSignals() < 0)
        {
            rocprofvis_controller_future_cancel(inner);
            return 0;
        }
        copy_progress(session, inner);
    }
    if(wait_result == kRocProfVisResultSuccess)
    {
        ok = 1;
    }
    else
    {
        raise_result("future_wait", wait_result);
    }
    return ok;
}

int
timeline_range(rocprofvis_controller_t* controller, double* start, double* end)
{
    int                  ok            = 0;
    rocprofvis_handle_t* timeline      = nullptr;
    rocprofvis_result_t  result        = rocprofvis_controller_get_object(
        controller, kRPVControllerSystemTimeline, 0, &timeline);
    if(result == kRocProfVisResultSuccess && timeline && start && end)
    {
        result = rocprofvis_controller_get_double(
            timeline, kRPVControllerTimelineMinTimestamp, 0, start);
        if(result == kRocProfVisResultSuccess)
        {
            result = rocprofvis_controller_get_double(
                timeline, kRPVControllerTimelineMaxTimestamp, 0, end);
        }
        if(result == kRocProfVisResultSuccess)
        {
            ok = 1;
        }
    }
    if(!ok)
    {
        raise_result("timeline range", result);
    }
    return ok;
}

PyObject* make_track(rocprofvis_handle_t* track, rocprofvis_controller_t* controller,
                     ScriptEngine::Session* session);

PyObject*
collect_tracks(rocprofvis_controller_t* controller, ScriptEngine::Session* session)
{
    uint64_t            num_tracks = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
    PyObject* list = nullptr;
    if(result != kRocProfVisResultSuccess)
    {
        return raise_result("num tracks", result);
    }
    list = PyList_New(static_cast<Py_ssize_t>(num_tracks));
    if(!list)
    {
        return nullptr;
    }
    for(uint64_t i = 0; i < num_tracks; ++i)
    {
        rocprofvis_handle_t* track = nullptr;
        result = rocprofvis_controller_get_object(controller,
                                                  kRPVControllerSystemTrackIndexed, i,
                                                  &track);
        if(result != kRocProfVisResultSuccess || !track)
        {
            Py_DECREF(list);
            return raise_result("track indexed", result);
        }
        PyObject* track_obj = make_track(track, controller, session);
        if(!track_obj)
        {
            Py_DECREF(list);
            return nullptr;
        }
        PyList_SET_ITEM(list, static_cast<Py_ssize_t>(i), track_obj);
    }
    return list;
}

int
parse_optional_double(PyObject* obj, double fallback, double* out)
{
    int ok = 0;
    if(!out)
    {
        return 0;
    }
    if(!obj || obj == Py_None)
    {
        *out = fallback;
        ok   = 1;
    }
    else
    {
        double value = PyFloat_AsDouble(obj);
        if(!PyErr_Occurred())
        {
            *out = value;
            ok   = 1;
        }
    }
    return ok;
}

int
parse_table_type(PyObject* obj, uint64_t* out)
{
    int ok = 0;
    if(!out)
    {
        return 0;
    }
    if(!obj || obj == Py_None)
    {
        *out = static_cast<uint64_t>(kRPVControllerTableTypeEvents);
        ok   = 1;
    }
    else if(PyUnicode_Check(obj))
    {
        char const* text = PyUnicode_AsUTF8(obj);
        if(text && std::strcmp(text, "events") == 0)
        {
            *out = static_cast<uint64_t>(kRPVControllerTableTypeEvents);
            ok   = 1;
        }
        else if(text && std::strcmp(text, "samples") == 0)
        {
            *out = static_cast<uint64_t>(kRPVControllerTableTypeSamples);
            ok   = 1;
        }
        else
        {
            PyErr_SetString(PyExc_ValueError, "type must be 'events' or 'samples'");
        }
    }
    else if(PyLong_Check(obj))
    {
        *out = PyLong_AsUnsignedLongLong(obj);
        if(!PyErr_Occurred())
        {
            ok = 1;
        }
    }
    else
    {
        PyErr_SetString(PyExc_TypeError, "type must be int or str");
    }
    return ok;
}

uint64_t
track_type_for_table(uint64_t table_type)
{
    uint64_t track_type = kRPVControllerTrackTypeEvents;
    if(table_type == kRPVControllerTableTypeSamples)
    {
        track_type = kRPVControllerTrackTypeSamples;
    }
    return track_type;
}

PyObject*
copy_event(rocprofvis_handle_t* entry, uint64_t track_type)
{
    EventObject* event = PyObject_New(EventObject, reinterpret_cast<PyTypeObject*>(g_event_type));
    if(!event)
    {
        return nullptr;
    }
    // PyObject_New does not zero the payload, so null the owned references
    // before any path that can decref a partially built event.
    event->id       = 0;
    event->start    = 0.0;
    event->end      = 0.0;
    event->level    = 0;
    event->name     = nullptr;
    event->category = nullptr;
    event->value    = nullptr;
    if(track_type == kRPVControllerTrackTypeEvents)
    {
        rocprofvis_controller_get_uint64(entry, kRPVControllerEventId, 0, &event->id);
        rocprofvis_controller_get_double(entry, kRPVControllerEventStartTimestamp, 0,
                                         &event->start);
        rocprofvis_controller_get_double(entry, kRPVControllerEventEndTimestamp, 0,
                                         &event->end);
        rocprofvis_controller_get_uint64(entry, kRPVControllerEventLevel, 0,
                                         &event->level);
        event->name     = py_handle_string(entry, kRPVControllerEventName);
        event->category = py_handle_string(entry, kRPVControllerEventCategory);
        Py_INCREF(Py_None);
        event->value = Py_None;
    }
    else
    {
        double sample_value = 0.0;
        rocprofvis_controller_get_uint64(entry, kRPVControllerSampleId, 0, &event->id);
        rocprofvis_controller_get_double(entry, kRPVControllerSampleTimestamp, 0,
                                         &event->start);
        event->end = event->start;
        rocprofvis_controller_get_double(entry, kRPVControllerSampleEndTimestamp, 0,
                                         &event->end);
        rocprofvis_controller_get_double(entry, kRPVControllerSampleValue, 0,
                                         &sample_value);
        event->name     = PyUnicode_FromString("");
        event->category = PyUnicode_FromString("");
        event->value    = PyFloat_FromDouble(sample_value);
    }
    if(!event->name || !event->category || !event->value)
    {
        Py_DECREF(event);
        return nullptr;
    }
    return reinterpret_cast<PyObject*>(event);
}

PyObject*
copy_table_rows(rocprofvis_controller_table_t* table, rocprofvis_controller_array_t* array)
{
    uint64_t            num_columns = 0;
    uint64_t            num_rows    = 0;
    rocprofvis_result_t result      = rocprofvis_controller_get_uint64(
        table, kRPVControllerTableNumColumns, 0, &num_columns);
    if(result != kRocProfVisResultSuccess)
    {
        return raise_result("table columns", result);
    }
    result = rocprofvis_controller_get_uint64(array, kRPVControllerArrayNumEntries, 0,
                                              &num_rows);
    if(result != kRocProfVisResultSuccess)
    {
        return raise_result("table rows", result);
    }
    std::vector<std::string> headers;
    headers.resize(static_cast<size_t>(num_columns));
    for(uint64_t i = 0; i < num_columns; ++i)
    {
        uint32_t length = 0;
        result = rocprofvis_controller_get_string(
            table, kRPVControllerTableColumnHeaderIndexed, i, nullptr, &length);
        if(result == kRocProfVisResultSuccess && length > 0)
        {
            headers[static_cast<size_t>(i)].resize(length);
            rocprofvis_controller_get_string(table, kRPVControllerTableColumnHeaderIndexed,
                                             i, headers[static_cast<size_t>(i)].data(),
                                             &length);
        }
        else
        {
            headers[static_cast<size_t>(i)].clear();
        }
    }
    PyObject* rows = PyList_New(static_cast<Py_ssize_t>(num_rows));
    if(!rows)
    {
        return nullptr;
    }
    for(uint64_t i = 0; i < num_rows; ++i)
    {
        rocprofvis_handle_t* row_array = nullptr;
        result = rocprofvis_controller_get_object(array, kRPVControllerArrayEntryIndexed,
                                                  i, &row_array);
        if(result != kRocProfVisResultSuccess || !row_array)
        {
            Py_DECREF(rows);
            return raise_result("table row", result);
        }
        PyObject* row = PyDict_New();
        if(!row)
        {
            Py_DECREF(rows);
            return nullptr;
        }
        for(uint64_t j = 0; j < num_columns; ++j)
        {
            uint64_t column_type = kRPVControllerPrimitiveTypeString;
            rocprofvis_controller_get_uint64(table, kRPVControllerTableColumnTypeIndexed,
                                             j, &column_type);
            PyObject* cell = nullptr;
            switch(column_type)
            {
                case kRPVControllerPrimitiveTypeUInt64:
                {
                    uint64_t value = 0;
                    if(rocprofvis_controller_get_uint64(row_array,
                                                        kRPVControllerArrayEntryIndexed, j,
                                                        &value) == kRocProfVisResultSuccess)
                    {
                        cell = PyLong_FromUnsignedLongLong(value);
                    }
                    break;
                }
                case kRPVControllerPrimitiveTypeDouble:
                {
                    double value = 0.0;
                    if(rocprofvis_controller_get_double(row_array,
                                                        kRPVControllerArrayEntryIndexed, j,
                                                        &value) == kRocProfVisResultSuccess)
                    {
                        cell = PyFloat_FromDouble(value);
                    }
                    break;
                }
                default:
                {
                    std::string text;
                    uint32_t    length = 0;
                    if(rocprofvis_controller_get_string(row_array,
                                                        kRPVControllerArrayEntryIndexed, j,
                                                        nullptr, &length) ==
                       kRocProfVisResultSuccess)
                    {
                        if(length > 0)
                        {
                            text.resize(length);
                            rocprofvis_controller_get_string(
                                row_array, kRPVControllerArrayEntryIndexed, j, text.data(),
                                &length);
                        }
                        cell = PyUnicode_FromStringAndSize(
                            text.data(), static_cast<Py_ssize_t>(text.size()));
                    }
                    break;
                }
            }
            if(!cell)
            {
                cell = Py_None;
                Py_INCREF(cell);
            }
            char const* key = headers[static_cast<size_t>(j)].c_str();
            if(PyDict_SetItemString(row, key, cell) != 0)
            {
                Py_DECREF(cell);
                Py_DECREF(row);
                Py_DECREF(rows);
                return nullptr;
            }
            Py_DECREF(cell);
        }
        PyList_SET_ITEM(rows, static_cast<Py_ssize_t>(i), row);
    }
    return rows;
}

void
event_dealloc(EventObject* self)
{
    Py_XDECREF(self->name);
    Py_XDECREF(self->category);
    Py_XDECREF(self->value);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject*
event_get_id(EventObject* self, void*)
{
    return PyLong_FromUnsignedLongLong(self->id);
}

PyObject*
event_get_start(EventObject* self, void*)
{
    return PyFloat_FromDouble(self->start);
}

PyObject*
event_get_end(EventObject* self, void*)
{
    return PyFloat_FromDouble(self->end);
}

PyObject*
event_get_name(EventObject* self, void*)
{
    Py_INCREF(self->name);
    return self->name;
}

PyObject*
event_get_level(EventObject* self, void*)
{
    return PyLong_FromUnsignedLongLong(self->level);
}

PyObject*
event_get_category(EventObject* self, void*)
{
    Py_INCREF(self->category);
    return self->category;
}

PyObject*
event_get_value(EventObject* self, void*)
{
    Py_INCREF(self->value);
    return self->value;
}

PyGetSetDef g_event_getset[] = {
    {"id", reinterpret_cast<getter>(event_get_id), nullptr, "Event id", nullptr},
    {"start", reinterpret_cast<getter>(event_get_start), nullptr, "Start timestamp",
     nullptr},
    {"end", reinterpret_cast<getter>(event_get_end), nullptr, "End timestamp", nullptr},
    {"name", reinterpret_cast<getter>(event_get_name), nullptr, "Event name", nullptr},
    {"level", reinterpret_cast<getter>(event_get_level), nullptr,
     "Nesting depth, 0 for samples", nullptr},
    {"category", reinterpret_cast<getter>(event_get_category), nullptr, "Event category",
     nullptr},
    {"value", reinterpret_cast<getter>(event_get_value), nullptr,
     "Sample value, None for interval events", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}};

PyType_Slot g_event_slots[] = {{Py_tp_dealloc, reinterpret_cast<void*>(event_dealloc)},
                               {Py_tp_getset, g_event_getset},
                               {Py_tp_doc, const_cast<char*>("Copied track event")},
                               {0, nullptr}};

PyType_Spec g_event_spec = {"optiq.Event", sizeof(EventObject), 0, Py_TPFLAGS_DEFAULT,
                            g_event_slots};

void
track_dealloc(TrackObject* self)
{
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject*
track_get_id(TrackObject* self, void*)
{
    uint64_t id = 0;
    rocprofvis_controller_get_uint64(self->track, kRPVControllerTrackId, 0, &id);
    return PyLong_FromUnsignedLongLong(id);
}

PyObject*
track_get_type(TrackObject* self, void*)
{
    uint64_t type = 0;
    rocprofvis_controller_get_uint64(self->track, kRPVControllerTrackType, 0, &type);
    return PyLong_FromUnsignedLongLong(type);
}

PyObject*
track_get_name(TrackObject* self, void*)
{
    return py_handle_string(self->track, kRPVControllerTrackMainName);
}

PyObject*
track_get_sub_name(TrackObject* self, void*)
{
    return py_handle_string(self->track, kRPVControllerTrackSubName);
}

PyObject*
track_get_min_time(TrackObject* self, void*)
{
    double value = 0.0;
    rocprofvis_controller_get_double(self->track, kRPVControllerTrackMinTimestamp, 0,
                                     &value);
    return PyFloat_FromDouble(value);
}

PyObject*
track_get_max_time(TrackObject* self, void*)
{
    double value = 0.0;
    rocprofvis_controller_get_double(self->track, kRPVControllerTrackMaxTimestamp, 0,
                                     &value);
    return PyFloat_FromDouble(value);
}

PyObject*
track_get_num_entries(TrackObject* self, void*)
{
    uint64_t value = 0;
    rocprofvis_controller_get_uint64(self->track, kRPVControllerTrackNumberOfEntries, 0,
                                     &value);
    return PyLong_FromUnsignedLongLong(value);
}

PyObject*
track_events(TrackObject* self, PyObject* args, PyObject* kwargs)
{
    static char start_key[] = "start";
    static char end_key[]   = "end";
    static char* keywords[] = {start_key, end_key, nullptr};
    PyObject*    start_obj  = Py_None;
    PyObject*    end_obj    = Py_None;
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|OO:events", keywords, &start_obj,
                                    &end_obj))
    {
        return nullptr;
    }
    if(!self->controller || !self->track)
    {
        PyErr_SetString(PyExc_RuntimeError, "track has no controller");
        return nullptr;
    }
    double min_time = 0.0;
    double max_time = 0.0;
    rocprofvis_controller_get_double(self->track, kRPVControllerTrackMinTimestamp, 0,
                                     &min_time);
    rocprofvis_controller_get_double(self->track, kRPVControllerTrackMaxTimestamp, 0,
                                     &max_time);
    double start = min_time;
    double end   = max_time;
    if(!parse_optional_double(start_obj, min_time, &start) ||
       !parse_optional_double(end_obj, max_time, &end))
    {
        return nullptr;
    }
    rocprofvis_controller_array_t*  array  = rocprofvis_controller_array_alloc(0);
    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    PyObject*                       events = nullptr;
    if(!array || !future)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        PyErr_SetString(PyExc_MemoryError, "track fetch alloc failed");
        return nullptr;
    }
    rocprofvis_result_t result = rocprofvis_controller_track_fetch_async(
        self->controller, reinterpret_cast<rocprofvis_controller_track_t*>(self->track),
        start, end, future, array);
    if(result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        return raise_result("track_fetch_async", result);
    }
    if(!wait_inner(self->session, future))
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        return nullptr;
    }
    uint64_t future_result = kRocProfVisResultUnknownError;
    rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                     &future_result);
    if(future_result != kRocProfVisResultSuccess &&
       future_result != kRocProfVisResultOutOfRange)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        return raise_result("track fetch",
                            static_cast<rocprofvis_result_t>(future_result));
    }
    uint64_t track_type = kRPVControllerTrackTypeEvents;
    rocprofvis_controller_get_uint64(self->track, kRPVControllerTrackType, 0, &track_type);
    uint64_t num_results = 0;
    rocprofvis_controller_get_uint64(array, kRPVControllerArrayNumEntries, 0,
                                     &num_results);
    events = PyList_New(static_cast<Py_ssize_t>(num_results));
    if(events)
    {
        for(uint64_t i = 0; i < num_results; ++i)
        {
            rocprofvis_handle_t* entry = nullptr;
            result = rocprofvis_controller_get_object(
                array, kRPVControllerArrayEntryIndexed, i, &entry);
            if(result != kRocProfVisResultSuccess || !entry)
            {
                Py_DECREF(events);
                events = raise_result("track entry", result);
                break;
            }
            PyObject* copied = copy_event(entry, track_type);
            if(!copied)
            {
                Py_DECREF(events);
                events = nullptr;
                break;
            }
            PyList_SET_ITEM(events, static_cast<Py_ssize_t>(i), copied);
        }
    }
    rocprofvis_controller_array_free(array);
    rocprofvis_controller_future_free(future);
    return events;
}

PyGetSetDef g_track_getset[] = {
    {"id", reinterpret_cast<getter>(track_get_id), nullptr, "Track id", nullptr},
    {"type", reinterpret_cast<getter>(track_get_type), nullptr, "Track type", nullptr},
    {"name", reinterpret_cast<getter>(track_get_name), nullptr, "Track main name",
     nullptr},
    {"sub_name", reinterpret_cast<getter>(track_get_sub_name), nullptr, "Track sub name",
     nullptr},
    {"min_time", reinterpret_cast<getter>(track_get_min_time), nullptr,
     "Track min timestamp", nullptr},
    {"max_time", reinterpret_cast<getter>(track_get_max_time), nullptr,
     "Track max timestamp", nullptr},
    {"num_entries", reinterpret_cast<getter>(track_get_num_entries), nullptr,
     "Declared entry count", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}};

PyMethodDef g_track_methods[] = {
    {"events", reinterpret_cast<PyCFunction>(track_events),
     METH_VARARGS | METH_KEYWORDS, "Fetch events in [start, end] and return copies."},
    {nullptr, nullptr, 0, nullptr}};

PyType_Slot g_track_slots[] = {{Py_tp_dealloc, reinterpret_cast<void*>(track_dealloc)},
                               {Py_tp_getset, g_track_getset},
                               {Py_tp_methods, g_track_methods},
                               {Py_tp_doc, const_cast<char*>("Borrowed controller track")},
                               {0, nullptr}};

PyType_Spec g_track_spec = {"optiq.Track", sizeof(TrackObject), 0, Py_TPFLAGS_DEFAULT,
                            g_track_slots};

PyObject*
make_track(rocprofvis_handle_t* track, rocprofvis_controller_t* controller,
           ScriptEngine::Session* session)
{
    TrackObject* object =
        PyObject_New(TrackObject, reinterpret_cast<PyTypeObject*>(g_track_type));
    if(!object)
    {
        return nullptr;
    }
    object->track      = track;
    object->controller = controller;
    object->session    = session;
    return reinterpret_cast<PyObject*>(object);
}

void
trace_dealloc(TraceObject* self)
{
    Py_XDECREF(self->tracks);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject*
trace_get_tracks(TraceObject* self, void*)
{
    Py_INCREF(self->tracks);
    return self->tracks;
}

PyGetSetDef g_trace_getset[] = {
    {"tracks", reinterpret_cast<getter>(trace_get_tracks), nullptr,
     "All tracks on the loaded controller", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}};

PyType_Slot g_trace_slots[] = {{Py_tp_dealloc, reinterpret_cast<void*>(trace_dealloc)},
                               {Py_tp_getset, g_trace_getset},
                               {Py_tp_doc, const_cast<char*>("Loaded controller")},
                               {0, nullptr}};

PyType_Spec g_trace_spec = {"optiq.Trace", sizeof(TraceObject), 0, Py_TPFLAGS_DEFAULT,
                            g_trace_slots};

void
selection_dealloc(SelectionObject* self)
{
    Py_XDECREF(self->tracks);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

PyObject*
selection_get_tracks(SelectionObject* self, void*)
{
    Py_INCREF(self->tracks);
    return self->tracks;
}

PyObject*
selection_get_start(SelectionObject* self, void*)
{
    return PyFloat_FromDouble(self->start);
}

PyObject*
selection_get_end(SelectionObject* self, void*)
{
    return PyFloat_FromDouble(self->end);
}

PyGetSetDef g_selection_getset[] = {
    {"tracks", reinterpret_cast<getter>(selection_get_tracks), nullptr,
     "Selected tracks", nullptr},
    {"start", reinterpret_cast<getter>(selection_get_start), nullptr,
     "Selection start timestamp", nullptr},
    {"end", reinterpret_cast<getter>(selection_get_end), nullptr,
     "Selection end timestamp", nullptr},
    {nullptr, nullptr, nullptr, nullptr, nullptr}};

PyType_Slot g_selection_slots[] = {
    {Py_tp_dealloc, reinterpret_cast<void*>(selection_dealloc)},
    {Py_tp_getset, g_selection_getset},
    {Py_tp_doc, const_cast<char*>("Script selection context")},
    {0, nullptr}};

PyType_Spec g_selection_spec = {"optiq.Selection", sizeof(SelectionObject), 0,
                                Py_TPFLAGS_DEFAULT, g_selection_slots};

void
table_dealloc(TableObject* self)
{
    if(self->table)
    {
        rocprofvis_controller_table_free(self->table);
        self->table = nullptr;
    }
    Py_XDECREF(self->rows);
    Py_TYPE(self)->tp_free(reinterpret_cast<PyObject*>(self));
}

int
append_tracks_from_sequence(PyObject* sequence, uint64_t want_type,
                            rocprofvis_controller_arguments_t* args, uint64_t* count)
{
    Py_ssize_t size = PySequence_Size(sequence);
    if(size < 0)
    {
        return 0;
    }
    uint64_t written = 0;
    for(Py_ssize_t i = 0; i < size; ++i)
    {
        PyObject* item = PySequence_GetItem(sequence, i);
        if(!item)
        {
            return 0;
        }
        if(!PyObject_TypeCheck(item, reinterpret_cast<PyTypeObject*>(g_track_type)))
        {
            Py_DECREF(item);
            PyErr_SetString(PyExc_TypeError, "tracks must contain optiq.Track");
            return 0;
        }
        TrackObject* track = reinterpret_cast<TrackObject*>(item);
        uint64_t     type  = 0;
        rocprofvis_controller_get_uint64(track->track, kRPVControllerTrackType, 0, &type);
        if(type == want_type)
        {
            rocprofvis_result_t result = rocprofvis_controller_set_object(
                args, kRPVControllerTableArgsTracksIndexed, written, track->track);
            if(result != kRocProfVisResultSuccess)
            {
                Py_DECREF(item);
                raise_result("set track", result);
                return 0;
            }
            ++written;
        }
        Py_DECREF(item);
    }
    *count = written;
    return 1;
}

int
append_tracks_from_controller(rocprofvis_controller_t* controller, uint64_t want_type,
                              rocprofvis_controller_arguments_t* args, uint64_t* count)
{
    uint64_t            num_tracks = 0;
    rocprofvis_result_t result     = rocprofvis_controller_get_uint64(
        controller, kRPVControllerSystemNumTracks, 0, &num_tracks);
    if(result != kRocProfVisResultSuccess)
    {
        raise_result("num tracks", result);
        return 0;
    }
    uint64_t written = 0;
    for(uint64_t i = 0; i < num_tracks; ++i)
    {
        rocprofvis_handle_t* track = nullptr;
        result = rocprofvis_controller_get_object(controller,
                                                  kRPVControllerSystemTrackIndexed, i,
                                                  &track);
        if(result != kRocProfVisResultSuccess || !track)
        {
            raise_result("track indexed", result);
            return 0;
        }
        uint64_t type = 0;
        rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0, &type);
        if(type == want_type)
        {
            result = rocprofvis_controller_set_object(
                args, kRPVControllerTableArgsTracksIndexed, written, track);
            if(result != kRocProfVisResultSuccess)
            {
                raise_result("set track", result);
                return 0;
            }
            ++written;
        }
    }
    *count = written;
    return 1;
}

PyObject*
table_fetch(TableObject* self, PyObject* args, PyObject* kwargs)
{
    static char tracks_key[]        = "tracks";
    static char start_key[]         = "start";
    static char end_key[]           = "end";
    static char where_key[]         = "where";
    static char filter_key[]        = "filter";
    static char group_key[]         = "group";
    static char group_columns_key[] = "group_columns";
    static char sort_column_key[]   = "sort_column";
    static char sort_order_key[]    = "sort_order";
    static char start_index_key[]   = "start_index";
    static char count_key[]         = "count";
    static char type_key[]          = "type";
    static char* keywords[]         = {tracks_key,        start_key,       end_key,
                               where_key,         filter_key,      group_key,
                               group_columns_key, sort_column_key, sort_order_key,
                               start_index_key,   count_key,       type_key,
                               nullptr};
    PyObject*    tracks_obj         = Py_None;
    PyObject*    start_obj          = Py_None;
    PyObject*    end_obj            = Py_None;
    char const*  where_text         = "";
    char const*  filter_text        = "";
    char const*  group_text         = "";
    char const*  group_columns_text = "";
    unsigned long long sort_column  = 0;
    unsigned long long sort_order   = kRPVControllerSortOrderAscending;
    unsigned long long start_index  = 0;
    unsigned long long count        = kDefaultTableFetchCount;
    PyObject*          type_obj     = Py_None;
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "|OOOzzzzKKKKO:fetch", keywords,
                                    &tracks_obj, &start_obj, &end_obj, &where_text,
                                    &filter_text, &group_text, &group_columns_text,
                                    &sort_column, &sort_order, &start_index, &count,
                                    &type_obj))
    {
        return nullptr;
    }
    if(!self->controller || !self->table)
    {
        PyErr_SetString(PyExc_RuntimeError, "table has no controller");
        return nullptr;
    }
    uint64_t table_type = static_cast<uint64_t>(kRPVControllerTableTypeEvents);
    if(!parse_table_type(type_obj, &table_type))
    {
        return nullptr;
    }
    double start = 0.0;
    double end   = 0.0;
    if(!timeline_range(self->controller, &start, &end))
    {
        return nullptr;
    }
    if(!parse_optional_double(start_obj, start, &start) ||
       !parse_optional_double(end_obj, end, &end))
    {
        return nullptr;
    }
    rocprofvis_controller_arguments_t* fetch_args =
        rocprofvis_controller_arguments_alloc();
    if(!fetch_args)
    {
        PyErr_SetString(PyExc_MemoryError, "arguments_alloc failed");
        return nullptr;
    }
    rocprofvis_result_t result = rocprofvis_controller_set_uint64(
        fetch_args, kRPVControllerTableArgsType, 0, table_type);
    if(result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(fetch_args);
        return raise_result("table type", result);
    }
    uint64_t track_count = 0;
    uint64_t want_type   = track_type_for_table(table_type);
    int      tracks_ok   = 0;
    if(!tracks_obj || tracks_obj == Py_None)
    {
        tracks_ok = append_tracks_from_controller(self->controller, want_type, fetch_args,
                                                  &track_count);
    }
    else
    {
        tracks_ok =
            append_tracks_from_sequence(tracks_obj, want_type, fetch_args, &track_count);
    }
    if(!tracks_ok)
    {
        rocprofvis_controller_arguments_free(fetch_args);
        return nullptr;
    }
    if(track_count == 0)
    {
        rocprofvis_controller_arguments_free(fetch_args);
        PyErr_SetString(PyExc_RuntimeError, "fetch requires at least one matching track");
        return nullptr;
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            fetch_args, kRPVControllerTableArgsNumTracks, 0, track_count);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_double(
            fetch_args, kRPVControllerTableArgsStartTime, 0, start);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_double(fetch_args, kRPVControllerTableArgsEndTime,
                                                  0, end);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            fetch_args, kRPVControllerTableArgsSortColumn, 0, sort_column);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            fetch_args, kRPVControllerTableArgsSortOrder, 0, sort_order);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            fetch_args, kRPVControllerTableArgsStartIndex, 0, start_index);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            fetch_args, kRPVControllerTableArgsStartCount, 0, count);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_string(fetch_args, kRPVControllerTableArgsWhere,
                                                  0, where_text ? where_text : "");
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_string(fetch_args, kRPVControllerTableArgsFilter,
                                                  0, filter_text ? filter_text : "");
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_string(fetch_args, kRPVControllerTableArgsGroup,
                                                  0, group_text ? group_text : "");
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_string(
            fetch_args, kRPVControllerTableArgsGroupColumns, 0,
            group_columns_text ? group_columns_text : "");
    }
    if(result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(fetch_args);
        return raise_result("table fetch args", result);
    }
    rocprofvis_controller_array_t*  array  = rocprofvis_controller_array_alloc(0);
    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    if(!array || !future)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(fetch_args);
        PyErr_SetString(PyExc_MemoryError, "table fetch alloc failed");
        return nullptr;
    }
    result = rocprofvis_controller_table_fetch_async(self->controller, self->table,
                                                     fetch_args, future, array);
    if(result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(fetch_args);
        return raise_result("table_fetch_async", result);
    }
    if(!wait_inner(self->session, future))
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(fetch_args);
        return nullptr;
    }
    uint64_t future_result = kRocProfVisResultUnknownError;
    rocprofvis_controller_get_uint64(future, kRPVControllerFutureResult, 0,
                                     &future_result);
    if(future_result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_array_free(array);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(fetch_args);
        return raise_result("table fetch",
                            static_cast<rocprofvis_result_t>(future_result));
    }
    PyObject* rows = copy_table_rows(self->table, array);
    rocprofvis_controller_array_free(array);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_arguments_free(fetch_args);
    if(!rows)
    {
        return nullptr;
    }
    Py_XDECREF(self->rows);
    self->rows = rows;
    Py_INCREF(self->rows);
    return self->rows;
}

PyObject*
table_rows(TableObject* self, PyObject*)
{
    if(self->rows)
    {
        Py_INCREF(self->rows);
        return self->rows;
    }
    return PyList_New(0);
}

PyMethodDef g_table_methods[] = {
    {"fetch", reinterpret_cast<PyCFunction>(table_fetch), METH_VARARGS | METH_KEYWORDS,
     "Run a private table query and return copied rows."},
    {"rows", reinterpret_cast<PyCFunction>(table_rows), METH_NOARGS,
     "Rows from the last fetch."},
    {nullptr, nullptr, 0, nullptr}};

PyType_Slot g_table_slots[] = {{Py_tp_dealloc, reinterpret_cast<void*>(table_dealloc)},
                               {Py_tp_methods, g_table_methods},
                               {Py_tp_doc, const_cast<char*>("Private query table")},
                               {0, nullptr}};

PyType_Spec g_table_spec = {"optiq.Table", sizeof(TableObject), 0, Py_TPFLAGS_DEFAULT,
                            g_table_slots};

int
ensure_types(void)
{
    int ok = 1;
    if(!g_event_type)
    {
        g_event_type = PyType_FromSpec(&g_event_spec);
        g_track_type = PyType_FromSpec(&g_track_spec);
        g_trace_type = PyType_FromSpec(&g_trace_spec);
        g_selection_type = PyType_FromSpec(&g_selection_spec);
        g_table_type = PyType_FromSpec(&g_table_spec);
        if(!g_event_type || !g_track_type || !g_trace_type || !g_selection_type ||
           !g_table_type)
        {
            ok = 0;
        }
    }
    return ok;
}

PyObject*
make_trace(ScriptEngine::Session* session)
{
    TraceObject* object =
        PyObject_New(TraceObject, reinterpret_cast<PyTypeObject*>(g_trace_type));
    if(!object)
    {
        return nullptr;
    }
    object->controller = session->controller;
    object->session    = session;
    object->tracks     = collect_tracks(session->controller, session);
    if(!object->tracks)
    {
        Py_DECREF(object);
        return nullptr;
    }
    return reinterpret_cast<PyObject*>(object);
}

PyObject*
make_selection(ScriptEngine::Session* session, PyObject* all_tracks, double start,
               double end)
{
    SelectionObject* object =
        PyObject_New(SelectionObject, reinterpret_cast<PyTypeObject*>(g_selection_type));
    if(!object)
    {
        return nullptr;
    }
    object->start  = start;
    object->end    = end;
    object->tracks = nullptr;
    uint64_t context_tracks = 0;
    if(session->context)
    {
        rocprofvis_controller_get_uint64(session->context,
                                         kRPVControllerScriptContextNumTracks, 0,
                                         &context_tracks);
        rocprofvis_controller_get_double(session->context,
                                         kRPVControllerScriptContextTimeRangeStart, 0,
                                         &object->start);
        rocprofvis_controller_get_double(session->context,
                                         kRPVControllerScriptContextTimeRangeEnd, 0,
                                         &object->end);
    }
    if(context_tracks > 0 && session->context)
    {
        object->tracks = PyList_New(static_cast<Py_ssize_t>(context_tracks));
        if(!object->tracks)
        {
            Py_DECREF(object);
            return nullptr;
        }
        for(uint64_t i = 0; i < context_tracks; ++i)
        {
            rocprofvis_handle_t* track = nullptr;
            rocprofvis_result_t result = rocprofvis_controller_get_object(
                session->context, kRPVControllerScriptContextTracksIndexed, i, &track);
            if(result != kRocProfVisResultSuccess || !track)
            {
                Py_DECREF(object);
                return raise_result("context track", result);
            }
            PyObject* track_obj = make_track(track, session->controller, session);
            if(!track_obj)
            {
                Py_DECREF(object);
                return nullptr;
            }
            PyList_SET_ITEM(object->tracks, static_cast<Py_ssize_t>(i), track_obj);
        }
    }
    else
    {
        Py_INCREF(all_tracks);
        object->tracks = all_tracks;
    }
    return reinterpret_cast<PyObject*>(object);
}

PyObject*
optiq_table(PyObject* self, PyObject*)
{
    ScriptEngine::Session* session = session_from_module(self);
    if(!session || !session->controller)
    {
        PyErr_SetString(PyExc_RuntimeError, "optiq.table requires a loaded controller");
        return nullptr;
    }
    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerCompute;
    if(rocprofvis_controller_get_object_type(session->controller, &type) !=
           kRocProfVisResultSuccess ||
       type != kRPVControllerObjectTypeControllerSystem)
    {
        PyErr_SetString(PyExc_RuntimeError, "optiq.table requires a system trace");
        return nullptr;
    }
    rocprofvis_controller_table_t* table = rocprofvis_controller_table_alloc();
    if(!table)
    {
        PyErr_SetString(PyExc_MemoryError, "table_alloc failed");
        return nullptr;
    }
    TableObject* object =
        PyObject_New(TableObject, reinterpret_cast<PyTypeObject*>(g_table_type));
    if(!object)
    {
        rocprofvis_controller_table_free(table);
        return nullptr;
    }
    object->table      = table;
    object->controller = session->controller;
    object->session    = session;
    object->rows       = nullptr;
    return reinterpret_cast<PyObject*>(object);
}

PyMethodDef kOptiqMethods[] = {
    {"table", optiq_table, METH_NOARGS,
     "Allocate a private query table (not the UI Event Table)."},
    {nullptr, nullptr, 0, nullptr}};

PyObject*
result_text(PyObject* self, PyObject* args)
{
    char const* text = nullptr;
    PyObject*   ret  = nullptr;
    if(PyArg_ParseTuple(args, "s:text", &text))
    {
        ScriptResult* result = result_from_module(self);
        if(result)
        {
            result->AppendText(text);
            Py_INCREF(Py_None);
            ret = Py_None;
        }
        else
        {
            PyErr_SetString(PyExc_RuntimeError, "optiq result session is missing");
        }
    }
    return ret;
}

PyMethodDef kResultMethods[] = {
    {"text", result_text, METH_VARARGS, "Append a text item to the script result."},
    {nullptr, nullptr, 0, nullptr}};

PyObject*
make_result_module(ScriptResult* script_result)
{
    PyObject* result_mod = PyModule_New("optiq.result");
    if(result_mod)
    {
        if(PyModule_AddFunctions(result_mod, kResultMethods) != 0)
        {
            Py_DECREF(result_mod);
            result_mod = nullptr;
        }
        else
        {
            PyObject* capsule =
                PyCapsule_New(script_result, kResultCapsuleName, nullptr);
            if(capsule)
            {
                PyObject_SetAttrString(result_mod, "_result", capsule);
                Py_DECREF(capsule);
            }
        }
    }
    return result_mod;
}

}  // namespace

void
optiq_prepare_globals(void* py_dict, void* script_session)
{
    PyObject*              globals = static_cast<PyObject*>(py_dict);
    ScriptEngine::Session* session =
        static_cast<ScriptEngine::Session*>(script_session);
    if(!globals || !session || !session->result)
    {
        return;
    }
    if(!ensure_types())
    {
        PyErr_Clear();
        return;
    }

    PyObject* optiq = PyModule_New("optiq");
    if(!optiq)
    {
        PyErr_Clear();
        return;
    }

    PyObject* result_mod = make_result_module(session->result);
    if(result_mod)
    {
        PyObject_SetAttrString(optiq, "result", result_mod);
        Py_DECREF(result_mod);
    }

    PyObject* session_capsule = PyCapsule_New(session, kSessionCapsuleName, nullptr);
    if(session_capsule)
    {
        PyObject_SetAttrString(optiq, "_session", session_capsule);
        Py_DECREF(session_capsule);
    }

    if(PyModule_AddFunctions(optiq, kOptiqMethods) != 0)
    {
        PyErr_Clear();
    }
    add_uint_constant(optiq, "TRACK_TYPE_SAMPLES",
                      static_cast<uint64_t>(kRPVControllerTrackTypeSamples));
    add_uint_constant(optiq, "TRACK_TYPE_EVENTS",
                      static_cast<uint64_t>(kRPVControllerTrackTypeEvents));
    add_uint_constant(optiq, "TABLE_TYPE_EVENTS",
                      static_cast<uint64_t>(kRPVControllerTableTypeEvents));
    add_uint_constant(optiq, "TABLE_TYPE_SAMPLES",
                      static_cast<uint64_t>(kRPVControllerTableTypeSamples));
    add_uint_constant(optiq, "SORT_ASCENDING",
                      static_cast<uint64_t>(kRPVControllerSortOrderAscending));
    add_uint_constant(optiq, "SORT_DESCENDING",
                      static_cast<uint64_t>(kRPVControllerSortOrderDescending));

    if(session->controller)
    {
        PyObject* trace = make_trace(session);
        if(trace)
        {
            PyObject_SetAttrString(optiq, "trace", trace);
            TraceObject* trace_obj = reinterpret_cast<TraceObject*>(trace);
            double       start     = 0.0;
            double       end       = 0.0;
            if(!timeline_range(session->controller, &start, &end))
            {
                PyErr_Clear();
            }
            PyObject* selection =
                make_selection(session, trace_obj->tracks, start, end);
            if(selection)
            {
                PyObject_SetAttrString(optiq, "selection", selection);
                Py_DECREF(selection);
            }
            else
            {
                PyErr_Clear();
                Py_INCREF(Py_None);
                PyObject_SetAttrString(optiq, "selection", Py_None);
            }
            Py_DECREF(trace);
        }
        else
        {
            PyErr_Clear();
            Py_INCREF(Py_None);
            PyObject_SetAttrString(optiq, "trace", Py_None);
            Py_INCREF(Py_None);
            PyObject_SetAttrString(optiq, "selection", Py_None);
        }
    }
    else
    {
        Py_INCREF(Py_None);
        PyObject_SetAttrString(optiq, "trace", Py_None);
        Py_INCREF(Py_None);
        PyObject_SetAttrString(optiq, "selection", Py_None);
    }

    PyDict_SetItemString(globals, "optiq", optiq);
    Py_DECREF(optiq);
}

}  // namespace Controller
}  // namespace RocProfVis

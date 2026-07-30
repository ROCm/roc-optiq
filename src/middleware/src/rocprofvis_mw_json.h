// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "json.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Thin, defensive helpers over jt::Json.
 *
 * jt::Json's accessors are non-const and operator[] inserts on a missing key,
 * so reading straight off a request object would mutate it. Every reader here
 * probes with contains() first and returns the supplied default when the key is
 * absent or holds an incompatible type. Nothing in this file aborts or throws:
 * jt::Json::getLong() and friends call abort() on a type mismatch, which would
 * take the process down on malformed client input.
 */
namespace Json
{

/*
 * Largest integer that survives a round trip through an IEEE-754 double, which
 * is what a JavaScript client will parse a JSON number into. Values above this
 * are emitted as decimal strings by MakeUInt so no precision is silently lost.
 */
constexpr uint64_t MAX_EXACT_INTEGER = (1ULL << 53) - 1;

/* Construct an empty object / array. */
jt::Json MakeObject(void);
jt::Json MakeArray(void);

/*
 * Encode an unsigned 64-bit value. Emits a JSON number when the value is
 * exactly representable by a double, otherwise a decimal string. Clients should
 * accept both for any id-shaped field.
 */
jt::Json MakeUInt(uint64_t value);

/*
 * Encode a double. Non-finite values (NaN, +/-Inf) have no JSON spelling and
 * are emitted as null rather than producing an unparseable document.
 */
jt::Json MakeDouble(double value);

/* Append to an array value. The target is converted to an array if needed. */
void Append(jt::Json& array, jt::Json value);

/* True when key is present on an object value. */
bool Has(const jt::Json& object, const std::string& key);

/*
 * Typed readers. Each returns def when the key is missing or the stored value
 * cannot be coerced to the requested type. GetUInt also accepts a decimal
 * string, mirroring MakeUInt's output.
 */
bool        GetBool(const jt::Json& object, const std::string& key, bool def);
uint64_t    GetUInt(const jt::Json& object, const std::string& key, uint64_t def);
int64_t     GetInt(const jt::Json& object, const std::string& key, int64_t def);
double      GetDouble(const jt::Json& object, const std::string& key, double def);
std::string GetString(const jt::Json& object, const std::string& key,
                      const std::string& def);

/* Outcome of reading an id-shaped value. */
enum class IdStatus
{
    kValid,
    kMissing,
    /* A number too large to have survived JSON parsing exactly. */
    kImprecise,
    kMalformed
};

/*
 * Read an id-shaped value, which MakeUInt spells as a decimal string once it
 * grows past what a double holds exactly. A client that echoes such an id back
 * as a JSON number has already lost the low bits, and rounding lands on a
 * neighbouring object rather than nothing, so that case is reported instead of
 * being resolved to whatever the rounded value happens to name.
 */
IdStatus GetId(const jt::Json& object, const std::string& key, uint64_t& out);

/* Human-facing reason for a non-valid IdStatus, for use in an error message. */
char const* IdStatusToString(IdStatus status);

/*
 * Array readers. Return an empty vector when the key is missing or is not an
 * array; individual entries that cannot be coerced are skipped.
 */
std::vector<uint64_t>    GetUIntArray(const jt::Json& object, const std::string& key);
std::vector<std::string> GetStringArray(const jt::Json& object, const std::string& key);

/*
 * Fetch a nested object by key. Returns an empty object when absent.
 */
jt::Json GetObject(const jt::Json& object, const std::string& key);

/*
 * Copy a member of any type verbatim. Returns a null value when absent. Used to
 * echo a client's opaque correlation id back without inspecting it.
 */
jt::Json GetValue(const jt::Json& object, const std::string& key);

/*
 * Parse a request document. Returns false and fills error_message when the text
 * is not a well-formed JSON object.
 */
bool Parse(const std::string& text, jt::Json& out, std::string& error_message);

}  // namespace Json
}  // namespace Middleware
}  // namespace RocProfVis

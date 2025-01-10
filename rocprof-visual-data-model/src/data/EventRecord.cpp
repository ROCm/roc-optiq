#include "EventRecord.h"
#include "Trace.h"
#include <cassert>

const char*  EventRecord::getTypeName(Trace* ctx)
{
    assert(ctx && "Trace context cannot be NULL");
    return ctx->getStringAt(m_typeId);
}

const char*  EventRecord::getDescription(Trace *ctx)
{
    assert(ctx && "Trace context cannot be NULL");
    return ctx->getStringAt(m_descriptionId);
}

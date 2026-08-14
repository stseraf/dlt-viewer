#include "ecutree.h"

void EcuTree::add(const QString& ecuId, const qdlt::msg::payload::GetLogInfo& info)
{
    /* Responses carry different fields depending on their status, and a log file
     * usually contains several of them. Merge into what is already known instead
     * of replacing it, so that a status 6 response (no descriptions) does not
     * drop the descriptions a status 7 response delivered earlier. */
    for (const auto& app : info.apps) {
        App& appData = ecus[ecuId][app.id];
        if (info.hasDescription) {
            appData.description = app.description;
        }
        for (const auto& ctx : app.ctxs) {
            Ctx& ctxData = appData.contexts[ctx.id];
            if (info.hasDescription) {
                ctxData.description = ctx.description;
            }
            if (info.hasLogLevel) {
                ctxData.logLevel = ctx.logLevel;
            }
            if (info.hasTraceStatus) {
                ctxData.traceStatus = ctx.traceStatus;
            }
        }
    }
}

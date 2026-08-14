#ifndef ECUTREEBUILDER_H
#define ECUTREEBUILDER_H

#include <qdltctrlmsg.h>

#include <map>

struct EcuTree
{
    void add(const QString& ecuId, const qdlt::msg::payload::GetLogInfo&);

    using EcuId = QString;
    using AppId = QString;
    using CtxId = QString;
    struct Ctx {
        QString description;
        int8_t logLevel = -1;
        int8_t traceStatus = -1;
    };
    struct App {
        QString description;
        std::map<CtxId, Ctx> contexts;
    };
    std::map<EcuId, std::map<AppId, App>> ecus;
};

#endif // ECUTREEBUILDER_H

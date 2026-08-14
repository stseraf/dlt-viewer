#ifndef QDLTCTRLMSG_H
#define QDLTCTRLMSG_H

#include "export_rules.h"

#include <vector>
#include <variant>

#include <QByteArray>
#include <QList>
#include <QString>


namespace qdlt::msg::payload {

struct GetLogInfo {
    struct App {
        struct Ctx {
            QString id;
            int8_t logLevel;
            int8_t traceStatus;
            QString description;
        };

        QString id;
        QString description;
        std::vector<Ctx> ctxs;
    };

    uint8_t status;
    std::vector<App> apps;
};

struct GetSoftwareVersion {
};

struct GetDefaultLogLevel
{
    uint8_t logLevel;
    uint8_t status;
};

struct SetLogLevel {
    uint8_t status;
};

struct Timezone {
    uint8_t status;
    int32_t timezone;
    uint8_t isDst;
};

struct UnregisterContext {
    uint8_t status;
    QString appid;
    QString ctxid;
};

struct Uninteresting {
    uint32_t serviceId;
    bool parseError = false;
};

using Type = std::variant<GetLogInfo, GetSoftwareVersion, GetDefaultLogLevel, SetLogLevel, Timezone,
                          UnregisterContext, Uninteresting>;

// Wire-format width of the GET_LOG_INFO (status 7) description-length fields.
// The AUTOSAR PRS_Dlt spec (PRS_Dlt_00197) defines these as uint8, but the COVESA
// dlt-daemon / dlt-control and the Wireshark DLT dissector encode them as uint16.
// Must match the daemon that produced the response, otherwise every field after the
// first description desyncs.
enum class DescriptionLengthWidth {
    Uint16, // COVESA dlt-daemon / Wireshark (default)
    Uint8,  // AUTOSAR PRS_Dlt / Vector
};

QDLT_EXPORT Type parse(const QByteArray&, bool isBigEndian,
                       DescriptionLengthWidth descLenWidth = DescriptionLengthWidth::Uint16);

// Same as parse(), but transparently handles the two GET_LOG_INFO description
// length encodings: the COVESA/uint16 layout is tried first and, if a
// GET_LOG_INFO response desyncs on it, the AUTOSAR/uint8 layout is retried.
QDLT_EXPORT Type parseAutoDetect(const QByteArray&, bool isBigEndian);

// One decoded parameter of a control message payload.
struct Field {
    QString name;
    QString value;
};

// Decodes the parameters of a control REQUEST payload (everything behind the
// service id) into printable name/value pairs. Services without parameters
// yield an empty list; a truncated or unknown payload yields a single entry
// holding the remaining raw bytes.
QDLT_EXPORT QList<Field> describeRequest(const QByteArray& payload, bool isBigEndian);

// Decodes a control RESPONSE payload into printable name/value pairs. The
// nested application/context data of a GET_LOG_INFO response is not included
// here - use parseAutoDetect() for that.
QDLT_EXPORT QList<Field> describeResponse(const QByteArray& payload, bool isBigEndian);

// Human readable form of a control response status / log level / trace status.
QDLT_EXPORT QString statusString(uint8_t status);
QDLT_EXPORT QString logLevelString(int8_t logLevel);
QDLT_EXPORT QString traceStatusString(int8_t traceStatus);

} // namespace qdlt::msg::payload

// helper type for the visitor
template<class... Ts>
struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

#endif // QDLTCTRLMSG_H

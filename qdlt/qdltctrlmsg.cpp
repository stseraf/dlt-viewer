#include "qdltctrlmsg.h"

#include "dlt_common.h"

#include <exception>
#include <stdexcept>
#include <cstring>
#include <array>

#include <QByteArray>

namespace qdlt::msg::payload {

namespace {
using IdType = std::array<char, DLT_ID_SIZE>;

QString asQString(IdType&& id) {
    QString str;
    for (size_t i = 0; i < id.size() && id[i] != '\0'; i++) {
        str.append(id[i]);
    }
    return str;
}

template <typename T>
T dltPayloadRead(const char *&dataPtr, int32_t &length, bool isBigEndian)
{
    if (length < 0 || length < static_cast<int32_t>(sizeof(T))) {
        throw std::runtime_error("Invalid data length");
    }

    T value{};
    std::memcpy(&value, dataPtr, sizeof(T));
    dataPtr += sizeof(T);
    length -= sizeof(T);

    if constexpr (sizeof(T) == sizeof(uint32_t)) {
        value = DLT_ENDIAN_GET_32((isBigEndian ? DLT_HTYP_MSBF : 0), value);
    }

    if constexpr (sizeof(T) == sizeof(uint16_t)) {
        value = DLT_ENDIAN_GET_16((isBigEndian ? DLT_HTYP_MSBF : 0), value);
    }

    return value;
}

IdType dltPayloadReadId(const char *&dataPtr, int32_t &length)
{
    if (length < 0 || length < DLT_ID_SIZE) {
        throw std::runtime_error("Invalid ID length");
    }
    IdType id{};
    std::copy(dataPtr, dataPtr + DLT_ID_SIZE, id.begin());
    dataPtr += DLT_ID_SIZE;
    length -= DLT_ID_SIZE;

    return id;
}

std::string dltPayloadReadString(const char *&dataPtr, int32_t &length, bool isBigEndian,
                                 DescriptionLengthWidth descLenWidth)
{
    uint16_t strLength = (descLenWidth == DescriptionLengthWidth::Uint8)
                             ? dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian)
                             : dltPayloadRead<uint16_t>(dataPtr, length, isBigEndian);
    if (length < 0 || strLength > length) {
        throw std::runtime_error(QString("Invalid string length %1 > %2").arg(strLength).arg(length).toStdString());
    }
    std::string str;
    str.assign(dataPtr, strLength);
    dataPtr += strLength;
    length -= strLength;

    return str;
}
}

Type parse(const QByteArray& data, bool isBigEndian, DescriptionLengthWidth descLenWidth)
{
    int32_t length = data.length();
    const char *dataPtr = data.data();
    uint32_t serviceId = 0;

    try {
        serviceId = dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian);
        switch (serviceId) {
        case DLT_SERVICE_ID_GET_LOG_INFO:
        {
            GetLogInfo msg;
            msg.status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            if ((msg.status != 6) && (msg.status != 7)) {
                return msg;
            }

            const auto numApps = dltPayloadRead<uint16_t>(dataPtr, length, isBigEndian);
            for (uint16_t i = 0; i < numApps; i++) {
                GetLogInfo::App app;
                app.id = asQString(dltPayloadReadId(dataPtr, length));
                const uint16_t numCtx = dltPayloadRead<uint16_t>(dataPtr, length, isBigEndian);
                for (uint16_t j = 0; j < numCtx; j++) {
                    GetLogInfo::App::Ctx ctx;
                    ctx.id = asQString(dltPayloadReadId(dataPtr, length));
                    ctx.logLevel = dltPayloadRead<int8_t>(dataPtr, length, isBigEndian);
                    ctx.traceStatus = dltPayloadRead<int8_t>(dataPtr, length, isBigEndian);
                    ctx.description = QString::fromStdString(dltPayloadReadString(dataPtr, length, isBigEndian, descLenWidth));
                    app.ctxs.push_back(std::move(ctx));
                }
                if (msg.status == 7) {
                    app.description = QString::fromStdString(dltPayloadReadString(dataPtr, length, isBigEndian, descLenWidth));
                }
                msg.apps.push_back(std::move(app));
            }
            return msg;
        }
        case DLT_SERVICE_ID_GET_SOFTWARE_VERSION:
        {
            return GetSoftwareVersion{};
        }
        case DLT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL:
        {
            GetDefaultLogLevel msg;
            msg.logLevel = dltPayloadRead<int8_t>(dataPtr, length, isBigEndian);
            msg.status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            return msg;
        }
        case DLT_SERVICE_ID_SET_LOG_LEVEL:
        {
            SetLogLevel msg;
            msg.status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            return msg;
        }
        case DLT_SERVICE_ID_TIMEZONE:
        {
            Timezone msg;
            msg.status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            msg.timezone = dltPayloadRead<int32_t>(dataPtr, length, isBigEndian);
            msg.isDst = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            return msg;
        }
        case DLT_SERVICE_ID_UNREGISTER_CONTEXT:
        {
            UnregisterContext msg;
            msg.status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            msg.appid = asQString(dltPayloadReadId(dataPtr, length));
            msg.ctxid = asQString(dltPayloadReadId(dataPtr, length));
            return msg;
        }
        }

        return Uninteresting{serviceId, false};
    } catch (const std::exception&) {
        return Uninteresting{serviceId, true};
    } catch (...) {
        return Uninteresting{serviceId, true};
    }
}

Type parseAutoDetect(const QByteArray& data, bool isBigEndian)
{
    auto result = parse(data, isBigEndian, DescriptionLengthWidth::Uint16);
    if (const auto* uninteresting = std::get_if<Uninteresting>(&result);
            uninteresting && uninteresting->parseError &&
            uninteresting->serviceId == DLT_SERVICE_ID_GET_LOG_INFO) {
        auto retry = parse(data, isBigEndian, DescriptionLengthWidth::Uint8);
        if (!std::holds_alternative<Uninteresting>(retry)) {
            return retry;
        }
    }
    return result;
}

QString statusString(uint8_t status)
{
    switch (status) {
    case 0: return QString("0 (ok)");
    case 1: return QString("1 (not supported)");
    case 2: return QString("2 (error)");
    case 8: return QString("8 (no matching context id)");
    default: return QString::number(status);
    }
}

QString logLevelString(int8_t level)
{
    switch (level) {
    case -1: return QString("-1 (default)");
    case 0:  return QString("0 (off)");
    case 1:  return QString("1 (fatal)");
    case 2:  return QString("2 (error)");
    case 3:  return QString("3 (warn)");
    case 4:  return QString("4 (info)");
    case 5:  return QString("5 (debug)");
    case 6:  return QString("6 (verbose)");
    default: return QString::number(level);
    }
}

QString traceStatusString(int8_t status)
{
    switch (status) {
    case -1: return QString("-1 (default)");
    case 0:  return QString("0 (off)");
    case 1:  return QString("1 (on)");
    default: return QString::number(status);
    }
}

namespace {

QString onOffString(uint8_t status)
{
    switch (status) {
    case 0:  return QString("0 (off)");
    case 1:  return QString("1 (on)");
    default: return QString::number(status);
    }
}

// An id field that is completely zeroed acts as a wildcard, e.g. "all contexts
// of this application" - show that instead of an empty cell.
QString idString(IdType&& id)
{
    QString str = asQString(std::move(id));
    return str.isEmpty() ? QString("(any)") : str;
}

QString remainingBytes(const char *dataPtr, int32_t length)
{
    return QString(QByteArray(dataPtr, length).toHex(' '));
}

} // namespace

QList<Field> describeRequest(const QByteArray& data, bool isBigEndian)
{
    int32_t length = data.length();
    const char *dataPtr = data.data();
    QList<Field> fields;

    try {
        const uint32_t serviceId = dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian);

        if (serviceId >= DLT_SERVICE_ID_CALLSW_CINJECTION) {
            fields.append({QString("service_id"), QString::number(serviceId)});
            const uint32_t dataLength = dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian);
            fields.append({QString("length"), QString::number(dataLength)});
            fields.append({QString("data"), remainingBytes(dataPtr, length)});
            return fields;
        }

        switch (serviceId) {
        case DLT_SERVICE_ID_SET_LOG_LEVEL:
            fields.append({QString("apid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("ctid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("log_level"), logLevelString(dltPayloadRead<int8_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_SET_TRACE_STATUS:
            fields.append({QString("apid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("ctid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("trace_status"), traceStatusString(dltPayloadRead<int8_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_GET_LOG_INFO:
        {
            const uint8_t options = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
            QString optionsText = QString::number(options);
            switch (options) {
            case 3: optionsText += " (ids only)"; break;
            case 4: optionsText += " (with log level)"; break;
            case 5: optionsText += " (with log level and trace status)"; break;
            case 6: optionsText += " (with context description)"; break;
            case 7: optionsText += " (with context and application description)"; break;
            default: break;
            }
            fields.append({QString("options"), optionsText});
            fields.append({QString("apid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("ctid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        }
        case DLT_SERVICE_ID_SET_DEFAULT_LOG_LEVEL:
            fields.append({QString("log_level"), logLevelString(dltPayloadRead<int8_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_SET_DEFAULT_TRACE_STATUS:
            fields.append({QString("trace_status"), traceStatusString(dltPayloadRead<int8_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_SET_COM_INTERFACE_STATUS:
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("new_status"), onOffString(dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_SET_COM_INTERFACE_MAX_BANDWIDTH:
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("max_bandwidth"), QString::number(dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_SET_VERBOSE_MODE:
        case DLT_SERVICE_ID_SET_MESSAGE_FILTERING:
        case DLT_SERVICE_ID_SET_TIMING_PACKETS:
        case DLT_SERVICE_ID_USE_ECU_ID:
        case DLT_SERVICE_ID_USE_SESSION_ID:
        case DLT_SERVICE_ID_USE_TIMESTAMP:
        case DLT_SERVICE_ID_USE_EXTENDED_HEADER:
            fields.append({QString("new_status"), onOffString(dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL:
        case DLT_SERVICE_ID_STORE_CONFIG:
        case DLT_SERVICE_ID_RESET_TO_FACTORY_DEFAULT:
        case DLT_SERVICE_ID_GET_LOCAL_TIME:
        case DLT_SERVICE_ID_GET_SOFTWARE_VERSION:
            /* no parameters */
            break;
        default:
            if (length > 0) {
                fields.append({QString("data"), remainingBytes(dataPtr, length)});
            }
            break;
        }
    } catch (const std::exception&) {
        fields.append({QString("(truncated payload)"), QString(data.toHex(' '))});
    } catch (...) {
        fields.append({QString("(truncated payload)"), QString(data.toHex(' '))});
    }

    return fields;
}

QList<Field> describeResponse(const QByteArray& data, bool isBigEndian)
{
    QList<Field> fields;

    int32_t length = data.length();
    const char *dataPtr = data.data();
    uint32_t serviceId = 0;

    try {
        serviceId = dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian);

        if (serviceId == DLT_SERVICE_ID_MARKER) {
            return fields;
        }

        /* Every response but the marker starts with the status byte. For
         * get_log_info it echoes the requested options instead of ok/error. */
        const uint8_t status = dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian);
        if (serviceId == DLT_SERVICE_ID_GET_LOG_INFO) {
            QString statusText = QString::number(status);
            switch (status) {
            case 1: statusText += " (not supported)"; break;
            case 2: statusText += " (error)"; break;
            case 3: statusText += " (ids only)"; break;
            case 4: statusText += " (with log level)"; break;
            case 5: statusText += " (with log level and trace status)"; break;
            case 6: statusText += " (with context description)"; break;
            case 7: statusText += " (with context and application description)"; break;
            case 8: statusText += " (no matching context id)"; break;
            default: break;
            }
            fields.append({QString("status"), statusText});
        }
        else {
            fields.append({QString("status"), statusString(status)});
        }

        switch (serviceId) {
        case DLT_SERVICE_ID_GET_SOFTWARE_VERSION:
        {
            const uint32_t versionLength = dltPayloadRead<uint32_t>(dataPtr, length, isBigEndian);
            if (static_cast<int32_t>(versionLength) > length) {
                throw std::runtime_error("Invalid version length");
            }
            fields.append({QString("length"), QString::number(versionLength)});
            fields.append({QString("version"), QString::fromLatin1(dataPtr, versionLength).trimmed()});
            break;
        }
        case DLT_SERVICE_ID_GET_DEFAULT_LOG_LEVEL:
            fields.append({QString("log_level"), logLevelString(dltPayloadRead<int8_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_MESSAGE_BUFFER_OVERFLOW:
            fields.append({QString("overflow"), onOffString(dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_UNREGISTER_CONTEXT:
            fields.append({QString("apid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("ctid"), idString(dltPayloadReadId(dataPtr, length))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_CONNECTION_INFO:
            fields.append({QString("state"), onOffString(dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("com"), idString(dltPayloadReadId(dataPtr, length))});
            break;
        case DLT_SERVICE_ID_TIMEZONE:
            fields.append({QString("timezone"), QString("%1 s").arg(dltPayloadRead<int32_t>(dataPtr, length, isBigEndian))});
            fields.append({QString("isdst"), QString::number(dltPayloadRead<uint8_t>(dataPtr, length, isBigEndian))});
            break;
        case DLT_SERVICE_ID_GET_LOG_INFO:
            /* Nested application/context data, see parseAutoDetect(). */
            break;
        default:
            if (length > 0) {
                fields.append({QString("data"), remainingBytes(dataPtr, length)});
            }
            break;
        }
    } catch (const std::exception&) {
        fields.append({QString("(truncated payload)"), QString(data.toHex(' '))});
    } catch (...) {
        fields.append({QString("(truncated payload)"), QString(data.toHex(' '))});
    }

    return fields;
}

}

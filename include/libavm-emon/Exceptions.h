#pragma once

#include "emon_export.h"

#include <QtCore/QString>
#include <stdexcept>

namespace avm_emon {

// Base for every exception this library throws. Carries the message as a
// QString (message()) in addition to std::runtime_error::what(), so
// callers can use either depending on whether they're already in Qt-land.
class AVM_EMON_EXPORT Exception : public std::runtime_error {
public:
    explicit Exception(const QString &message)
        : std::runtime_error(message.toStdString())
        , m_message(message)
    {
    }

    const QString &message() const noexcept { return m_message; }

private:
    QString m_message;
};

// Frame is malformed, or doesn't match the wire format (bad sync byte,
// bad CRC, truncated/timed-out read, ...). See md/mindflow.md for the
// confirmed frame format this is checked against.
class AVM_EMON_EXPORT ProtocolError : public Exception {
public:
    using Exception::Exception;
};

// Frame was well-formed but the device reported NOT-NORMA (status 0xFF),
// or the caller used the API incorrectly (e.g. called a command before
// connecting).
class AVM_EMON_EXPORT DeviceError : public Exception {
public:
    using Exception::Exception;
};

} // namespace avm_emon

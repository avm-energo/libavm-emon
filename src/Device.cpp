#include <libavm-emon/Device.h>
#include <libavm-emon/Exceptions.h>

#include "Codecs.h"
#include "Constants.h"
#include "Frame.h"
#include "Transport.h"

#include <utility>

namespace avm_emon {

namespace {

void requireConnected(const std::unique_ptr<detail::Transport> &transport)
{
    if (!transport)
        throw ProtocolError(QStringLiteral("not connected - call connectDevice() first"));
}

QByteArray callMode(detail::Transport &transport, std::uint8_t mode, const QByteArray &payload = {})
{
    QByteArray request = detail::buildRequest(mode, payload);
    QByteArray raw = transport.request(request);
    detail::Response response = detail::parseResponse(raw);
    if (!response.ok) {
        throw DeviceError(
            QStringLiteral("device returned NOT-NORMA for mode 0x%1").arg(mode, 2, 16, QLatin1Char('0')));
    }
    return response.payload;
}

std::uint16_t readU16(const QByteArray &data, int offset)
{
    return static_cast<std::uint8_t>(data[offset])
         | (static_cast<std::uint16_t>(static_cast<std::uint8_t>(data[offset + 1])) << 8);
}

void appendU16(QByteArray &out, std::uint16_t value)
{
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

} // namespace

Device::Device(QString portName, LinkType linkType, int timeoutMs)
    : m_portName(std::move(portName))
    , m_linkType(linkType)
    , m_timeoutMs(timeoutMs)
{
}

Device::~Device() = default;

void Device::connectDevice()
{
    m_transport = std::make_unique<detail::Transport>(m_portName, m_linkType, m_timeoutMs);
}

void Device::disconnectDevice()
{
    m_transport.reset();
}

bool Device::isConnected() const
{
    return static_cast<bool>(m_transport);
}

DeviceInfo Device::getDeviceInfo()
{
    requireConnected(m_transport);

    // Mode 15, start word 0, count 256 words (512 bytes) - the system
    // area is exactly one FRAM "block closed by CRC", see md/mindflow.md.
    QByteArray payload;
    appendU16(payload, 0);
    appendU16(payload, 256);

    QByteArray raw = callMode(*m_transport, static_cast<std::uint8_t>(detail::Mode::ReadFram), payload);
    if (raw.size() < 28) {
        throw ProtocolError(QStringLiteral("unexpected device-info response size: %1, expected >= 28").arg(raw.size()));
    }

    DeviceInfo info;
    info.deviceType = detail::decodeAscii(detail::swapWordBytes(raw.mid(0, 16)));
    info.serialNumber = readU16(raw, 16);
    info.swVersionRawWord0 = readU16(raw, 18);
    info.swVersionRawWord1 = readU16(raw, 20);
    info.swVersion = QStringLiteral("%1.%2")
                          .arg(detail::decodeDaaDigitString(info.swVersionRawWord0, 4).toInt())
                          .arg(detail::decodeDaaDigitString(info.swVersionRawWord1, 4).toInt());
    detail::DaaDate date = detail::decodeDaaDate(readU16(raw, 22), readU16(raw, 24));
    info.swVersionYear = date.year;
    info.swVersionMonth = date.month;
    info.swVersionDay = date.day;
    info.deviceTypeId = readU16(raw, 26);
    return info;
}

AppliedLimits Device::setLimits(std::uint16_t currentLimitType, std::uint16_t currentLimitIndex,
                                 std::uint16_t voltageLimitIndex)
{
    requireConnected(m_transport);

    QByteArray payload;
    appendU16(payload, currentLimitType);
    appendU16(payload, currentLimitIndex);
    appendU16(payload, voltageLimitIndex);

    QByteArray raw = callMode(*m_transport, static_cast<std::uint8_t>(detail::Mode::SetLimits), payload);
    if (raw.size() != 10) {
        throw ProtocolError(QStringLiteral("unexpected setLimits response size: %1, expected 10").arg(raw.size()));
    }

    AppliedLimits applied;
    applied.currentLimitType = readU16(raw, 0);
    applied.currentLimitIndex = readU16(raw, 2);
    applied.voltageLimitIndex = readU16(raw, 4);
    applied.accessLevel = readU16(raw, 6);
    applied.wiringScheme = readU16(raw, 8);
    return applied;
}

SupportedLimits Device::getSupportedLimits()
{
    requireConnected(m_transport);

    QByteArray payload;
    appendU16(payload, 0);
    appendU16(payload, 256);
    QByteArray raw = callMode(*m_transport, static_cast<std::uint8_t>(detail::Mode::ReadFram), payload);
    if (raw.size() < detail::kLimitsTableByteOffset) {
        throw ProtocolError(
            QStringLiteral("unexpected supported-limits response size: %1").arg(raw.size()));
    }

    static const QHash<std::uint16_t, int> kDivisorByType = {
        {static_cast<std::uint16_t>(detail::LimitTableType::Voltage), 64},
        {static_cast<std::uint16_t>(detail::LimitTableType::CurrentTransformer), 512},
        {static_cast<std::uint16_t>(detail::LimitTableType::CurrentClamp), 512},
        {static_cast<std::uint16_t>(detail::LimitTableType::CurrentUptt), 512},
    };
    auto entries = detail::decodeLimitsTable(raw.mid(detail::kLimitsTableByteOffset), kDivisorByType);

    SupportedLimits limits;
    for (const auto &[type, value] : entries) {
        switch (static_cast<detail::LimitTableType>(type)) {
        case detail::LimitTableType::Voltage: limits.voltage.append(value); break;
        case detail::LimitTableType::CurrentTransformer: limits.currentTransformer.append(value); break;
        case detail::LimitTableType::CurrentClamp: limits.currentClamp.append(value); break;
        case detail::LimitTableType::CurrentUptt: limits.currentUptt.append(value); break;
        case detail::LimitTableType::NoMoreLimits: break;
        }
    }
    return limits;
}

QByteArray Device::getAnalogInputsRaw()
{
    requireConnected(m_transport);
    return callMode(*m_transport, static_cast<std::uint8_t>(detail::Mode::CalcAll));
}

AnalogInputs Device::getAnalogInputs()
{
    requireConnected(m_transport);

    QByteArray payload = callMode(*m_transport, static_cast<std::uint8_t>(detail::Mode::CalcAll));
    if (payload.size() != detail::kResponsePayloadBytes) {
        throw ProtocolError(QStringLiteral("unexpected analog-inputs payload size: %1, expected %2")
                                 .arg(payload.size())
                                 .arg(detail::kResponsePayloadBytes));
    }

    auto rms = [&payload](int channelIndex) {
        int off = detail::channelBlockByteOffset(channelIndex) + detail::kField5RmsWordOffset * 2;
        return detail::decodeUFixed(payload.mid(off, 4), 20);
    };
    auto angle = [&payload](int channelIndex) {
        int off = detail::channelBlockByteOffset(channelIndex) + detail::kField11AngleWordOffset * 2;
        return detail::decodeSignedFixed(payload.mid(off, 4), 16, 32);
    };

    auto uuAngle = [&payload](int channelIndex) {
        int off = detail::channelBlockByteOffset(channelIndex) + detail::kField12AngleUuWordOffset * 2;
        return detail::decodeSignedFixed(payload.mid(off, 4), 16, 32);
    };

    int freqOff = detail::kFooterByteOffset + detail::kField33FrequencyFooterWordOffset * 2;
    double frequency = detail::decodeUFixed(payload.mid(freqOff, 4), 16);

    int limit45Off = detail::kFooterByteOffset + detail::kField45CurrentLimitFooterWordOffset * 2;
    std::uint16_t limit45 = readU16(payload, limit45Off);
    int limit46Off = detail::kFooterByteOffset + detail::kField46VoltageLimitFooterWordOffset * 2;

    AnalogInputs result;
    result.Ia = rms(detail::ChIa);
    result.Ua = rms(detail::ChUa);
    result.Ib = rms(detail::ChIb);
    result.Ub = rms(detail::ChUb);
    result.Ic = rms(detail::ChIc);
    result.Uc = rms(detail::ChUc);
    result.frequency = frequency;
    result.phiIaUa = angle(detail::ChIa);
    result.phiIbUb = angle(detail::ChIb);
    result.phiIcUc = angle(detail::ChIc);
    result.phiUaUb = uuAngle(detail::ChUa);
    result.phiUbUc = uuAngle(detail::ChUb);
    result.phiUcUa = uuAngle(detail::ChUc);
    result.appliedCurrentLimitType = static_cast<std::uint16_t>((limit45 >> 8) & 0xFF);
    result.appliedCurrentLimitIndex = static_cast<std::uint16_t>(limit45 & 0xFF);
    result.appliedVoltageLimitIndex = readU16(payload, limit46Off);
    return result;
}

} // namespace avm_emon

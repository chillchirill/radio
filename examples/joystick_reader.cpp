#include "joystick_reader.hpp"

namespace {

std::uint16_t ToUint16(DWORD value) {
    constexpr DWORD kMaxUint16 = 0xFFFF;
    return static_cast<std::uint16_t>(value > kMaxUint16 ? kMaxUint16 : value);
}

bool TryReadJoystickInfo(UINT device_id, JOYINFOEX& info) {
    JOYCAPSA caps{};
    if (joyGetDevCapsA(device_id, &caps, sizeof(caps)) != JOYERR_NOERROR) {
        return false;
    }

    info = {};
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV;
    return joyGetPosEx(device_id, &info) == JOYERR_NOERROR;
}

std::optional<UINT> ResolveDeviceId(const std::optional<UINT>& requested_device_id) {
    JOYINFOEX info{};

    if (requested_device_id.has_value()) {
        return TryReadJoystickInfo(*requested_device_id, info) ? requested_device_id : std::nullopt;
    }

    const UINT device_count = joyGetNumDevs();
    for (UINT device_id = 0; device_id < device_count; ++device_id) {
        if (TryReadJoystickInfo(device_id, info)) {
            return device_id;
        }
    }

    return std::nullopt;
}

JoystickState MakeState(const JOYINFOEX& info) {
    return JoystickState{
        ToUint16(info.dwVpos),
        ToUint16(info.dwZpos),
        ToUint16(info.dwXpos),
        ToUint16(info.dwYpos),
    };
}

}  // namespace

JoystickReader::JoystickReader(std::optional<UINT> device_id)
    : requested_device_id_(device_id) {
}

bool JoystickReader::IsConnected() {
    return ResolveDeviceId(requested_device_id_).has_value();
}

std::optional<UINT> JoystickReader::GetDeviceId() {
    return ResolveDeviceId(requested_device_id_);
}

std::optional<JoystickState> JoystickReader::Read() {
    const auto device_id = ResolveDeviceId(requested_device_id_);
    if (!device_id.has_value()) {
        return std::nullopt;
    }

    JOYINFOEX info{};
    if (!TryReadJoystickInfo(*device_id, info)) {
        return std::nullopt;
    }

    return MakeState(info);
}

std::optional<JoystickState> ReadJoystickState(std::optional<UINT> device_id) {
    return JoystickReader(device_id).Read();
}

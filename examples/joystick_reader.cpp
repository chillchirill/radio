#include "joystick_reader.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct DeviceInfo {
    UINT id = 0;
    JOYCAPSA caps{};
};

std::uint16_t ToUint16(DWORD value) {
    constexpr DWORD kMaxUint16 = 0xFFFF;
    return static_cast<std::uint16_t>(value > kMaxUint16 ? kMaxUint16 : value);
}

std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool ContainsInsensitive(std::string_view text, std::string_view needle) {
    return ToLower(std::string(text)).find(ToLower(std::string(needle))) != std::string::npos;
}

std::vector<DeviceInfo> EnumerateDevices() {
    std::vector<DeviceInfo> devices;
    const UINT count = joyGetNumDevs();

    for (UINT id = 0; id < count; ++id) {
        JOYCAPSA caps{};
        if (joyGetDevCapsA(id, &caps, sizeof(caps)) == JOYERR_NOERROR) {
            devices.push_back(DeviceInfo{ id, caps });
        }
    }

    return devices;
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

std::optional<UINT> SelectPreferredDeviceId(
    const std::vector<DeviceInfo>& devices,
    const std::optional<UINT>& requested_device_id) {
    if (devices.empty()) {
        return std::nullopt;
    }

    if (requested_device_id.has_value()) {
        for (const auto& device : devices) {
            if (device.id == *requested_device_id) {
                return device.id;
            }
        }
        return std::nullopt;
    }

    static constexpr std::string_view kAutoNames[] = {
        "radiomaster",
        "edgetx",
        "opentx",
        "tx16",
        "boxer",
        "zorro"
    };

    for (std::string_view expected : kAutoNames) {
        for (const auto& device : devices) {
            if (ContainsInsensitive(device.caps.szPname, expected)) {
                return device.id;
            }
        }
    }

    return devices.front().id;
}

std::optional<UINT> ResolveDeviceId(const std::optional<UINT>& requested_device_id) {
    const auto devices = EnumerateDevices();
    const auto selected_device_id = SelectPreferredDeviceId(devices, requested_device_id);
    if (!selected_device_id.has_value()) {
        return std::nullopt;
    }

    JOYINFOEX info{};
    return TryReadJoystickInfo(*selected_device_id, info) ? selected_device_id : std::nullopt;
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
    if (resolved_device_id_.has_value()) {
        JOYINFOEX info{};
        return TryReadJoystickInfo(*resolved_device_id_, info);
    }

    resolved_device_id_ = ResolveDeviceId(requested_device_id_);
    return resolved_device_id_.has_value();
}

std::optional<UINT> JoystickReader::GetDeviceId() {
    if (!resolved_device_id_.has_value()) {
        resolved_device_id_ = ResolveDeviceId(requested_device_id_);
    }

    return resolved_device_id_;
}

std::optional<JoystickState> JoystickReader::Read() {
    const auto device_id = GetDeviceId();
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

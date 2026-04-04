#include "joystick_reader.hpp"

namespace {

std::uint16_t ToUint16(DWORD value) {
    constexpr DWORD kMaxUint16 = 0xFFFF;
    return static_cast<std::uint16_t>(value > kMaxUint16 ? kMaxUint16 : value);
}

}  // namespace

std::optional<JoystickState> ReadJoystickState(UINT device_id) {
    JOYINFOEX info{};
    info.dwSize = sizeof(info);
    info.dwFlags = JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV;

    if (joyGetPosEx(device_id, &info) != JOYERR_NOERROR) {
        return std::nullopt;
    }

    return JoystickState{
        ToUint16(info.dwXpos),
        ToUint16(info.dwYpos),
        ToUint16(info.dwZpos),
        ToUint16(info.dwRpos),
        ToUint16(info.dwUpos),
        ToUint16(info.dwVpos),
    };
}

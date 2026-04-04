#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <optional>

struct JoystickState {
    std::uint16_t x = 0;
    std::uint16_t y = 0;
    std::uint16_t z = 0;
    std::uint16_t r = 0;
    std::uint16_t u = 0;
    std::uint16_t v = 0;
};

std::optional<JoystickState> ReadJoystickState(UINT device_id);

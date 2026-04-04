#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include <cstdint>
#include <optional>

struct JoystickState {
    std::uint16_t v = 0;
    std::uint16_t z = 0;
    std::uint16_t x = 0;
    std::uint16_t y = 0;
};

class JoystickReader {
public:
    explicit JoystickReader(std::optional<UINT> device_id = std::nullopt);

    bool IsConnected();
    std::optional<UINT> GetDeviceId();
    std::optional<JoystickState> Read();

private:
    std::optional<UINT> requested_device_id_;
};

std::optional<JoystickState> ReadJoystickState(std::optional<UINT> device_id = std::nullopt);

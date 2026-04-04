#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>

#include "joystick_reader.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

std::atomic_bool g_running = true;

struct DeviceInfo {
    UINT id = 0;
    JOYCAPSA caps{};
};

struct Config {
    int hz = 120;
    bool list_only = false;
    bool csv = false;
    std::optional<UINT> device_id;
    std::string name_filter;
};

class TimerResolutionGuard {
public:
    TimerResolutionGuard() {
        active_ = (timeBeginPeriod(1) == TIMERR_NOERROR);
    }

    ~TimerResolutionGuard() {
        if (active_) {
            timeEndPeriod(1);
        }
    }

    TimerResolutionGuard(const TimerResolutionGuard&) = delete;
    TimerResolutionGuard& operator=(const TimerResolutionGuard&) = delete;

private:
    bool active_ = false;
};

BOOL WINAPI ConsoleHandler(DWORD signal) {
    switch (signal) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_running = false;
            return TRUE;
        default:
            return FALSE;
    }
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

void PrintUsage(const char* exe_name) {
    std::cout
        << "Usage:\n"
        << "  " << exe_name << " [--list] [--device N] [--name SUBSTRING] [--hz 120]\n"
        << "                 [--csv]\n\n"
        << "Options:\n"
        << "  --list          Show available joysticks and exit.\n"
        << "  --device N      Use a specific joystick id.\n"
        << "  --name TEXT     Pick a device by partial name match.\n"
        << "  --hz N          Polling rate, for example 120 or 240.\n"
        << "  --csv           Print raw X,Y,Z,R,U,V on every line.\n"
        << "  --help          Show this help.\n";
}

bool ParseUInt(std::string_view text, UINT& out) {
    try {
        size_t pos = 0;
        const auto value = std::stoul(std::string(text), &pos);
        if (pos != text.size() || value > std::numeric_limits<UINT>::max()) {
            return false;
        }
        out = static_cast<UINT>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool ParseInt(std::string_view text, int& out) {
    try {
        size_t pos = 0;
        const auto value = std::stoi(std::string(text), &pos);
        if (pos != text.size()) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<Config> ParseArgs(int argc, char** argv) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[i];

        auto require_value = [&](const char* flag) -> std::optional<std::string_view> {
            if (i + 1 >= argc) {
                std::cerr << "Option " << flag << " requires a value.\n";
                return std::nullopt;
            }
            return argv[++i];
        };

        if (arg == "--help" || arg == "-h") {
            PrintUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "--list") {
            config.list_only = true;
            continue;
        }
        if (arg == "--csv") {
            config.csv = true;
            continue;
        }
        if (arg == "--device") {
            const auto value = require_value("--device");
            if (!value) {
                return std::nullopt;
            }
            UINT parsed = 0;
            if (!ParseUInt(*value, parsed)) {
                std::cerr << "Invalid joystick id: " << *value << "\n";
                return std::nullopt;
            }
            config.device_id = parsed;
            continue;
        }
        if (arg == "--name") {
            const auto value = require_value("--name");
            if (!value) {
                return std::nullopt;
            }
            config.name_filter = std::string(*value);
            continue;
        }
        if (arg == "--hz") {
            const auto value = require_value("--hz");
            if (!value) {
                return std::nullopt;
            }
            if (!ParseInt(*value, config.hz) || config.hz <= 0 || config.hz > 1000) {
                std::cerr << "Invalid polling rate: " << *value << "\n";
                return std::nullopt;
            }
            continue;
        }

        std::cerr << "Unknown option: " << arg << "\n";
        PrintUsage(argv[0]);
        return std::nullopt;
    }

    return config;
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

void PrintDevices(const std::vector<DeviceInfo>& devices) {
    if (devices.empty()) {
        std::cout << "No joysticks found.\n";
        return;
    }

    std::cout << "Detected joysticks:\n";
    for (const auto& device : devices) {
        std::cout
            << "  id=" << device.id
            << "  name=\"" << device.caps.szPname << "\""
            << "  X[" << device.caps.wXmin << ".." << device.caps.wXmax << "]"
            << "  Y[" << device.caps.wYmin << ".." << device.caps.wYmax << "]\n";
    }
}

const DeviceInfo* SelectDevice(const std::vector<DeviceInfo>& devices, const Config& config) {
    if (devices.empty()) {
        return nullptr;
    }

    if (config.device_id.has_value()) {
        for (const auto& device : devices) {
            if (device.id == *config.device_id) {
                return &device;
            }
        }
        return nullptr;
    }

    if (!config.name_filter.empty()) {
        for (const auto& device : devices) {
            if (ContainsInsensitive(device.caps.szPname, config.name_filter)) {
                return &device;
            }
        }
        return nullptr;
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
                return &device;
            }
        }
    }

    return &devices.front();
}

void PrintLiveLine(const JoystickState& state, int hz) {
    std::cout
        << '\r'
        << "X:" << std::setw(6) << state.x
        << " Y:" << std::setw(6) << state.y
        << " Z:" << std::setw(6) << state.z
        << " R:" << std::setw(6) << state.r
        << " U:" << std::setw(6) << state.u
        << " V:" << std::setw(6) << state.v
        << "  @ " << hz << " Hz   ";
    std::cout.flush();
}

}  // namespace

int main(int argc, char** argv) {
    const auto config = ParseArgs(argc, argv);
    if (!config.has_value()) {
        return 1;
    }

    if (!SetConsoleCtrlHandler(ConsoleHandler, TRUE)) {
        std::cerr << "Failed to install Ctrl+C handler.\n";
        return 1;
    }

    const auto devices = EnumerateDevices();
    if (config->list_only) {
        PrintDevices(devices);
        return devices.empty() ? 1 : 0;
    }

    const DeviceInfo* device = SelectDevice(devices, *config);
    if (device == nullptr) {
        std::cerr << "Requested joystick was not found.\n";
        PrintDevices(devices);
        return 1;
    }

    std::cout
        << "Using joystick id=" << device->id
        << "  name=\"" << device->caps.szPname << "\"\n"
        << "Showing raw HID axes X/Y/Z/R/U/V.\n"
        << "Press Ctrl+C to stop.\n";

    TimerResolutionGuard timer_resolution_guard;

    using clock = std::chrono::steady_clock;
    const auto period = std::chrono::nanoseconds(static_cast<long long>(std::llround(1'000'000'000.0 / config->hz)));
    auto next_tick = clock::now();

    while (g_running) {
        const auto state = ReadJoystickState(device->id);
        if (!state.has_value()) {
            std::cerr << "\nFailed to read joystick data.\n";
            return 1;
        }
        // Тут!!!!!!! запускаю dc для надсилання даних
        if (config->csv) {
            std::cout
                << state->x << ","
                << state->y << ","
                << state->z << ","
                << state->r << ","
                << state->u << ","
                << state->v << "\n";
        } else {
            PrintLiveLine(*state, config->hz);
        }

        next_tick += period;
        auto now = clock::now();
        if (next_tick <= now) {
            next_tick = now + period;
            continue;
        }

        const auto spin_window = std::chrono::microseconds(500);
        if (next_tick - now > spin_window) {
            std::this_thread::sleep_until(next_tick - spin_window);
        }
        while (clock::now() < next_tick) {
            std::this_thread::yield();
        }
    }

    if (!config->csv) {
        std::cout << "\n";
    }

    return 0;
}

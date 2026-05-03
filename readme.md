# Joystick Control over UDP

This project converts a joystick-controlled device so it can be controlled over the internet.

The idea is to read joystick stick positions on a Windows PC, send them over UDP, and reproduce the same analog voltages on the remote side with DAC outputs. Those DAC voltages replace the voltages that normally come from the joystick potentiometers.

The control side consists of:

- a Windows PC
- a connected joystick, including a game controller
- internet access with a reachable public/static address, or another route such as VPN
- port forwarding on the router if the device must be reached from outside the local network

The drone/device side consists of:

- the original joystick/controller that used to control the device
- two DAC boards/chips
- Raspberry Pi
- RC-HD video input through an HDMI-to-MIPI CSI adapter

![Control side](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/image.png)

![Device side](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/image2.png)

## Code Structure

All example code is in the `examples` directory.

### SPILib

`examples/SPILib` is a slightly modified version of the ADCDACPi code from:

https://github.com/abelectronicsuk/ABElectronics_CPP_Libraries/tree/master/ADCDACPi

The project uses two ADCDAC Pi Zero boards.

One board is mounted normally on the Raspberry Pi pins and uses SPI 0, plus 3.3 V and GND. The second board is connected by wires to SPI 1 on the Raspberry Pi, but on the ADCDAC board side the wires go into the board's SPI 0 pins. This is because ADCDAC works with SPI 0 at the hardware level.

![Raspberry Pi pinout](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/pinouts.png)

![ADCDAC Pi Zero](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/DACADC.png)

The ADC and DAC on this board are 12-bit. That is enough here because the joystick data arrives with the same useful resolution.

If you use hardware other than two ADCDAC Pi Zero boards, the code must be changed. This applies both to `SPILib` and to the code that uses it. If your DAC expects 16-bit values instead of 12-bit values, remove the bit shift, because values from the PC are sent as `0` to `2^16 - 1`, corresponding to `0` to `3.3 V`.

SPI must be enabled on the Raspberry Pi. In this setup both SPI 0 and SPI 1 are used.

The library was modified mainly so Linux SPI interface names can be passed as parameters.

### JoystickReader

`JoystickReader` reads joystick data on Windows. By default it uses the first connected joystick. If no joystick is connected, `udp_server.cpp` exits from `main`.

Axis mapping:

- `V` - left stick X axis
- `Z` - left stick Y axis
- `X` - right stick X axis
- `Y` - right stick Y axis

### UDP protocol

`udp_protocol.hpp` packs and unpacks joystick values into a byte buffer, because the data is sent as raw bytes.

All network exchange in this project uses Boost.Asio UDP sockets.

Each data packet contains:

- four 16-bit unsigned joystick values
- one 32-bit millisecond timestamp from the moment the client connected

The timestamp can be used to keep packet order, but packet queueing is not implemented yet.

### UDP server

`udp_server.cpp` runs on the control side, on the Windows PC.

Run example from the `build` directory:

```powershell
.\udp_server.exe password port interval_ms
```

`UdpPasswordServer` receives a Boost.Asio context, port, and password. `WaitForAuthorizedClient()` waits until a client sends the correct password. The first client with the correct password becomes the active session, and the server stops accepting new clients.

`SendValues()` sends four 16-bit values and the 32-bit elapsed time value to the connected client.

The `main` flow is:

1. Validate command-line arguments.
2. Wait for a client with the correct password.
3. Connect to the joystick.
4. Read joystick values and send them in an infinite loop.

### UDP client

`udp_client.cpp` runs on the Raspberry Pi side. It receives data from the remote PC and reproduces joystick voltages on the DAC outputs.

At startup it creates two objects for the DAC boards:

```cpp
ADCDACPi adc_dac;                       // regular SPI 0
ADCDACPi adc_dac2("", "/dev/spidev1.0"); // second SPI
```

`UdpPasswordClient` receives a Boost.Asio context, IP address, and port.

`Authorize()` tries to connect with the password.

`ReceiveLoop()` receives UDP packets, parses them, converts 16-bit joystick values to 12-bit DAC values, and writes them to the DAC outputs.

The client flow is similar to `udp_server.cpp`.

Run example:

```bash
./udp_client password server_ip port
```

## Build

### Raspberry Pi

Install Raspberry Pi OS, enable SPI, clone this repository, and run:

```bash
./build.sh
```

The script installs the required packages, downloads and bootstraps `vcpkg`, installs the dependencies from `vcpkg.json`, configures CMake with Ninja, and builds the project.

### Windows

On Windows, run:

```powershell
.\build.ps1
```

You need to install CMake and Ninja yourself. You also need a supported C++ compiler, for example Visual Studio Build Tools or MSYS2 MinGW-w64.

## Video

Video uses GStreamer. The Raspberry Pi receives video as an embedded camera through the MIPI CSI channel.

On Windows, install the full GStreamer package from the installer.

On Linux/Raspberry Pi, install GStreamer with:

```bash
sudo apt update
sudo apt install -y \
  gstreamer1.0-tools \
  gstreamer1.0-libcamera \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  libcamera-tools
```

On the client/Raspberry Pi side, start video streaming:

```bash
gst-launch-1.0 -v libcamerasrc ! video/x-raw,format=NV12,width=640,height=360,framerate=30/1 ! queue max-size-buffers=3 max-size-time=0 max-size-bytes=0 ! x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 key-int-max=30 bframes=0 byte-stream=true ! h264parse ! rtph264pay pt=96 config-interval=1 ! udpsink host=192.168.137.1 port=5600 sync=false async=false
```

Use your own host and port. The port can stay `5600`.

On Windows PowerShell, receive video with:

```powershell
gst-launch-1.0 -v `
udpsrc port=5600 caps="application/x-rtp,media=video,encoding-name=H264,payload=96,clock-rate=90000" ! `
rtpjitterbuffer latency=10 ! `
queue max-size-buffers=3 max-size-bytes=0 max-size-time=0 ! `
rtph264depay ! `
h264parse ! `
d3d11h264dec ! `
d3d11videosink sync=false
```

Choose the port that matches the sender.

For internet use, forward the required UDP ports on the router.

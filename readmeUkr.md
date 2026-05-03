# Керування джойстиком через UDP

Цей проект створений, щоб перетворити пристрій, який керується джойстиком, на пристрій із керуванням через інтернет.

Ідея така: на Windows PC читаються положення стіків джойстика, ці значення передаються через UDP, а на віддаленій стороні DAC-виходи відтворюють такі самі аналогові напруги. Ці напруги замінюють напруги, які зазвичай приходять із потенціометрів джойстика.

Сторона керування складається з:

- Windows PC
- підключеного джойстика, включно з ігровим контролером
- інтернету з доступною публічною/статичною адресою або іншого маршруту, наприклад VPN
- пробросу портів на роутері, якщо до пристрою треба підключатися ззовні локальної мережі

Сторона дрона/пристрою складається з:

- оригінального джойстика/контролера, яким раніше керувався пристрій
- двох DAC плат/мікросхем
- Raspberry Pi
- RC-HD відеовходу через HDMI-to-MIPI CSI адаптер

![Сторона керування](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/image.png)

![Сторона пристрою](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/image2.png)

## Структура коду

Весь прикладний код знаходиться в папці `examples`.

### SPILib

`examples/SPILib` - це трохи змінений код ADCDACPi з:

https://github.com/abelectronicsuk/ABElectronics_CPP_Libraries/tree/master/ADCDACPi

У проекті використовується дві плати ADCDAC Pi Zero.

Одна плата встановлена стандартно поверх пінів Raspberry Pi і використовує SPI 0, а також 3.3 V і GND. Друга плата підключена проводами до SPI 1 на Raspberry Pi, але зі сторони самої ADCDAC плати проводи підключені до її SPI 0 пінів. Це потрібно тому, що на hardware-рівні ADCDAC працює зі SPI 0.

![Pinout Raspberry Pi](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/pinouts.png)

![ADCDAC Pi Zero](https://raw.githubusercontent.com/chillchirill/radio/main/docs_images/DACADC.png)

ADC і DAC на цій платі 12-бітні. Цього тут достатньо, бо дані з джойстика приходять із такою ж корисною роздільною здатністю.

Якщо ви використовуєте не дві ADCDAC Pi Zero плати, код треба буде змінити. Це стосується і `SPILib`, і коду, який її використовує. Якщо ваш DAC очікує 16-бітні значення замість 12-бітних, треба прибрати побітовий зсув, бо з PC значення приходять як `0` до `2^16 - 1`, що відповідає `0` до `3.3 V`.

На Raspberry Pi обов'язково потрібно увімкнути SPI. У цій конфігурації використовуються SPI 0 і SPI 1.

Бібліотеку було змінено в основному для того, щоб можна було передавати назви Linux SPI-інтерфейсів у параметрах.

### JoystickReader

`JoystickReader` читає дані з джойстика на Windows. За замовчуванням використовується перший підключений джойстик. Якщо джойстик не підключений, `udp_server.cpp` завершує роботу в `main`.

Відповідність осей:

- `V` - вісь X лівого стіка
- `Z` - вісь Y лівого стіка
- `X` - вісь X правого стіка
- `Y` - вісь Y правого стіка

### UDP protocol

`udp_protocol.hpp` пакує і розпаковує значення джойстика в byte buffer, бо дані передаються як сирий потік байтів.

Весь мережевий обмін у проекті відбувається через Boost.Asio UDP sockets.

Кожен пакет даних містить:

- чотири 16-бітні unsigned значення джойстика
- одне 32-бітне значення часу в мілісекундах від моменту підключення клієнта

Timestamp можна використати для збереження порядку пакетів, але черга пакетів тут поки не реалізована.

### UDP server

`udp_server.cpp` працює на стороні керування, тобто на Windows PC.

Приклад запуску з папки `build`:

```powershell
.\udp_server.exe password port interval_ms
```

`UdpPasswordServer` отримує Boost.Asio context, порт і пароль. `WaitForAuthorizedClient()` чекає, поки клієнт надішле правильний пароль. Перший клієнт із правильним паролем стає активною сесією, і після цього сервер більше не приймає нові підключення.

`SendValues()` надсилає чотири 16-бітні значення і 32-бітне значення elapsed time підключеному клієнту.

Логіка `main`:

1. Валідація command-line аргументів.
2. Очікування клієнта з правильним паролем.
3. Підключення до джойстика.
4. Читання значень джойстика і відправка їх у нескінченному циклі.

### UDP client

`udp_client.cpp` працює на стороні Raspberry Pi. Він приймає дані з віддаленого PC і відтворює напруги джойстика на DAC-виходах.

На старті створюються два об'єкти для DAC плат:

```cpp
ADCDACPi adc_dac;                       // звичайний SPI 0
ADCDACPi adc_dac2("", "/dev/spidev1.0"); // другий SPI
```

`UdpPasswordClient` отримує Boost.Asio context, IP-адресу і порт.

`Authorize()` пробує підключитися з паролем.

`ReceiveLoop()` приймає UDP-пакети, парсить їх, перетворює 16-бітні значення джойстика в 12-бітні значення DAC і записує їх на DAC-виходи.

Логіка клієнта аналогічна до `udp_server.cpp`.

Приклад запуску:

```bash
./udp_client password server_ip port
```

## Build

### Raspberry Pi

Встановіть Raspberry Pi OS, увімкніть SPI, склонуйте цей репозиторій і запустіть:

```bash
./build.sh
```

Скрипт встановить потрібні пакети, завантажить і збере `vcpkg`, встановить залежності з `vcpkg.json`, налаштує CMake з Ninja і збере проект.

### Windows

На Windows запустіть:

```powershell
.\build.ps1
```

CMake і Ninja треба встановити самостійно. Також потрібен підтримуваний C++ compiler, наприклад Visual Studio Build Tools або MSYS2 MinGW-w64.

## Відео

Для відео використовується GStreamer. Raspberry Pi отримує відео як вбудовану камеру через MIPI CSI канал.

На Windows потрібно встановити повну версію GStreamer з інсталятора.

На Linux/Raspberry Pi GStreamer встановлюється так:

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

На стороні клієнта/Raspberry Pi запустіть трансляцію відео:

```bash
gst-launch-1.0 -v libcamerasrc ! video/x-raw,format=NV12,width=640,height=360,framerate=30/1 ! queue max-size-buffers=3 max-size-time=0 max-size-bytes=0 ! x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 key-int-max=30 bframes=0 byte-stream=true ! h264parse ! rtph264pay pt=96 config-interval=1 ! udpsink host=192.168.137.1 port=5600 sync=false async=false
```

Вкажіть свій host і port. Порт можна залишити `5600`.

На Windows PowerShell відео приймається так:

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

Порт має збігатися з тим, який використовується на відправнику.

Для роботи через інтернет потрібно зробити проброс потрібних UDP-портів на домашньому роутері.

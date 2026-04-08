cmake_minimum_required(VERSION 3.21)

project(radiomaster_reader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost CONFIG REQUIRED COMPONENTS asio system)

function(configure_target_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(add_udp_example target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    target_link_libraries(${target_name} PRIVATE Boost::asio Boost::system)

    if(WIN32)
        target_link_libraries(${target_name} PRIVATE ws2_32)
        target_compile_definitions(${target_name} PRIVATE
            WIN32_LEAN_AND_MEAN
            NOMINMAX
            _WIN32_WINNT=0x0601
        )
    endif()

    configure_target_warnings(${target_name})
endfunction()

add_library(joystick_reader STATIC
    examples/joystick_reader.cpp
)

target_include_directories(joystick_reader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/examples)
target_link_libraries(joystick_reader PUBLIC winmm)
configure_target_warnings(joystick_reader)

add_executable(radiomaster_reader
    main.cpp
)

target_link_libraries(radiomaster_reader PRIVATE joystick_reader)
configure_target_warnings(radiomaster_reader)

add_udp_example(udp_server examples/udp_server.cpp)
target_link_libraries(udp_server PRIVATE joystick_reader)

if(UNIX AND NOT APPLE)
    add_library(spilib STATIC
        examples/SPILib/ABE_ADCDACPi.cpp
    )

    target_include_directories(spilib PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/SPILib
    )

    configure_target_warnings(spilib)

    add_udp_example(udp_client examples/udp_client.cpp)
    target_link_libraries(udp_client PRIVATE spilib)
else()
    message(STATUS "Skipping udp_client and SPILib: Linux SPI headers are required.")
endif()

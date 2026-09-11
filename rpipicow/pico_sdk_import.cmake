# pico_sdk_import.cmake
#
# Points the build at the Raspberry Pi Pico SDK.
#
# Before configuring, either:
#   - set the PICO_SDK_PATH environment variable, or
#   - pass it to CMake: cmake -DPICO_SDK_PATH="C:/pico/pico-sdk" -B build -S . -G Ninja

if (NOT DEFINED PICO_SDK_PATH)
    if (DEFINED ENV{PICO_SDK_PATH})
        set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
    endif()
endif()

if (NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR
        "PICO_SDK_PATH is not set. Set the environment variable or pass -DPICO_SDK_PATH=... to CMake.")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

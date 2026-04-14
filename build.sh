#!/bin/bash
# FestStefan build helper — sources the correct ESP-IDF v6.0 environment
# Usage: ./build.sh [build|flash|monitor|flash monitor]

export IDF_PATH=$HOME/.espressif/v6.0/esp-idf
export IDF_PYTHON_ENV_PATH=$HOME/.espressif/python_env/idf6.0_py3.14_env
export ESP_IDF_VERSION=6.0.0
export PATH="$IDF_PYTHON_ENV_PATH/bin:$HOME/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin:$HOME/.espressif/tools/cmake/3.31.5/CMake.app/Contents/bin:$HOME/.espressif/tools/ninja/1.12.1:$PATH"

SERIAL_PORT="/dev/cu.usbserial-0001"

if [ $# -eq 0 ]; then
    echo "Usage: ./build.sh [build|flash|monitor|all]"
    echo "  build   — compile only"
    echo "  flash   — build + flash to board"
    echo "  monitor — serial monitor (Ctrl+] to exit)"
    echo "  all     — build + flash + monitor"
    exit 0
fi

case "$1" in
    build)
        python3 $IDF_PATH/tools/idf.py build
        ;;
    flash)
        python3 $IDF_PATH/tools/idf.py build && \
        python3 $IDF_PATH/tools/idf.py -p $SERIAL_PORT flash
        ;;
    monitor)
        python3 $IDF_PATH/tools/idf.py -p $SERIAL_PORT monitor
        ;;
    all)
        python3 $IDF_PATH/tools/idf.py build && \
        python3 $IDF_PATH/tools/idf.py -p $SERIAL_PORT flash && \
        python3 $IDF_PATH/tools/idf.py -p $SERIAL_PORT monitor
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use: build, flash, monitor, or all"
        exit 1
        ;;
esac

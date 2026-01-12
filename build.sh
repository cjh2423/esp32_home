#!/bin/bash
# ESP32-S3 一键编译烧录脚本

set -e

cd "$(dirname "${BASH_SOURCE[0]}")"

PORT="${PORT:-/dev/ttyUSB0}"

if [ -z "$IDF_PATH" ]; then
    echo "错误: 请先运行 '. \$IDF_PATH/export.sh'"
    exit 1
fi

echo "编译中..."
idf.py build
echo "编译完成! 烧录到 $PORT ..."
idf.py -p "$PORT" -b 921600 flash monitor

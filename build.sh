#!/bin/bash
# ESP32-S3 编译烧录脚本

set -e

cd "$(dirname "${BASH_SOURCE[0]}")"

PORT="${PORT:-/dev/ttyUSB0}"

if [ -z "$IDF_PATH" ]; then
    echo "错误: 请先运行 '. \$IDF_PATH/export.sh'"
    exit 1
fi

case "${1:-build}" in
    build)
        echo "编译中..."
        idf.py build
        echo "编译完成!"
        ;;
    flash)
        echo "烧录到 $PORT ..."
        idf.py -p "$PORT" flash monitor
        ;;
    *)
        echo "用法: ./build.sh [build|flash]"
        echo "  build  - 编译项目"
        echo "  flash  - 烧录并监视 (可用 PORT=/dev/ttyXXX 指定串口)"
        ;;
esac

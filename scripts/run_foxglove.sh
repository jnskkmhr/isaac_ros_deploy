#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Serve ROS telemetry to Foxglove on port 8765 by default.
set -euo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$workspace"
if [[ ${1:-} == --jetson ]]; then
    shift
fi
export RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}
if [[ $# -gt 1 || ! ${1:-8765} =~ ^[0-9]+$ ]]; then
    echo "Usage: $0 [--jetson] [PORT]" >&2
    exit 1
fi
port=${1:-8765}
if (( port < 1 || port > 65535 )); then
    echo 'Port must be between 1 and 65535.' >&2
    exit 1
fi
exec ./scripts/ros.sh run foxglove_bridge foxglove_bridge --ros-args \
        -p address:=0.0.0.0 -p port:="$port"

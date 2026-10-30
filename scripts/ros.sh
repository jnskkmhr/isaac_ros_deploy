#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -eo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$workspace/scripts/build_paths.sh"
source /opt/ros/jazzy/setup.bash
if [[ ! -f "$workspace/ros_ws/$DEPLOY_INSTALL_DIR/setup.bash" ]]; then
    echo 'Run ./scripts/build.sh inside the deployment container first.' >&2
    exit 1
fi
source "$workspace/ros_ws/$DEPLOY_INSTALL_DIR/setup.bash"
export RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}
exec ros2 "$@"

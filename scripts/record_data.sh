#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Record joint effort, controller targets and motor temperatures; Ctrl-C closes the bag.
set -euo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$workspace"
if [[ ${1:-} == --jetson ]]; then
    shift
fi
export RMW_IMPLEMENTATION=${RMW_IMPLEMENTATION:-rmw_fastrtps_cpp}
if [[ $# != 1 || $1 == --help ]]; then
    echo "Usage: $0 [--jetson] OUTPUT_DIRECTORY" >&2
    exit 1
fi
output=$(realpath -m -- "$1")
if [[ -e $output ]]; then
    echo "Output already exists: $output" >&2
    exit 1
fi
exec ./scripts/ros.sh bag record -s mcap -o "$output" --topics \
        /joint_states /safety_controller/blended_command /diagnostics \
        /imu_sensor_broadcaster/imu /cmd_vel /policy_cmd_vel /policy_heading \
        /clock /simulator/floating_base_state /tf /tf_static \
        /robot_description /parameter_events /rosout

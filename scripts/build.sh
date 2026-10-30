#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -eo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$workspace/scripts/build_paths.sh"
source /opt/ros/jazzy/setup.bash
export PATH=/opt/deploy-python/bin:$PATH
cd "$workspace/ros_ws"
# Limit memory use on Jetson; override BUILD_JOBS if desired.
export CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS:-2}
colcon --log-base "$DEPLOY_LOG_DIR" build --build-base "$DEPLOY_BUILD_DIR" --install-base "$DEPLOY_INSTALL_DIR" --executor sequential --packages-up-to g1_bringup --symlink-install \
    --cmake-args --no-warn-unused-cli -DCMAKE_BUILD_TYPE=Release -DUSE_NVTX=OFF \
    -DISAAC_DEPLOY_BACKEND="${ISAAC_DEPLOY_BACKEND:-onnxruntime}" \
    -DMUJOCO_DEP_VERSION_lodepng=17d08dd26cac4d63f43af217ebd70318bfb8189c \
    -DBUILD_TESTING=OFF "$@"

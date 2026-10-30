#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -eo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
source "$workspace/scripts/build_paths.sh"
source /opt/ros/jazzy/setup.bash
source "$workspace/ros_ws/$DEPLOY_INSTALL_DIR/setup.bash"
export PATH=/opt/deploy-python/bin:$PATH
export CMAKE_BUILD_PARALLEL_LEVEL=${BUILD_JOBS:-2}
cd "$workspace/ros_ws"
colcon --log-base "$DEPLOY_LOG_DIR" build --build-base "$DEPLOY_BUILD_DIR" --install-base "$DEPLOY_INSTALL_DIR" --executor sequential --packages-select isaac_deploy_core g1_bringup \
    --cmake-args -DBUILD_TESTING=ON -DUSE_NVTX=OFF -DISAAC_DEPLOY_BACKEND="${ISAAC_DEPLOY_BACKEND:-onnxruntime}"
colcon --log-base "$DEPLOY_LOG_DIR" test --build-base "$DEPLOY_BUILD_DIR" --install-base "$DEPLOY_INSTALL_DIR" --packages-select isaac_deploy_core g1_bringup --return-code-on-test-failure
colcon test-result --test-result-base "$DEPLOY_BUILD_DIR" --verbose
cmake -S src/isaac_ros_deploy/isaac_deploy/isaac_ros_deploy_ros2_control \
    -B "$DEPLOY_BUILD_DIR/isaac_ros_deploy_ros2_control" -DBUILD_TESTING=ON
cmake --build "$DEPLOY_BUILD_DIR/isaac_ros_deploy_ros2_control" --target test_leapp_backend_mapping
"$DEPLOY_BUILD_DIR/isaac_ros_deploy_ros2_control/test_leapp_backend_mapping"
fixture_dir=$(mktemp -d)
trap 'rm -rf "$fixture_dir"' EXIT
for bundle in src/isaac_ros_robots/unitree_g1/g1_bringup/policies/*/*.yaml; do
    echo "Checking $bundle"
    model=$(python3 "$workspace/scripts/make_parity_fixture.py" "$bundle" "$fixture_dir")
    "$DEPLOY_BUILD_DIR/isaac_deploy_core/onnx_runtime_parity_test" "$model" \
        "$fixture_dir/inputs.safetensors" "$fixture_dir/outputs.safetensors" "${ISAAC_DEPLOY_BACKEND:-onnxruntime}"
done

#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
workspace=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
profile=cpu
build=false
while [[ $# -gt 0 ]]; do
    case "$1" in
        --profile) profile=${2:?Provide a profile}; shift 2 ;;
        --build) build=true; shift ;;
        *) echo "Usage: $0 [--build] [--profile cpu|x86-gpu|jp72-gpu|jp71-gpu|jp6-gpu|jp5-gpu]" >&2; exit 1 ;;
    esac
done
case "$profile:$(uname -m)" in
    cpu:*) ;;
    x86-gpu:x86_64|jp72-gpu:aarch64) ;;
    jp71-gpu:aarch64|jp6-gpu:aarch64|jp5-gpu:aarch64) echo "$profile is experimental: native build and device verification are required." >&2 ;;
    *) echo "Unsupported profile/host combination: $profile / $(uname -m)." >&2; exit 1 ;;
esac
image=${DEPLOY_IMAGE:-isaac-ros-deploy:$profile}
if $build; then
    if [[ $profile == cpu ]]; then
        docker build -t "$image" -f "$workspace/docker/Dockerfile" "$workspace"
    else
        docker build -t isaac-ros-deploy:cpu -f "$workspace/docker/Dockerfile" "$workspace"
        gpu_dockerfile="$workspace/docker/Dockerfile.gpu"
        gpu_build_args=(--build-arg "DEPLOY_PROFILE=$profile")
        if [[ $profile == jp6-gpu || $profile == jp5-gpu ]]; then
            gpu_dockerfile="$workspace/docker/Dockerfile.jetpack-legacy"
            gpu_build_args+=(--build-arg "BUILD_JOBS=${BUILD_JOBS:-2}")
        fi
        docker build "${gpu_build_args[@]}" \
            -t "$image" -f "$gpu_dockerfile" "$workspace"
    fi
fi
if ! docker image inspect "$image" >/dev/null 2>&1; then
    echo "Image $image is missing. Run $0 --profile $profile --build first." >&2
    exit 1
fi
# Host networking is required for the Unitree SDK and keeps terminal discovery shared.
args=(run --rm -it --network host --ipc host --cap-add SYS_NICE --ulimit rtprio=99 \
      -e "DEPLOY_PROFILE=$profile" -e "DEPLOY_UID=$(id -u)" -e "DEPLOY_GID=$(id -g)" \
      -e "TERM=${TERM:-xterm-256color}" \
      -e "ROS_DOMAIN_ID=${ROS_DOMAIN_ID:-0}" \
      -v "$workspace:/workspaces/isaac_ros_deploy")
# Mount the host NVIDIA graphics driver when its container runtime is available.
# /dev/dri alone leaves NVIDIA systems on Mesa's CPU renderer (llvmpipe).
# Runtime selection also works on Jetson versions that do not support --gpus.
if [[ $(docker info --format '{{if index .Runtimes "nvidia"}}yes{{end}}') == yes ]]; then
    args+=(--runtime=nvidia -e NVIDIA_VISIBLE_DEVICES=all \
           -e NVIDIA_DRIVER_CAPABILITIES=compute,graphics,utility,display)
elif [[ $profile != cpu ]]; then
    echo "GPU profiles require the NVIDIA Container Toolkit runtime." >&2
    exit 1
fi
if [[ -n ${DISPLAY:-} ]]; then
    args+=(-e DISPLAY -v /tmp/.X11-unix:/tmp/.X11-unix:rw)
    if [[ -f ${XAUTHORITY:-} ]]; then
        args+=(-e XAUTHORITY=/tmp/.deploy-xauth -v "$XAUTHORITY:/tmp/.deploy-xauth:ro")
    fi
fi
if [[ -d /dev/dri ]]; then args+=(--device /dev/dri); fi
exec docker "${args[@]}" "$image"

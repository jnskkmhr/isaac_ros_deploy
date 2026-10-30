#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -e

# Match Isaac ROS's admin account to the host file owner, then drop privileges.
# A numeric docker --user alone leaves no passwd/group entry for the shell.
if [[ $(id -u) == 0 && -n ${DEPLOY_UID:-} && $DEPLOY_UID != 0 ]]; then
    if [[ ! $DEPLOY_UID =~ ^[0-9]+$ || ! ${DEPLOY_GID:-} =~ ^[0-9]+$ ]]; then
        echo 'DEPLOY_UID and DEPLOY_GID must be numeric host IDs.' >&2
        exit 1
    fi
    if ! getent group "$DEPLOY_GID" >/dev/null; then
        groupadd --gid "$DEPLOY_GID" admin
    fi
    existing_user=$(getent passwd "$DEPLOY_UID" | cut -d: -f1)
    if [[ -n $existing_user && $existing_user != admin ]]; then
        # Ubuntu's base image already has UID 1000; reuse that account.
        usermod --login admin --home /home/admin --move-home "$existing_user"
    elif ! getent passwd admin >/dev/null; then
        useradd --no-log-init --create-home --shell /bin/bash \
            --uid "$DEPLOY_UID" --gid "$DEPLOY_GID" -K UID_MAX=2147483647 admin
    fi
    if [[ $(id -u admin) != "$DEPLOY_UID" ]]; then
        echo 'The image already has an admin account with a different UID.' >&2
        exit 1
    fi
    if [[ $(id -g admin) != "$DEPLOY_GID" || $(getent passwd admin | cut -d: -f7) != /bin/bash ]]; then
        usermod --gid "$DEPLOY_GID" --shell /bin/bash admin
    fi
    sed -i 's/^#force_color_prompt=yes$/force_color_prompt=yes/' /home/admin/.bashrc
    exec setpriv --reuid="$DEPLOY_UID" --regid="$DEPLOY_GID" --init-groups \
        env HOME=/home/admin USER=admin LOGNAME=admin /deploy-entrypoint.sh "$@"
fi

mkdir -p "$HOME"
# Fail before GPU model loading if the NVIDIA runtime did not expose a GPU.
if [[ ${ISAAC_DEPLOY_BACKEND:-onnxruntime} == triton || ${ISAAC_DEPLOY_ONNX_DEVICE:-cpu} == cuda ]]; then
    python3 - <<'GPU_CHECK'
import ctypes
try:
    cuda = ctypes.CDLL('libcuda.so.1')
    status = cuda.cuInit(0)
    count = ctypes.c_int()
    if status != 0 or cuda.cuDeviceGetCount(ctypes.byref(count)) != 0 or count.value < 1:
        raise RuntimeError(f'CUDA initialization status={status}, visible devices={count.value}')
except (OSError, RuntimeError) as error:
    raise SystemExit(f'GPU profile requires a visible NVIDIA GPU: {error}. Use scripts/activate.sh with NVIDIA Container Toolkit.')
GPU_CHECK
fi
source /opt/ros/jazzy/setup.bash
source /workspaces/isaac_ros_deploy/scripts/build_paths.sh
if [[ -f /workspaces/isaac_ros_deploy/ros_ws/${DEPLOY_INSTALL_DIR}/setup.bash ]]; then
    source /workspaces/isaac_ros_deploy/ros_ws/${DEPLOY_INSTALL_DIR}/setup.bash
fi
exec "$@"

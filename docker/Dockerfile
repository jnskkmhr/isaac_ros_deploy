# SPDX-License-Identifier: Apache-2.0
ARG BASE_IMAGE=ros:jazzy-ros-base
FROM ${BASE_IMAGE} AS development
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git curl ca-certificates patch pkg-config \
    python3-colcon-common-extensions python3-rosdep python3-pip python3-venv \
    python3-pytest python3-numpy python3-yaml \
    libyaml-cpp-dev nlohmann-json3-dev libeigen3-dev libfmt-dev \
    libglfw3-dev libegl1-mesa-dev libtinyxml2-dev libssl-dev \
    ros-jazzy-ros2-control ros-jazzy-ros2-controllers ros-jazzy-control-toolbox \
    ros-jazzy-tl-expected ros-jazzy-pinocchio ros-jazzy-backward-ros \
    ros-jazzy-tf2-ros ros-jazzy-urdf ros-jazzy-xacro ros-jazzy-robot-state-publisher \
    ros-jazzy-foxglove-bridge ros-jazzy-rosbag2-storage-mcap ros-jazzy-teleop-twist-keyboard \
    ros-jazzy-ament-cmake-gtest ros-jazzy-ament-cmake-pytest ros-jazzy-ament-lint-auto \
    ros-jazzy-ament-lint-common tmux iputils-ping iproute2 \
    && rm -rf /var/lib/apt/lists/*
# CPU-only Torch retains the original controller's tensor API without CUDA.
RUN python3 -m venv --system-site-packages /opt/deploy-python
ENV PATH=/opt/deploy-python/bin:$PATH
RUN pip install --no-cache-dir torch==2.9.0 --index-url https://download.pytorch.org/whl/cpu \
    && pip install --no-cache-dir onnx==1.19.1 onnxruntime==1.23.2
ARG TARGETARCH
RUN case "${TARGETARCH:-$(dpkg --print-architecture)}" in \
      amd64) ort_arch=x64 ;; arm64) ort_arch=aarch64 ;; *) exit 1 ;; esac \
    && mkdir -p /opt/onnxruntime \
    && curl -fLsS "https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-${ort_arch}-1.23.2.tgz" \
       | tar -xz --strip-components=1 -C /opt/onnxruntime
RUN apt-get update && apt-get install -y --no-install-recommends ros-jazzy-ros2-control-cmake patchelf \
    && rm -rf /var/lib/apt/lists/*
ENV RMW_IMPLEMENTATION=rmw_fastrtps_cpp
ENV LD_LIBRARY_PATH=/opt/onnxruntime/lib
WORKDIR /workspaces/isaac_ros_deploy
COPY docker/entrypoint.sh /deploy-entrypoint.sh
ENTRYPOINT ["/deploy-entrypoint.sh"]
CMD ["bash"]

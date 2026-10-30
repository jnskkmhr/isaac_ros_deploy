#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# Native ARM64 build; these legacy JetPack configurations require device verification.
set -euo pipefail
profile=${1:?Provide jp6-gpu or jp5-gpu}
case "$profile" in jp6-gpu|jp5-gpu) ;; *) exit 2 ;; esac
readarray -t settings < <(python3 - "$profile" <<'CONFIG'
import json, sys
p = json.load(open('/opt/gpu_dependencies.json'))[sys.argv[1]]
print(p['onnxruntime_commit'])
print(p['cuda_architectures'])
CONFIG
)
git init /tmp/onnxruntime-src
git -C /tmp/onnxruntime-src remote add origin https://github.com/microsoft/onnxruntime.git
git -C /tmp/onnxruntime-src fetch --depth 1 origin "${settings[0]}"
git -C /tmp/onnxruntime-src checkout --detach FETCH_HEAD
git -C /tmp/onnxruntime-src submodule update --init --recursive --depth 1
export CC=/usr/bin/gcc-11 CXX=/usr/bin/g++-11
export PATH=/opt/cuda/bin:$PATH
# Flat NVIDIA redistribution layout; CMake also looks for lib64 under CUDA_HOME.
ln -s lib /opt/cuda/lib64
cd /tmp/onnxruntime-src
./build.sh --build_dir /tmp/onnxruntime-build --config Release \
    --update --build --build_shared_lib --skip_tests --allow_running_as_root \
    --parallel "${BUILD_JOBS:-2}" --compile_no_warning_as_error \
    --use_cuda --cuda_home /opt/cuda --cudnn_home /opt/cuda \
    --cmake_extra_defines \
    CMAKE_CUDA_HOST_COMPILER=/usr/bin/g++-11 \
    "CMAKE_CUDA_ARCHITECTURES=${settings[1]}" \
    onnxruntime_BUILD_UNIT_TESTS=OFF \
    onnxruntime_USE_FLASH_ATTENTION=OFF \
    onnxruntime_USE_LEAN_ATTENTION=OFF \
    onnxruntime_USE_MEMORY_EFFICIENT_ATTENTION=OFF \
    onnxruntime_USE_FPA_INTB_GEMM=OFF
mkdir -p /opt/onnxruntime-cuda/include /opt/onnxruntime-cuda/lib /opt/cuda-compat
cp include/onnxruntime/core/session/*.h /opt/onnxruntime-cuda/include/
cp -a /tmp/onnxruntime-build/Release/libonnxruntime*.so* /opt/onnxruntime-cuda/lib/
cp LICENSE ThirdPartyNotices.txt /opt/onnxruntime-cuda/
if [[ $profile == jp5-gpu ]]; then
    cp -a /usr/local/cuda-11.8/compat/. /opt/cuda-compat/
fi
# Copy runtime libraries and their licenses, not nvcc or static compiler libraries.
find /opt/cuda/lib -name '*.a' -delete
rm -rf /opt/cuda/lib/stubs
mkdir -p /opt/cuda-licenses
find /opt/cuda -maxdepth 1 -type f -iname '*license*' -exec cp {} /opt/cuda-licenses/ \;

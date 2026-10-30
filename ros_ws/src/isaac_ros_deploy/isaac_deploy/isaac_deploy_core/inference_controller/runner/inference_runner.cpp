// SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
// Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "isaac_deploy_core/inference_controller/runner/inference_runner.hpp"

#include "isaac_deploy_core/inference_controller/runner/mock_runner.hpp"
#ifdef ISAAC_DEPLOY_ONNX_RUNTIME
#include "isaac_deploy_core/inference_controller/runner/onnx_runtime_runner.hpp"
#else
#include "isaac_deploy_core/inference_controller/runner/triton_runner.hpp"
#endif

namespace isaac_deploy_core
{

expected<std::unique_ptr<InferenceRunner>> InferenceRunner::create(const Config & config)
{
#ifdef ISAAC_DEPLOY_ONNX_RUNTIME
  if (config.runner_type == "onnxruntime") {
    return OnnxRuntimeRunner::create(config.model_path);
  }
#else
  if (config.runner_type == "triton") {
    TritonRunnerConfig triton_config{.model_path = config.model_path};
    return TritonRunner::create(triton_config);
  }

#endif

  if (config.runner_type == "mock") {
    return MockRunner::create();
  }

  return tl::unexpected(
      make_error(
        Error::Code::kInvalidArgument,
        "Unknown runner type: " + config.runner_type));
}

}  // namespace isaac_deploy_core

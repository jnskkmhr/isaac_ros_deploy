// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <onnxruntime_cxx_api.h>

#include "isaac_deploy_core/inference_controller/runner/inference_runner.hpp"

namespace isaac_deploy_core
{
/// CPU inference for portable G1 deployment; controllers retain ownership of feedback state.
class OnnxRuntimeRunner : public InferenceRunner
{
public:
  static expected<std::unique_ptr<OnnxRuntimeRunner>> create(const std::filesystem::path& path);
  expected<void> run(const TensorDict& inputs, TensorDict& outputs) override;
  std::vector<std::string> get_input_names() const override { return input_names_; }
  std::vector<std::string> get_output_names() const override { return output_names_; }
  void reset() override {}
  expected<void> warmup() override;

private:
  explicit OnnxRuntimeRunner(const std::filesystem::path& path);
  Ort::Env env_{ORT_LOGGING_LEVEL_WARNING, "isaac_deploy"};
  Ort::Session session_{nullptr};
  std::vector<std::string> input_names_, output_names_;
  std::vector<const char*> input_name_ptrs_, output_name_ptrs_;
  std::vector<torch::Tensor> input_buffers_, output_buffers_;
  std::vector<Ort::Value> input_values_, output_values_;
};
}  // namespace isaac_deploy_core

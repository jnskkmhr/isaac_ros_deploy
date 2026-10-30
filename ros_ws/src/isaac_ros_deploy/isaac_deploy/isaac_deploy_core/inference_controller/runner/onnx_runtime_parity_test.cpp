// SPDX-License-Identifier: Apache-2.0
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <string>
#include <iostream>

#include "isaac_deploy_core/inference_controller/runner/inference_runner.hpp"
#include "isaac_deploy_core/inference_controller/safetensors_loader.hpp"

int main(int argc, char** argv)
{
  using namespace isaac_deploy_core;
  if (argc != 4 && argc != 5)
  {
    return 2;
  }
  const std::string backend = argc == 5 ? argv[4] : "onnxruntime";
  // CUDA and CPU kernels accumulate FP32 products in different orders.
  const char* device = std::getenv("ISAAC_DEPLOY_ONNX_DEVICE");
  const bool cuda = device != nullptr && std::string(device) == "cuda";
  const double absolute_tolerance = backend == "triton" || cuda ? 1e-5 : 1e-6;
  double max_absolute_error = 0.0;
  auto runner = InferenceRunner::create({argv[1], backend});
  auto inputs = load_safetensors(argv[2]);
  auto reference = load_safetensors(argv[3]);
  if (!runner || !inputs || !reference)
  {
    std::cerr << "Could not load parity fixtures or model\n";
    if (!runner) {std::cerr << runner.error().message << "\n";}
    return 1;
  }
  auto& model = *runner.value();
  auto warmup = model.warmup();
  if (!warmup)
  {
    std::cerr << warmup.error().message;
    return 1;
  }
  TensorDict outputs;
  auto start = std::chrono::steady_clock::now();
  for (int step = 0; step < 30; ++step)
  {
    TensorDict step_inputs;
    for (const auto& name : model.get_input_names())
    {
      step_inputs[name] = inputs->at(std::to_string(step) + "/" + name);
    }
    auto status = model.run(step_inputs, outputs);
    if (!status)
    {
      std::cerr << status.error().message;
      return 1;
    }
    for (const auto& name : model.get_output_names())
    {
      const auto& expected = reference->at(std::to_string(step) + "/" + name);
      max_absolute_error = std::max(max_absolute_error,
          (outputs.at(name) - expected).abs().max().item<double>());
      if (!torch::allclose(outputs.at(name), expected, 1e-5, absolute_tolerance))
      {
        std::cerr << "Output differs: " << step << "/" << name
                  << "; max absolute error=" << (outputs.at(name) - expected).abs().max().item<double>() << "\n";
        return 1;
      }
    }
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
  TensorDict invalid;
  auto invalid_status = model.run(invalid, outputs);
  if (invalid_status)
  {
    std::cerr << "Missing input was accepted";
    return 1;
  }
  const auto shutdown_start = std::chrono::steady_clock::now();
  runner.value().reset();
  const auto shutdown_seconds =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - shutdown_start).count();
  // Leaked Triton request objects retain their model and exhaust its 30 s exit timeout.
  if (shutdown_seconds >= 5.0)
  {
    std::cerr << "Model shutdown exceeded 5 seconds: " << shutdown_seconds << "\n";
    return 1;
  }
  std::cout << "30 stateful steps match Python ORT; missing input rejected; " << elapsed / 30.0
            << " ms/step including comparison; max absolute error=" << max_absolute_error
            << "; shutdown=" << shutdown_seconds << " s\n";
  return 0;
}

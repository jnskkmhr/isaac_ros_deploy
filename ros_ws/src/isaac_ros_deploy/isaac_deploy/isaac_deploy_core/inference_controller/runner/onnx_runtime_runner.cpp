// SPDX-License-Identifier: Apache-2.0
#include "isaac_deploy_core/inference_controller/runner/onnx_runtime_runner.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace isaac_deploy_core
{
namespace
{
torch::ScalarType torch_type(ONNXTensorElementDataType type)
{
  switch (type)
  {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
      return torch::kFloat32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:
      return torch::kFloat64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:
      return torch::kInt64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
      return torch::kInt32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:
      return torch::kBool;
    default:
      throw std::invalid_argument("Unsupported ONNX tensor element type");
  }
}
}  // namespace
OnnxRuntimeRunner::OnnxRuntimeRunner(const std::filesystem::path& path)
{
  Ort::SessionOptions options;
  options.SetIntraOpNumThreads(1);
  options.SetInterOpNumThreads(1);
  options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
  const char* device_setting = std::getenv("ISAAC_DEPLOY_ONNX_DEVICE");
  const std::string device = device_setting == nullptr ? "cpu" : device_setting;
  if (device == "cuda")
  {
    const auto providers = Ort::GetAvailableProviders();
    if (std::find(providers.begin(), providers.end(), "CUDAExecutionProvider") == providers.end())
    {
      throw std::invalid_argument("CUDA inference requested but ONNX Runtime has no CUDA provider");
    }
    OrtCUDAProviderOptions cuda_options{};
    cuda_options.device_id = 0;
    options.AppendExecutionProvider_CUDA(cuda_options);
  }
  else if (device != "cpu")
  {
    throw std::invalid_argument("ISAAC_DEPLOY_ONNX_DEVICE must be cpu or cuda");
  }
  session_ = Ort::Session(env_, path.c_str(), options);
  Ort::AllocatorWithDefaultOptions allocator;
  auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
  for (bool input : {true, false})
  {
    auto& names = input ? input_names_ : output_names_;
    auto& buffers = input ? input_buffers_ : output_buffers_;
    auto& values = input ? input_values_ : output_values_;
    const auto count = input ? session_.GetInputCount() : session_.GetOutputCount();
    for (size_t i = 0; i < count; ++i)
    {
      auto name = input ? session_.GetInputNameAllocated(i, allocator)
                        : session_.GetOutputNameAllocated(i, allocator);
      names.emplace_back(name.get());
      auto info = input ? session_.GetInputTypeInfo(i) : session_.GetOutputTypeInfo(i);
      auto tensor_info = info.GetTensorTypeAndShapeInfo();
      auto shape = tensor_info.GetShape();
      for (auto dim : shape)
      {
        if (dim <= 0)
        {
          throw std::invalid_argument("LEAPP deployment requires static tensor shapes");
        }
      }
      auto type = tensor_info.GetElementType();
      buffers.push_back(
          torch::zeros(shape, torch::TensorOptions().dtype(torch_type(type)).device(torch::kCPU)));
      auto& buffer = buffers.back();
      values.push_back(Ort::Value::CreateTensor(memory, buffer.data_ptr(), buffer.nbytes(),
                                                shape.data(), shape.size(), type));
    }
  }
  for (const auto& name : input_names_)
  {
    input_name_ptrs_.push_back(name.c_str());
  }
  for (const auto& name : output_names_)
  {
    output_name_ptrs_.push_back(name.c_str());
  }
}
expected<std::unique_ptr<OnnxRuntimeRunner>> OnnxRuntimeRunner::create(
    const std::filesystem::path& path)
{
  try
  {
    return std::unique_ptr<OnnxRuntimeRunner>(new OnnxRuntimeRunner(path));
  }
  catch (const std::exception& error)
  {
    return tl::unexpected(make_error(Error::Code::kInvalidArgument, error.what()));
  }
}
expected<void> OnnxRuntimeRunner::warmup()
{
  try
  {
    session_.Run(Ort::RunOptions{nullptr}, input_name_ptrs_.data(), input_values_.data(),
                 input_values_.size(), output_name_ptrs_.data(), output_values_.data(),
                 output_values_.size());
    return {};
  }
  catch (const std::exception& error)
  {
    return tl::unexpected(make_error(Error::Code::kInternal, error.what()));
  }
}
expected<void> OnnxRuntimeRunner::run(const TensorDict& inputs, TensorDict& outputs)
{
  try
  {
    for (size_t i = 0; i < input_names_.size(); ++i)
    {
      const auto& source = inputs.at(input_names_[i]);
      const auto& buffer = input_buffers_[i];
      if (!source.device().is_cpu() || !source.is_contiguous() ||
          source.sizes() != buffer.sizes() || source.scalar_type() != buffer.scalar_type())
      {
        return tl::unexpected(make_error(Error::Code::kInvalidArgument,
                                         "Input shape/type/device mismatch: " + input_names_[i]));
      }
      std::memcpy(buffer.data_ptr(), source.data_ptr(), buffer.nbytes());
    }
    RETURN_IF_ERROR(warmup());
    for (size_t i = 0; i < output_names_.size(); ++i)
    {
      // The controller passes an empty map after activation. Retain tensor storage
      // in the runner and publish handles, as required by the runner interface.
      outputs[output_names_[i]] = output_buffers_[i];
    }
    return {};
  }
  catch (const std::exception& error)
  {
    return tl::unexpected(make_error(Error::Code::kInternal, error.what()));
  }
}
}  // namespace isaac_deploy_core

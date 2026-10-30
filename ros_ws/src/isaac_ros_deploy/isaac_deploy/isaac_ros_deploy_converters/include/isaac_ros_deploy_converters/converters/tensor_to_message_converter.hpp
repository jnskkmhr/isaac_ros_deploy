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

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/serialization.hpp"
#include "torch/torch.h"

namespace isaac_ros_deploy_converters
{

/// Abstract base class for tensor-to-message output converters.
///
/// Each converter handles a single kind (e.g., "joint_pos_targets") and writes
/// its data into a shared typed message. Multiple converters can write to the
/// same message sequentially (e.g., position + stiffness into JointCommand).
///
/// This is symmetric with the input side's MessageToTensorConverter: one
/// converter per kind, stateless, grouped by topic in the node.
class TensorToMessageConverter
{
public:
  virtual ~TensorToMessageConverter() = default;

  /// The single kind this converter handles (e.g., "joint_pos_targets").
  virtual std::string get_kind() const = 0;

  /// The ROS message type (e.g., "isaac_ros_deploy_interfaces/msg/JointCommand").
  virtual std::string get_message_type() const = 0;

  /// Create a new default message instance (type-erased).
  virtual std::shared_ptr<void> create_message() const = 0;

  /// Write tensor data into the message.
  ///
  /// Each converter writes its own data field plus shared fields (header, names).
  /// Redundant writes to shared fields are harmless — values should be identical
  /// across converters in the same publication group.
  ///
  /// @param tensor The output tensor to write.
  /// @param element_names Element names for the tensor (e.g., joint names).
  /// @param msg The typed message (type-erased via shared_ptr<void>).
  /// @param timestamp_ns Timestamp in nanoseconds.
  virtual void write(
    const torch::Tensor & tensor,
    const std::vector<std::string> & element_names,
    const std::shared_ptr<void> & msg,
    int64_t timestamp_ns) const = 0;

  /// Serialize the typed message to a SerializedMessage.
  /// @param msg The typed message (type-erased via shared_ptr<void>).
  /// @return Serialized ROS message.
  virtual std::shared_ptr<rclcpp::SerializedMessage> serialize(
    const std::shared_ptr<void> & msg) const = 0;
};

/// Template base class that provides create_message() and serialize()
/// for a specific ROS message type. Per-kind converters only need to
/// implement get_kind(), get_message_type(), and write().
template<typename MsgT>
class TypedOutputConverter : public TensorToMessageConverter
{
public:
  std::shared_ptr<void> create_message() const override
  {
    return std::make_shared<MsgT>();
  }

  std::shared_ptr<rclcpp::SerializedMessage> serialize(
    const std::shared_ptr<void> & msg) const override
  {
    auto serialized = std::make_shared<rclcpp::SerializedMessage>();
    rclcpp::Serialization<MsgT> serializer;
    serializer.serialize_message(static_cast<const MsgT *>(msg.get()), serialized.get());
    return serialized;
  }
};

/// Factory function type for creating output converters.
/// `shape` is the YAML-declared tensor shape (e.g. {1, 7} or {1, 30, 7});
/// factories may return nullptr if they don't handle this shape rank.
using OutputConverterFactory =
  std::function<std::shared_ptr<TensorToMessageConverter>(
      const std::vector<int64_t> & shape)>;

/// Registry for output converters.
class OutputConverterRegistry
{
public:
  static OutputConverterRegistry & instance()
  {
    static OutputConverterRegistry registry;
    return registry;
  }

  /// Add or replace the unlabeled compatibility factory for a kind.
  /// This factory takes priority for default lookup. Typed siblings remain
  /// available for explicit message-type selection.
  void register_converter(std::string kind, OutputConverterFactory factory)
  {
    auto & entries = kind_to_entries_[kind];
    for (auto & entry : entries) {
      if (entry.message_type.empty()) {
        entry.factory = std::move(factory);
        return;
      }
    }
    entries.push_back({"", std::move(factory)});
  }

  /// Register a converter factory for a (kind, message_type) pair.
  /// Registration order determines the default when multiple message types
  /// support the requested shape.
  void register_converter(
    std::string kind, std::string message_type, OutputConverterFactory factory)
  {
    auto & entries = kind_to_entries_[kind];
    for (auto & entry : entries) {
      if (entry.message_type == message_type) {
        entry.factory = std::move(factory);
        return;
      }
    }
    entries.push_back({std::move(message_type), std::move(factory)});
  }

  /// Create a converter for a specific kind and shape, optionally selecting
  /// a ROS message type. When message_type is empty, the first registered
  /// factory that supports the shape is used.
  std::shared_ptr<TensorToMessageConverter> create_for_kind(
    const std::string & kind,
    const std::vector<int64_t> & shape,
    const std::string & message_type = {}) const
  {
    const auto kind_it = kind_to_entries_.find(kind);
    if (kind_it == kind_to_entries_.end()) {
      return nullptr;
    }
    if (message_type.empty()) {
      for (const auto & entry : kind_it->second) {
        if (entry.message_type.empty()) {
          if (auto converter = entry.factory(shape)) {
            return converter;
          }
        }
      }
    }
    for (const auto & entry : kind_it->second) {
      if (entry.message_type.empty()) {
        continue;
      }
      if (!message_type.empty() && !entry.message_type.empty() &&
        entry.message_type != message_type)
      {
        continue;
      }
      auto converter = entry.factory(shape);
      if (converter &&
        (message_type.empty() || converter->get_message_type() == message_type))
      {
        return converter;
      }
    }
    if (!message_type.empty()) {
      for (const auto & entry : kind_it->second) {
        if (entry.message_type.empty()) {
          auto converter = entry.factory(shape);
          if (converter && converter->get_message_type() == message_type) {
            return converter;
          }
        }
      }
    }
    return nullptr;
  }

private:
  OutputConverterRegistry() = default;

  struct ConverterEntry
  {
    std::string message_type;
    OutputConverterFactory factory;
  };
  std::unordered_map<std::string, std::vector<ConverterEntry>> kind_to_entries_;
};

/// Initialize all built-in output converters.
/// Must be called before using the registry.
void initialize_output_converters();

}  // namespace isaac_ros_deploy_converters

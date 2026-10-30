// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <torch/torch.h>

#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cerrno>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "rclcpp/rclcpp.hpp"
#include "rclcpp/executors/single_threaded_executor.hpp"

#include "isaac_ros_deploy_converters/nodes/output_builder_node.hpp"
#include "isaac_ros_deploy_converters/utils/tensor_list_utils.hpp"
#include "isaac_ros_tensor_msgs/msg/tensor_list.hpp"

namespace
{

constexpr std::string_view kVelocityOutputConfig =
  R"(
models:
  policy:
    inputs: []
    outputs:
    - name: velocity_command_output
      kind: velocity_command
      shape: [1, 3]
)";

constexpr std::string_view kTwoVelocityOutputsConfig =
  R"(
models:
  policy:
    inputs: []
    outputs:
    - name: velocity_command_output
      kind: velocity_command
      shape: [1, 3]
    - name: stamped_velocity_command_output
      kind: velocity_command
      shape: [1, 3]
)";

constexpr std::string_view kMissingOutputNameConfig =
  R"(
models:
  policy:
    inputs: []
    outputs:
    - kind: velocity_command
      shape: [1, 3]
)";

class TemporaryConfigFile
{
public:
  explicit TemporaryConfigFile(std::string_view contents)
  {
    const auto test_tmpdir = std::getenv("TEST_TMPDIR");
    const auto temp_directory = test_tmpdir && test_tmpdir[0] != '\0' ?
      std::filesystem::path(test_tmpdir) : std::filesystem::temp_directory_path();
    auto path_template = (temp_directory / "output_message_type_XXXXXX").string();
    std::vector<char> writable_path(path_template.begin(), path_template.end());
    writable_path.push_back('\0');

    const int file_descriptor = mkstemp(writable_path.data());
    if (file_descriptor == -1) {
      throw std::system_error(errno, std::generic_category(), "mkstemp");
    }
    path_ = writable_path.data();
    if (close(file_descriptor) != 0) {
      const auto error = errno;
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
      throw std::system_error(error, std::generic_category(), "close");
    }

    std::ofstream config(path_);
    if (!config.is_open()) {
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
      throw std::runtime_error("Failed to open temporary output-builder config");
    }
    config << contents;
    if (!config.good()) {
      config.close();
      std::error_code ignored;
      std::filesystem::remove(path_, ignored);
      throw std::runtime_error("Failed to write temporary output-builder config");
    }
  }

  ~TemporaryConfigFile()
  {
    std::error_code ignored;
    std::filesystem::remove(path_, ignored);
  }

  TemporaryConfigFile(const TemporaryConfigFile &) = delete;
  TemporaryConfigFile & operator=(const TemporaryConfigFile &) = delete;

  const std::filesystem::path & path() const {return path_;}

private:
  std::filesystem::path path_;
};

class OutputBuilderMessageTypeTest : public ::testing::Test
{
protected:
  static void SetUpTestSuite()
  {
    rclcpp::init(0, nullptr);
  }

  static void TearDownTestSuite()
  {
    rclcpp::shutdown();
  }
};

std::string capture_configuration_error(
  std::string_view config_contents, std::vector<rclcpp::Parameter> overrides = {})
{
  const TemporaryConfigFile config(config_contents);
  overrides.emplace_back("config_path", config.path().string());
  const auto options = rclcpp::NodeOptions().parameter_overrides(overrides);

  try {
    const auto node =
      std::make_shared<isaac_ros_deploy_converters::OutputBuilderNode>(options);
    (void)node;
    ADD_FAILURE() << "Expected OutputBuilderNode configuration to fail";
  } catch (const std::runtime_error & error) {
    return error.what();
  }
  return {};
}

TEST_F(OutputBuilderMessageTypeTest, PublishesSelectedTwistStampedMessage)
{
  const TemporaryConfigFile config(kVelocityOutputConfig);
  const auto test_namespace =
    "/output_builder_message_type_test_" + std::to_string(getpid());
  const auto input_topic = test_namespace + "/output_tensors";
  const auto output_topic = test_namespace + "/cmd_vel";

  const auto options = rclcpp::NodeOptions().parameter_overrides({
      rclcpp::Parameter("config_path", config.path().string()),
      rclcpp::Parameter("input_topic", input_topic),
      rclcpp::Parameter(
        "output_message_type.velocity_command_output",
        "geometry_msgs/msg/TwistStamped"),
      rclcpp::Parameter("output_to_topic.velocity_command_output", output_topic),
    });
  const auto output_builder =
    std::make_shared<isaac_ros_deploy_converters::OutputBuilderNode>(options);
  const auto test_node = std::make_shared<rclcpp::Node>(
    "output_builder_message_type_test_" + std::to_string(getpid()));

  std::optional<geometry_msgs::msg::TwistStamped> received;
  const auto subscription =
    test_node->create_subscription<geometry_msgs::msg::TwistStamped>(
    output_topic, 10,
    [&received](const geometry_msgs::msg::TwistStamped::SharedPtr msg) {
      received = *msg;
    });
  const auto publisher =
    test_node->create_publisher<isaac_ros_tensor_msgs::msg::TensorList>(input_topic, 10);

  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(output_builder);
  executor.add_node(test_node);

  for (int i = 0;
    i < 500 &&
    (publisher->get_subscription_count() == 0 || subscription->get_publisher_count() == 0);
    ++i)
  {
    executor.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  ASSERT_GT(publisher->get_subscription_count(), 0U);
  ASSERT_GT(subscription->get_publisher_count(), 0U);

  const auto input = isaac_ros_deploy_converters::tensor_to_tensor_list(
    "velocity_command_output",
    torch::tensor({{1.25F, -2.5F, 0.75F}}, torch::kFloat32),
    rclcpp::Time(3, 500'000'042, RCL_ROS_TIME));
  publisher->publish(input);

  for (int i = 0; i < 500 && !received; ++i) {
    executor.spin_some();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_TRUE(received.has_value());
  EXPECT_EQ(received->header.stamp.sec, 3);
  EXPECT_EQ(received->header.stamp.nanosec, 500'000'042U);
  EXPECT_DOUBLE_EQ(received->twist.linear.x, 1.25);
  EXPECT_DOUBLE_EQ(received->twist.linear.y, -2.5);
  EXPECT_DOUBLE_EQ(received->twist.angular.z, 0.75);
}

TEST_F(OutputBuilderMessageTypeTest, RejectsUnsupportedExplicitMessageType)
{
  const auto error = capture_configuration_error(
    kVelocityOutputConfig,
    {
      rclcpp::Parameter(
        "output_message_type.velocity_command_output",
        "geometry_msgs/msg/TwistStamp"),
    });
  EXPECT_NE(error.find("geometry_msgs/msg/TwistStamp"), std::string::npos);
}

TEST_F(OutputBuilderMessageTypeTest, RejectsMessageTypeOverrideForUnknownOutput)
{
  const auto error = capture_configuration_error(
    kVelocityOutputConfig,
    {
      rclcpp::Parameter(
        "output_message_type.velocity_command_outpt",
        "geometry_msgs/msg/TwistStamped"),
    });
  EXPECT_NE(error.find("velocity_command_outpt"), std::string::npos);
}

TEST_F(OutputBuilderMessageTypeTest, RejectsOutputWithoutName)
{
  const auto error = capture_configuration_error(kMissingOutputNameConfig);
  EXPECT_NE(error.find("missing required 'name'"), std::string::npos);
}

TEST_F(OutputBuilderMessageTypeTest, RejectsMessageTypeOverrideForOutputWithoutKindOrShape)
{
  for (const std::string metadata : {
      "      kind: velocity_command\n", "      shape: [1, 3]\n"})
  {
    SCOPED_TRACE(metadata);
    auto config = std::string(kVelocityOutputConfig);
    config.erase(config.find(metadata), metadata.size());
    const auto error = capture_configuration_error(config,
        {rclcpp::Parameter(
          "output_message_type.velocity_command_output", "geometry_msgs/msg/TwistStamped")});
    EXPECT_NE(error.find("requires kind and shape"), std::string::npos);
  }
}

TEST_F(OutputBuilderMessageTypeTest, RejectsConflictingMessageTypesOnDefaultTopic)
{
  const auto error = capture_configuration_error(
    kTwoVelocityOutputsConfig,
    {
      rclcpp::Parameter(
        "output_message_type.stamped_velocity_command_output",
        "geometry_msgs/msg/TwistStamped"),
    });
  EXPECT_NE(error.find("cmd_vel"), std::string::npos);
  EXPECT_NE(
    error.find("output_to_topic.stamped_velocity_command_output"),
    std::string::npos);
}

}  // namespace

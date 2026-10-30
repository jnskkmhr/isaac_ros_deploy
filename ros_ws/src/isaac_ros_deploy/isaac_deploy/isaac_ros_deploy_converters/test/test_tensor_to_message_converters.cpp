// SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <torch/torch.h>

#include "geometry_msgs/msg/twist_stamped.hpp"
#include "isaac_ros_deploy_converters/converters/tensor_to_message_converter.hpp"

namespace
{

using isaac_ros_deploy_converters::OutputConverterRegistry;
using isaac_ros_deploy_converters::initialize_output_converters;

class LegacyVelocityConverter
  : public isaac_ros_deploy_converters::TypedOutputConverter<geometry_msgs::msg::TwistStamped>
{
public:
  std::string get_kind() const override {return "velocity_command";}
  std::string get_message_type() const override {return "geometry_msgs/msg/TwistStamped";}
  void write(
    const torch::Tensor &, const std::vector<std::string> &,
    const std::shared_ptr<void> &, int64_t) const override {}
};

class TensorToMessageConverterTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    initialize_output_converters();
  }
};

TEST_F(TensorToMessageConverterTest, DefaultVelocityCommandConverterIsTwist)
{
  const auto converter = OutputConverterRegistry::instance().create_for_kind(
    "velocity_command", {1, 3});

  ASSERT_NE(converter, nullptr);
  EXPECT_EQ(converter->get_message_type(), "geometry_msgs/msg/Twist");
}

TEST_F(TensorToMessageConverterTest, ExplicitVelocityCommandConverterIsTwistStamped)
{
  const auto converter = OutputConverterRegistry::instance().create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/TwistStamped");

  ASSERT_NE(converter, nullptr);
  EXPECT_EQ(converter->get_message_type(), "geometry_msgs/msg/TwistStamped");
}

TEST_F(TensorToMessageConverterTest, UnknownMessageTypeReturnsNullptr)
{
  const auto converter = OutputConverterRegistry::instance().create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/Nonexistent");

  EXPECT_EQ(converter, nullptr);
}

TEST_F(TensorToMessageConverterTest, LegacyRegistrationInterfaceRemainsSupported)
{
  auto registry = OutputConverterRegistry::instance();
  const auto twist_converter = registry.create_for_kind("velocity_command", {1, 3});
  const auto stamped_converter = registry.create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/TwistStamped");
  ASSERT_NE(twist_converter, nullptr);
  ASSERT_NE(stamped_converter, nullptr);

  const auto custom_converter = std::make_shared<LegacyVelocityConverter>();
  registry.register_converter(
    "velocity_command",
    [custom_converter](const std::vector<int64_t> &) {return custom_converter;});

  const auto converter = registry.create_for_kind("velocity_command", {1, 3});
  ASSERT_NE(converter, nullptr);
  EXPECT_EQ(converter, custom_converter);

  const auto selected_twist = registry.create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/Twist");
  ASSERT_NE(selected_twist, nullptr);
  EXPECT_NE(selected_twist, custom_converter);
  EXPECT_EQ(selected_twist->get_message_type(), twist_converter->get_message_type());
  const auto selected_stamped = registry.create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/TwistStamped");
  ASSERT_NE(selected_stamped, nullptr);
  EXPECT_NE(selected_stamped, custom_converter);
  EXPECT_EQ(selected_stamped->get_message_type(), stamped_converter->get_message_type());
}

TEST_F(TensorToMessageConverterTest, TwistStampedContainsVelocityAndTimestamp)
{
  const auto converter = OutputConverterRegistry::instance().create_for_kind(
    "velocity_command", {1, 3}, "geometry_msgs/msg/TwistStamped");
  ASSERT_NE(converter, nullptr);

  const auto message = converter->create_message();
  converter->write(
    torch::tensor({{1.25F, -2.5F, 0.75F}}, torch::kFloat32), {}, message,
    3'500'000'042);

  const auto & stamped = *static_cast<geometry_msgs::msg::TwistStamped *>(message.get());
  EXPECT_EQ(stamped.header.stamp.sec, 3);
  EXPECT_EQ(stamped.header.stamp.nanosec, 500'000'042U);
  EXPECT_DOUBLE_EQ(stamped.twist.linear.x, 1.25);
  EXPECT_DOUBLE_EQ(stamped.twist.linear.y, -2.5);
  EXPECT_DOUBLE_EQ(stamped.twist.angular.z, 0.75);
}

}  // namespace

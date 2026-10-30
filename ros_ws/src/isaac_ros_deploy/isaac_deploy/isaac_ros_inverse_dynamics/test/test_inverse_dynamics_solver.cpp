// Copyright 2026 NVIDIA CORPORATION & AFFILIATES
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

#include <cstdio>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include "gtest/gtest.h"
#include "isaac_ros_inverse_dynamics/inverse_dynamics_solver.hpp"

namespace isaac_ros_inverse_dynamics
{
namespace
{

// Independent vertical sliders have analytic tau_i = mass_i * (a_i + 9.81).
// Controller order intentionally differs from URDF order and includes an absent joint.
class InverseDynamicsSolverTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    std::string pattern = ::testing::TempDir() + "inverse_dynamics_XXXXXX";
    std::vector<char> filename(pattern.begin(), pattern.end());
    filename.push_back('\0');
    const int fd = mkstemp(filename.data());
    ASSERT_NE(fd, -1);
    close(fd);
    path_ = filename.data();
    std::ofstream urdf(path_);
    urdf << R"(<robot name="sliders">
      <link name="base"/>
      <link name="light"><inertial><mass value="2"/>
        <inertia ixx="1" ixy="0" ixz="0" iyy="1" iyz="0" izz="1"/>
      </inertial></link>
      <link name="heavy"><inertial><mass value="3"/>
        <inertia ixx="1" ixy="0" ixz="0" iyy="1" iyz="0" izz="1"/>
      </inertial></link>
      <joint name="light_joint" type="prismatic">
        <parent link="base"/><child link="light"/><axis xyz="0 0 1"/>
        <limit lower="-10" upper="10" effort="100" velocity="10"/>
      </joint>
      <joint name="heavy_joint" type="prismatic">
        <parent link="base"/><child link="heavy"/><axis xyz="0 0 1"/>
        <limit lower="-10" upper="10" effort="100" velocity="10"/>
      </joint>
    </robot>)";
    urdf.close();
    ASSERT_TRUE(urdf.good());
  }

  void TearDown() override
  {
    if (!path_.empty()) {
      std::remove(path_.c_str());
    }
  }

  std::string path_;
  const std::vector<std::string> joints_{"heavy_joint", "missing", "light_joint"};
};

TEST_F(InverseDynamicsSolverTest, GravityUsesControllerOrderAndZerosMissingJoints)
{
  InverseDynamicsSolver solver(path_, joints_);
  EXPECT_EQ(solver.num_joints(), 3u);
  EXPECT_EQ(solver.num_mapped_joints(), 2u);
  Eigen::VectorXd q = Eigen::VectorXd::Zero(3);
  Eigen::VectorXd tau = Eigen::VectorXd::Constant(3, 123.0);
  solver.computeGravityCompensation(q, tau);
  EXPECT_NEAR(tau[0], 3.0 * 9.81, 1e-10);
  EXPECT_DOUBLE_EQ(tau[1], 0.0);
  EXPECT_NEAR(tau[2], 2.0 * 9.81, 1e-10);
}

TEST_F(InverseDynamicsSolverTest, RneaMatchesAnalyticTorquesAndRefreshesScratchBuffers)
{
  InverseDynamicsSolver solver(path_, joints_);
  Eigen::VectorXd q(3), v(3), a(3), tau(3);
  q << 0.4, 99.0, -0.2;
  v << 0.2, 99.0, -0.3;
  a << 1.5, 99.0, -2.0;
  solver.computeInverseDynamics(q, v, a, tau);
  EXPECT_NEAR(tau[0], 3.0 * (9.81 + 1.5), 1e-10);
  EXPECT_DOUBLE_EQ(tau[1], 0.0);
  EXPECT_NEAR(tau[2], 2.0 * (9.81 - 2.0), 1e-10);

  Eigen::VectorXd gravity(3);
  solver.computeGravityCompensation(q, gravity);
  v.setZero();
  a.setZero();
  solver.computeInverseDynamics(q, v, a, tau);
  EXPECT_TRUE(tau.isApprox(gravity, 1e-12));
  EXPECT_NEAR(tau[0], 3.0 * 9.81, 1e-10);
  EXPECT_NEAR(tau[2], 2.0 * 9.81, 1e-10);
}

TEST_F(InverseDynamicsSolverTest, RejectsAnUnmappedController)
{
  EXPECT_THROW(InverseDynamicsSolver(path_, {"missing"}), std::runtime_error);
  EXPECT_THROW(InverseDynamicsSolver(path_, {}), std::runtime_error);
}

TEST_F(InverseDynamicsSolverTest, RejectsMalformedUrdf)
{
  std::ofstream(path_) << "not a URDF";
  EXPECT_THROW(InverseDynamicsSolver(path_, joints_), std::exception);
}

}  // namespace
}  // namespace isaac_ros_inverse_dynamics

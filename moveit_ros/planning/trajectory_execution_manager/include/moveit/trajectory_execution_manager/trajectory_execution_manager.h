/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2012, Willow Garage, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.
 *   * Neither the name of Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

/* Author: Ioan Sucan */

#pragma once

#include <moveit/macros/class_forward.h>
#include <moveit/robot_model/robot_model.h>
#include <moveit/planning_scene_monitor/current_state_monitor.h>
#include <moveit_msgs/RobotTrajectory.h>
#include <sensor_msgs/JointState.h>
#include <std_msgs/String.h>
#include <ros/ros.h>
#include <moveit/controller_manager/controller_manager.h>
#include <boost/thread.hpp>
#include <pluginlib/class_loader.hpp>

#include <memory>

namespace trajectory_execution_manager
{
MOVEIT_CLASS_FORWARD(TrajectoryExecutionManager);

// Two modes based on manage_controllers
// Managed controllers   - Activate controllers if they are not active
// Unmanaged controllers - Fail if required controllers are not active
class TrajectoryExecutionManager
{
public:
  static const std::string EXECUTION_EVENT_TOPIC;

  /// Definition of the function signature that is called when the execution of all the pushed trajectories completes.
  /// The status of the overall execution is passed as argument
  typedef boost::function<void(const moveit_controller_manager::ExecutionStatus&)> ExecutionCompleteCallback;

  /// Definition of the function signature that is called when the execution of a pushed trajectory completes
  /// successfully.
  typedef boost::function<void(std::size_t)> PathSegmentCompleteCallback;

  /// Data structure that represents information necessary to execute a trajectory
  struct TrajectoryExecutionContext
  {
    /// The controllers to use for executing the different trajectory parts;
    std::vector<std::string> controllers_;

    /// The trajectory to execute, split in different parts (by joints),
    /// each set of joints corresponding to one controller
    std::vector<moveit_msgs::RobotTrajectory> trajectory_parts_;
  };

  /// Load the controller manager plugin, start listening for events on a topic.
  TrajectoryExecutionManager(const robot_model::RobotModelConstPtr& robot_model,
                             const planning_scene_monitor::CurrentStateMonitorPtr& csm);

  /// Load the controller manager plugin, start listening for events on a topic.
  TrajectoryExecutionManager(const robot_model::RobotModelConstPtr& robot_model,
                             const planning_scene_monitor::CurrentStateMonitorPtr& csm, bool manage_controllers);

  /// Destructor. Cancels all running trajectories (if any)
  ~TrajectoryExecutionManager();

  /// If this function returns true, then this instance of the manager is allowed to load/unload/switch controllers
  bool isManagingControllers() const;

  /// Get the instance of the controller manager used (this is the plugin instance loaded)
  const moveit_controller_manager::MoveItControllerManagerPtr& getControllerManager() const;

  /** \brief Execute a named event (e.g., 'stop') */
  void processEvent(const std::string& event);

  /** \brief Make sure the active controllers are such that trajectories that actuate joints in the specified group can
     be executed.
      \note If manage_controllers_ is false and the controllers that happen to be active do not cover the joints in the
     group to be actuated, this function fails. */
  bool ensureActiveControllersForGroup(const std::string& group);

  /** \brief Make sure the active controllers are such that trajectories that actuate joints in the specified set can be
     executed.
      \note If manage_controllers_ is false and the controllers that happen to be active do not cover the joints to be
     actuated, this function fails. */
  bool ensureActiveControllersForJoints(const std::vector<std::string>& joints);

  /** \brief Make sure a particular controller is active.
      \note If manage_controllers_ is false and the controllers that happen to be active to not include the one
     specified as argument, this function fails. */
  bool ensureActiveController(const std::string& controller);

  /** \brief Make sure a particular set of controllers are active.
      \note If manage_controllers_ is false and the controllers that happen to be active to not include the ones
     specified as argument, this function fails. */
  bool ensureActiveControllers(const std::vector<std::string>& controllers);

  /** \brief Check if a controller is active */
  bool isControllerActive(const std::string& controller);

  /** \brief Check if a set of controllers are active */
  bool areControllersActive(const std::vector<std::string>& controllers);

  /// Add a trajectory for future execution. Optionally specify controllers to consider using for the
  /// trajectory. Multiple controllers can be used simultaneously to execute the different parts of the trajectory.
  /// If multiple controllers can be used, preference is given to the already loaded ones.
  /// If no controllers are specified, use defaults. */
  bool push(const moveit_msgs::RobotTrajectory& trajectory, const std::string& controller = "");
  /// \copydoc push
  bool push(const trajectory_msgs::JointTrajectory& trajectory, const std::string& controller = "");
  /// \copydoc push
  bool push(const trajectory_msgs::JointTrajectory& trajectory, const std::vector<std::string>& controllers);
  /// \copydoc push
  bool push(const moveit_msgs::RobotTrajectory& trajectory, const std::vector<std::string>& controllers);

  /// Get the trajectories to be executed
  const std::vector<TrajectoryExecutionContext*>& getTrajectories() const;

  /// Start the execution of pushed trajectories; this does not wait for completion, but calls a callback when done.
  void execute(const ExecutionCompleteCallback& callback = ExecutionCompleteCallback(), bool auto_clear = true);

  /// Start the execution of pushed trajectories; this does not wait for completion, but calls a callback when done. A
  /// callback is also called for every trajectory part that completes successfully.
  void execute(const ExecutionCompleteCallback& callback, const PathSegmentCompleteCallback& part_callback,
               bool auto_clear = true);

  /// This is a blocking call for the execution of the passed in trajectories. This just calls execute() and
  /// waitForExecution()
  moveit_controller_manager::ExecutionStatus executeAndWait(bool auto_clear = true);

  /// Add a trajectory for immediate execution. Optionally specify controllers to use for the trajectory.
  /// If no controllers are specified, use default. This call is non-blocking.
  bool pushAndExecute(const moveit_msgs::RobotTrajectory& trajectory, const std::string& controller = "");
  /// \copydoc pushAndExecute
  bool pushAndExecute(const trajectory_msgs::JointTrajectory& trajectory, const std::string& controller = "");
  /// \copydoc pushAndExecute
  bool pushAndExecute(const trajectory_msgs::JointTrajectory& trajectory, const std::vector<std::string>& controllers);
  /// \copydoc pushAndExecute
  bool pushAndExecute(const moveit_msgs::RobotTrajectory& trajectory, const std::vector<std::string>& controllers);
  /// \copydoc pushAndExecute
  /// Trajectory consists of a single waypoint specified by state
  bool pushAndExecute(const sensor_msgs::JointState& state, const std::string& controller = "");
  /// \copydoc pushAndExecute
  /// Trajectory consists of a single waypoint specified by state
  bool pushAndExecute(const sensor_msgs::JointState& state, const std::vector<std::string>& controllers);

  /// Wait until the execution is complete. This only works for executions started by execute().  If you call this after
  /// pushAndExecute(), it will immediately stop execution.
  moveit_controller_manager::ExecutionStatus waitForExecution();

  /// Get the state that the robot is expected to be at, given current time, after execute() has been called. The return
  /// value is a pair of two index values:
  /// first = the index of the trajectory to be executed (in the order push() was called), second = the index of the
  /// point within that trajectory.
  /// Values of -1 are returned when there is no trajectory being executed, or if the trajectory was passed using
  /// pushAndExecute().
  std::pair<int, int> getCurrentExpectedTrajectoryIndex() const;

  /// Return the controller status for the last attempted execution
  moveit_controller_manager::ExecutionStatus getLastExecutionStatus() const;

  /// Stop whatever executions are active, if any
  void stopExecution(bool auto_clear = true);

  /// Clear the trajectories to execute
  void clear();

  /// Enable or disable the monitoring of trajectory execution duration. If a controller takes
  /// longer than expected, the trajectory is canceled
  void enableExecutionDurationMonitoring(bool flag);

  /// When determining the expected duration of a trajectory, this multiplicative factor is applied
  /// to get the allowed duration of execution
  void setAllowedExecutionDurationScaling(double scaling);

  /// When determining the expected duration of a trajectory, this multiplicative factor is applied
  /// to allow more than the expected execution time before triggering trajectory cancel
  void setAllowedGoalDurationMargin(double margin);

  [[deprecated("Interface was broken and got removed. Scale trajectories manually before execution.")]] void
  setExecutionVelocityScaling(double scaling);

  /// Set joint-value tolerance for validating trajectory's start point against current robot state
  void setAllowedStartTolerance(double tolerance);

  /// Enable or disable waiting for trajectory completion
  void setWaitForTrajectoryCompletion(bool flag);

private:
  struct ControllerInformation
  {
    std::string name_;
    std::set<std::string> joints_;
    std::set<std::string> overlapping_controllers_;
    moveit_controller_manager::MoveItControllerManager::ControllerState state_;
    ros::Time last_update_;

    bool operator<(ControllerInformation& other) const
    {
      if (joints_.size() != other.joints_.size())
        return joints_.size() < other.joints_.size();
      return name_ < other.name_;
    }
  };

  void initialize();

  /// Get controller information (known_controllers_) from ControllerManager plugin
  void reloadControllerInformation();

  /// Validate first point of trajectory matches current robot state
  bool validate(const TrajectoryExecutionContext& context) const;

  /// Initialize context based on requested trajectory and controllers
  bool configure(TrajectoryExecutionContext& context, const moveit_msgs::RobotTrajectory& trajectory,
                 const std::vector<std::string>& controllers);

  /// Refresh controller state if outdated
  void updateControllersState(const ros::Duration& age);
  void updateControllerState(const std::string& controller, const ros::Duration& age);
  void updateControllerState(ControllerInformation& ci, const ros::Duration& age);

  /// Split trajectory into joint groups as required by controllers
  bool distributeTrajectory(const moveit_msgs::RobotTrajectory& trajectory, const std::vector<std::string>& controllers,
                            std::vector<moveit_msgs::RobotTrajectory>& parts);

  /// Compile controller_count controllers to control actuated_joints
  bool findControllers(const std::set<std::string>& actuated_joints, std::size_t controller_count,
                       const std::vector<std::string>& available_controllers,
                       std::vector<std::string>& selected_controllers);
  /// Do controllers cover all actuated_joints?
  bool checkControllerCombination(std::vector<std::string>& controllers, const std::set<std::string>& actuated_joints);

  void generateControllerCombination(std::size_t start_index, std::size_t controller_count,
                                     const std::vector<std::string>& available_controllers,
                                     std::vector<std::string>& selected_controllers,
                                     std::vector<std::vector<std::string> >& selected_options,
                                     const std::set<std::string>& actuated_joints);
  /// Decide on a set of controllers for actuated_joints
  bool selectControllers(const std::set<std::string>& actuated_joints,
                         const std::vector<std::string>& available_controllers,
                         std::vector<std::string>& selected_controllers);

  /// Execute trajectory number part_index, previously added via push()
  bool executePart(std::size_t part_index);

  /// Monitor joint state and wait for stable positions
  bool waitForRobotToStop(const TrajectoryExecutionContext& context, double wait_time = 1.0);

  /// Cancel all active controllers
  /// Requires caller to hold execution_state_mutex_
  void stopExecutionInternal();

  /// Thread for asynchronous execution when using push()
  void executeThread(const ExecutionCompleteCallback& callback, const PathSegmentCompleteCallback& part_callback,
                     bool auto_clear);

  /// Thread for asynchronous execution when using pushAndExecute()
  void continuousExecutionThread();

  /// Callback for ROS message interface
  void receiveEvent(const std_msgs::StringConstPtr& event);

  /// Load ROS parameters from internal node handle
  void loadControllerParams();

  /// Name of this class for logging
  const std::string name_ = "trajectory_execution_manager";

  /// Verbose output (currently hardcoded to false)
  bool verbose_;

  robot_model::RobotModelConstPtr robot_model_;

  /// \name push() / execute() mechanism
  /// \{

  /// Thread used to execute trajectories using the execute() command
  std::unique_ptr<boost::thread> execution_thread_;
  /// Trajectories scheduled for execution with execute()
  std::vector<TrajectoryExecutionContext*> trajectories_;
  /// Index of currently executing trajectory in trajectories_
  int current_context_;
  /// Controllers currently used by execute()
  std::vector<moveit_controller_manager::MoveItControllerHandlePtr> active_handles_;
  /// Indicate finished execution
  bool execution_complete_;
  /// Notify waitForExecution upon completion of execute()
  boost::condition_variable execution_complete_condition_;
  boost::mutex execution_state_mutex_;

  /// used to find current expected trajectory location
  std::vector<ros::Time> time_index_;
  mutable boost::mutex time_index_mutex_;
  /// \}

  /// \name pushAndExecute() mechanism
  /// \{
  /// Thread executing trajectories using pushAndExecute()
  std::unique_ptr<boost::thread> continuous_execution_thread_;
  /// Request shutdown of thread
  bool run_continuous_execution_thread_;
  /// Trajectories scheduled for execution with pushAndExecute()
  std::deque<TrajectoryExecutionContext*> continuous_execution_queue_;
  /// Protects continuous_execution_thread_ and continuous_execution_queue_
  boost::mutex continuous_execution_mutex_;
  /// Indicate request to clear trajectories scheduled via pushAndExecute()
  bool stop_continuous_execution_;
  /// Wake up continuousExecutionThread
  /// Also signals empty processing queue from within continuousExecutionThread
  boost::condition_variable continuous_execution_condition_;
  /// \}

  moveit_controller_manager::ExecutionStatus last_execution_status_;

  /// \name Execution monitoring
  /// \{

  /// Cancel trajectories that take too long?
  bool execution_duration_monitoring_;
  /// Default parameters for duration monitoring
  double allowed_execution_duration_scaling_;
  double allowed_goal_duration_margin_;
  /// Controller-specific overwrites
  std::map<std::string, double> controller_allowed_execution_duration_scaling_;
  std::map<std::string, double> controller_allowed_goal_duration_margin_;

  /// joint tolerance for validate(): radians for revolute joints
  double allowed_start_tolerance_;

  bool wait_for_trajectory_completion_;
  /// \}

  /// \name Framework interfaces
  /// \{
  planning_scene_monitor::CurrentStateMonitorPtr csm_;
  ros::NodeHandle node_handle_;
  ros::NodeHandle root_node_handle_;
  ros::Subscriber event_topic_subscriber_;

  class DynamicReconfigureImpl;
  DynamicReconfigureImpl* reconfigure_impl_;
  /// \}

  /// \name ControllerManager
  /// \{
  std::unique_ptr<pluginlib::ClassLoader<moveit_controller_manager::MoveItControllerManager> >
      controller_manager_loader_;
  moveit_controller_manager::MoveItControllerManagerPtr controller_manager_;
  bool manage_controllers_;
  std::map<std::string, ControllerInformation> known_controllers_;
  /// \}
};
}

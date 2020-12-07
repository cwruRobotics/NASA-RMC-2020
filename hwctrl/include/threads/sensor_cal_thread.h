#pragma once

#include <ros/callback_queue.h>
#include <ros/ros.h>

#include <boost/utility/string_view.hpp>

#include "threads/sensor_thread.h"

class SensorCalThread : SensorThread {
 public:
  SensorCalThread(ros::NodeHandle);
  virtual ~SensorCalThread() = default;

  void operator()();

 private:
  void calibrate_sensor_with_name(boost::string_view name,
                                  std::vector<Calibration>& cals);
};
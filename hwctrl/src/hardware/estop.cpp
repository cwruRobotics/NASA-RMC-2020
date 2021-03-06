#include "hardware/estop.h"

#include  <ros/ros.h>

EStop::EStop(GpioSensorArgs) : GpioSensorArgsPass {}

void EStop::update() {
  auto state = m_gpio->is_reset();
  auto msg = boost::make_shared<std_msgs::Bool>();
  msg->data = state;
  m_pub.publish(msg);
  if(state != m_last_state) {
    // print out warning to ros console
    ROS_INFO("EStop state changed!");
  }
  m_last_state = state;
  m_update = false;
}

#!/usr/bin/python3
import rospy
from obstacle_detection.realsense_motion_tracking import RealSenseIMU

if __name__ == '__main__':
    try:
        imu_node = RealSenseIMU()
    except rospy.exceptions.ROSInterruptException:
        pass

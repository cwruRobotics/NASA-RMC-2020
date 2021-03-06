#!/usr/bin/env python3
import rospy
from std_srvs.srv import Trigger

rospy.init_node("randomize_tester_node", anonymous=False)
rospy.loginfo("Waiting for randomize_arena_service")
rospy.wait_for_service("/randomize_rock_state")
rospy.wait_for_service("/randomize_robot_state")
rospy.wait_for_service("/randomize_hole_state")

while not rospy.is_shutdown():
    rospy.sleep(2)

    try:
        rock_s = rospy.ServiceProxy('/randomize_rock_state', Trigger)
        robot_s = rospy.ServiceProxy('/randomize_robot_state', Trigger)
        hole_s = rospy.ServiceProxy('/randomize_hole_state', Trigger)

        rock_s()
        robot_s()
        hole_s()
    except rospy.ServiceException as e:
        print("Failed to randomize arena:\n%s"%e)

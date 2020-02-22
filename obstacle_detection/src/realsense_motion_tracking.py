#!/usr/bin/env python3
import os
import sys
import glob
import cv2
import math
import rospy
import math
import matplotlib
matplotlib.use('Agg')  # necessary when plotting without $DISPLAY
import numpy as np
import pandas as pd
import pyrealsense2 as rs
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from scipy import signal
from scipy.spatial.transform import Rotation as R
from scipy.ndimage import gaussian_filter

from geometry_msgs.msg import Vector3

# Configure depth and color streams
pipeline = rs.pipeline()
config = rs.config()
config.enable_stream(rs.stream.accel)
config.enable_stream(rs.stream.gyro)

# Start streaming
pipeline.start(config)


class RealSenseIMU:

    def __init__(self):
        self.first_frame = True # initial IMU reference frame
        self.alpha = 0.98
        self.motion_topic = 'realsense_angle'
        self.viz_dir = 'realsense_angle/'
        self.window = 5

        os.makedirs(self.viz_dir, exist_ok=True)
        try:
            files = glob.glob('%s/*' % self.viz_dir)
            for f in files:
                os.remove(f)
        except Exception as e:
            print(e)

        print('Booting up node...')
        rospy.init_node('realsenseMotion', anonymous=True)

    def listen_for_frames(self):
        xs = []
        ys = []
        zs = []
        while not rospy.is_shutdown():
            # Wait for a coherent pair of frames: depth and color
            frames = pipeline.wait_for_frames()
            #gather IMU data
            # integration of IMU data referenced from https://github.com/IntelRealSense/librealsense/issues/4391
            accel = frames[0].as_motion_frame().get_motion_data()
            gyro = frames[1].as_motion_frame().get_motion_data()
            ts = frames.get_timestamp()

            #calculation for the first frame
            if (self.first_frame):
                self.first_frame = False
                last_ts_gyro = ts
                # accelerometer calculation
                accel_angle_z = math.atan2(accel.y, accel.z)
                accel_angle_x = math.atan2(accel.x, math.sqrt(accel.y * accel.y + accel.z * accel.z))
                accel_angle_y = math.pi
                continue

            #calculation for the second frame onwards
            # gyrometer calculations
            dt_gyro = (ts - last_ts_gyro) / 1000
            last_ts_gyro = ts

            gyro_angle_x = gyro.x * dt_gyro
            gyro_angle_y = gyro.y * dt_gyro
            gyro_angle_z = gyro.z * dt_gyro

            totalgyroangleX = accel_angle_x + gyro_angle_x
            totalgyroangleY = accel_angle_y + gyro_angle_y
            totalgyroangleZ = accel_angle_z + gyro_angle_z

            #accelerometer calculation
            accel_angle_z = math.atan2(accel.y, accel.z)
            accel_angle_x = math.atan2(accel.x, math.sqrt(accel.y * accel.y + accel.z * accel.z))
            accel_angle_y = math.pi


            #combining gyrometer and accelerometer angles
            combinedangleX = totalgyroangleX * self.alpha + accel_angle_x * (1 - self.alpha)
            combinedangleZ = totalgyroangleZ * self.alpha + accel_angle_z * (1 - self.alpha)
            combinedangleY = totalgyroangleY

            xs.append(combinedangleX)
            ys.append(combinedangleY)
            zs.append(combinedangleZ)

            if len(xs) >= self.window:
                combinedangleX = np.mean(xs)
                combinedangleY = np.mean(ys)
                combinedangleZ = np.mean(zs)

                print("Angle -  X: " + str(round(combinedangleX,2)) + "   Y: " + str(round(combinedangleY,2)) + "   Z: " + str(round(combinedangleZ,2)))

                fig = plt.figure()
                ax = fig.add_subplot(111, projection='3d')

                r = R.from_euler('zyx', [combinedangleX, combinedangleY, combinedangleZ])
                x_new = r.apply(np.array([1, 0, 0]))
                y_new = r.apply(np.array([0, 1, 0]))
                z_new = r.apply(np.array([0, 0, 1]))

                ax.plot([0, x_new[0]], [0, x_new[1]], [0, x_new[2]], label='x')
                ax.plot([0, y_new[0]], [0, y_new[1]], [0, y_new[2]], label='y')
                ax.plot([0, z_new[0]], [0, z_new[1]], [0, z_new[2]], label='z')

                ax.set_xlim(-1.5, 1.5)
                ax.set_ylim(-1.5, 1.5)
                ax.set_zlim(-1.5, 1.5)
                ax.set_xlabel('X')
                ax.set_ylabel('Y')
                ax.set_zlabel('Z')
                ax.legend(loc='best')
                #ax.plot([0, combinedangleX], [0, combinedangleY], [0, combinedangleZ])

                fig.savefig(self.viz_dir + '%d.png' % len(os.listdir(self.viz_dir)))
                plt.close()

                try:
                    pub = rospy.Publisher('realsense_orientation', Vector3, queue_size=1)
                    pub.publish(Vector3(combinedangleX, combinedangleY, combinedangleZ))
                except rospy.ROSInterruptException as e:
                    print(e.getMessage())
                    pass

if __name__ == '__main__':
    try:
        imu_node = RealSenseIMU()
        print('Listening for frames...')
        imu_node.listen_for_frames()
    except rospy.exceptions.ROSInterruptException:
        pass
    finally:
        # Stop streaming
        pipeline.stop()

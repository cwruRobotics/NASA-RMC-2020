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
import matplotlib.pyplot as plt
from scipy import signal
from scipy.spatial.transform import Rotation as R
from scipy import ndimage

from std_msgs.msg import Header
from nav_msgs.msg import MapMetaData, OccupancyGrid, Odometry
from geometry_msgs.msg import Pose, Point, Quaternion


class GlobalOccupancyGrid:

    def __init__(self):
        self.save_imgs = True
        self.save_data = True
        self.robot_x = []
        self.robot_y = []
        self.robot_pitch = []
        self.local_grid = None
        self.arena_length = 5.4
        self.arena_width = 3.6
        self.resolution = 0.15
        self.global_grid_shape = (int(self.arena_length / self.resolution), int(self.arena_width / self.resolution))
        self.global_grid = np.zeros(self.global_grid_shape)
        self.global_counts = np.zeros_like(self.global_grid)  # keeps track of number of measurements for each cell
        self.global_totals = np.zeros_like(self.global_grid)  # keeps track of sum of measurements for each cell
        self.localization_topic = 'odometry/filtered_map'
        self.local_grid_topic = 'local_occupancy_grid'
        self.viz_dir = 'global_map/'
        self.data_dir = 'occupancy_grid_data/'

        print('Booting up node...')
        rospy.init_node('globalMap', anonymous=True)

        os.makedirs(self.viz_dir, exist_ok=True)
        os.makedirs(self.data_dir, exist_ok=True)
        os.makedirs(self.data_dir + 'localization', exist_ok=True)
        try:
            files = glob.glob('%s/*' % self.viz_dir)
            for f in files:
                os.remove(f)
        except Exception as e:
            print(e)
        try:
            files = glob.glob('%s/*' % self.data_dir)
            for f in files:
                os.remove(f)
        except Exception as e:
            print(e)
        try:
            files = glob.glob('%s/*' % (self.data_dir + 'localization'))
            for f in files:
                os.remove(f)
        except Exception as e:
            print(e)

    def localization_listener(self, msg):
        pose = msg.pose.pose
        covariance = msg.pose.covariance
        covariance = np.reshape(covariance, (6, 6))
        self.robot_x.append(pose.position.x)
        self.robot_y.append(pose.position.y)
        quat = pose.orientation
        euler = R.from_quat([quat.x, quat.y, quat.z, quat.w]).as_euler('xyz')
        self.robot_pitch.append(euler[2])  # rotation about vertical z-axis
        print('X: %.4f \tY: %.4f \tpitch: %.4f' % (self.robot_x[-1], self.robot_y[-1], self.robot_pitch[-1]))

    def has_localization_data(self):
        return len(self.robot_x) > 0 and len(self.robot_y) > 0 and len(self.robot_pitch) > 0

    def paste_slices(self, tup):
        pos, w, max_w = tup
        wall_min = max(pos, 0)
        wall_max = min(pos + w, max_w)
        block_min = -min(pos, 0)
        block_max = max_w - max(pos + w, max_w)
        block_max = block_max if block_max != 0 else None
        return slice(wall_min, wall_max), slice(block_min, block_max)

    def paste(self, wall, block, loc):
        loc_zip = zip(loc, block.shape, wall.shape)
        wall_slices, block_slices = zip(*map(self.paste_slices, loc_zip))
        wall[wall_slices] += block[block_slices]

    def local_grid_callback(self, msg):
        grid_size = msg.info.width
        self.local_grid = np.reshape(msg.data, (grid_size, grid_size))
        if self.has_localization_data():
            if self.save_data:
                print('Saving...')
                np.save('%s/%d.npy' % (self.data_dir, len(os.listdir(self.data_dir))), self.local_grid)
                np.save('%s/%d.npy' % (self.data_dir + 'localization', len(os.listdir(self.data_dir + 'localization'))),
                        np.array([self.robot_x[-1], self.robot_y[-1], self.robot_pitch[-1]]))
            angle = self.robot_pitch[-1]
            local_origin = np.array([0.41 + grid_size / 2 * msg.info.resolution, -0.13]).reshape(-1, 1)
            local_origin = np.array([[np.cos(angle), -np.sin(angle)],
                                     [np.sin(angle), np.cos(angle)]]).dot(local_origin)
            local_origin = (int((local_origin[0, 0] + self.robot_x[-1]) / self.resolution), int((local_origin[1, 0] + self.robot_y[-1]) / self.resolution))

            # local_origin = (int(self.robot_x[-1] - (grid_size / 2) * self.resolution * np.cos(self.robot_pitch[-1])),
                            # int(self.robot_y[-1] - (grid_size / 2) * self.resolution * np.sin(self.robot_pitch[-1])))

            rotated_grid = ndimage.rotate(self.local_grid, -self.robot_pitch[-1] * 180 / math.pi + 90, mode='constant', cval=-1, reshape=True)

            local_origin = (local_origin[0] - int(rotated_grid.shape[0] / 2), local_origin[1] - int(rotated_grid.shape[1] / 2))

            print(self.global_totals.shape, local_origin, rotated_grid.shape)

            self.paste(self.global_totals, rotated_grid, local_origin)

            #self.global_totals[global_left:global_right, global_bottom:global_top] += rotated_grid
            counts = np.ones_like(rotated_grid)
            counts[rotated_grid == -1] = 0
            self.paste(self.global_counts, counts, local_origin)
            #self.global_counts[local_origin[1]: local_origin[1] + rotated_grid.shape[1], local_origin[0] + local_origin[0] + rotated_grid.shape[0]] += counts
            self.global_grid = np.divide(self.global_totals, self.global_counts, out=np.zeros_like(self.global_totals), where=self.global_counts!=0)
            #np.nan_to_num(self.global_grid, copy=False)
            self.global_grid /= np.max(self.global_grid)  # ensure values are 0-1

            header = Header()
            header.stamp = rospy.Time.now()
            header.frame_id = 'map'

            map_meta_data = MapMetaData()
            map_meta_data.map_load_time = rospy.Time.now()
            map_meta_data.resolution = self.resolution  # each cell is 15cm
            map_meta_data.width = self.global_grid_shape[1]
            map_meta_data.height = self.global_grid_shape[0]
            map_meta_data.origin = Pose(Point(0, 0, 0),
                                        Quaternion(0, 0, 0, 1))

            grid_msg = OccupancyGrid()
            grid_msg.header = header
            grid_msg.info = map_meta_data
            grid_msg.data = list(np.int8(self.global_grid.flatten() * 100))

            try:
                pub = rospy.Publisher('global_occupancy_grid', OccupancyGrid, queue_size=1)
                pub.publish(grid_msg)
            except rospy.ROSInterruptException as e:
                print(e.getMessage())
                pass

            if self.save_imgs:
                fig = plt.figure(figsize=(15, 5))

                ax = plt.subplot(131)
                ax.set_xticks([])
                ax.set_yticks([])
                ax.imshow(self.local_grid, cmap='Reds')
                ax.set_title('Local Occupancy Grid')

                ax = plt.subplot(132)
                ax.set_xticks([])
                ax.set_yticks([])
                ax.imshow(rotated_grid, cmap='Reds')
                ax.set_title('Rotated Occupancy Grid')

                ax = plt.subplot(133)
                ax.imshow(self.global_grid, cmap='Reds')
                ax.set_title('Global Occupancy Grid')

                fig.savefig('%s/%d' % (self.viz_dir, len(os.listdir(self.viz_dir))))
                plt.close()


if __name__ == '__main__':
    try:
        global_grid = GlobalOccupancyGrid()
        rospy.Subscriber(global_grid.local_grid_topic, OccupancyGrid, global_grid.local_grid_callback, queue_size=1)
        rospy.Subscriber(global_grid.localization_topic, Odometry, global_grid.localization_listener, queue_size=1)
        rospy.spin()
    except rospy.exceptions.ROSInterruptException:
        pass

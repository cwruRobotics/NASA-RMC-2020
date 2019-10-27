#!/usr/bin/env python3
import os
import glob
import math
import rospy
import rospkg
import math
import matplotlib
from matplotlib.patches import Ellipse
import matplotlib.transforms as transforms
matplotlib.use('Agg')  # necessary when plotting without $DISPLAY
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
from filterpy.stats import plot_covariance
from scipy.spatial.transform import Rotation as R

from std_msgs.msg import Header
from geometry_msgs.msg import Quaternion, Vector3
from geometry_msgs.msg import Pose
from nav_msgs.msg import Odometry



class KalmanFilterNode:
    def __init__(self):
        self.topic = 'odometry/filtered_map'
        print('Booting up node...')
        rospy.init_node('kalman_filter_listener', anonymous=True)
        self.robot_x = []
        self.robot_y = []
        self.robot_pitch = []
        self.viz_dir = 'robot_localization_viz'
        self.viz_step = 1

        os.makedirs(self.viz_dir, exist_ok=True)

        try:
            files = glob.glob('%s/*' % self.viz_dir)
            for f in files:
                os.remove(f)
        except Exception as e:
            print(e)

    def confidence_ellipse(self, x, y, cov, ax, n_std=3.0, facecolor='none', **kwargs):
        """
        Create a plot of the covariance confidence ellipse of *x* and *y*.

        Parameters
        ----------
        x, y : array-like, shape (n, )
            Input data.

        ax : matplotlib.axes.Axes
            The axes object to draw the ellipse into.

        n_std : float
            The number of standard deviations to determine the ellipse's radiuses.

        Returns
        -------
        matplotlib.patches.Ellipse

        Other parameters
        ----------------
        kwargs : `~matplotlib.patches.Patch` properties
        """
        
        print(cov)
        pearson = cov[0, 1]/np.sqrt(cov[0, 0] * cov[1, 1])
        # Using a special case to obtain the eigenvalues of this
        # two-dimensionl dataset.
        ell_radius_x = np.sqrt(1 + pearson)
        ell_radius_y = np.sqrt(1 - pearson)
        ellipse = Ellipse((0, 0),
            width=ell_radius_x * 2,
            height=ell_radius_y * 2,
            facecolor=facecolor,
            **kwargs)

        # Calculating the stdandard deviation of x from
        # the squareroot of the variance and multiplying
        # with the given number of standard deviations.
        scale_x = np.sqrt(cov[0, 0]) * n_std
        mean_x = x

        # calculating the stdandard deviation of y ...
        scale_y = np.sqrt(cov[1, 1]) * n_std
        mean_y = y

        transf = transforms.Affine2D() \
            .rotate_deg(45) \
            .scale(scale_x, scale_y) \
            .translate(mean_x, mean_y)

        ellipse.set_transform(transf + ax.transData)
        return ax.add_patch(ellipse)

    def position_callback(self, msg):
        pose = msg.pose.pose
        covariance = msg.pose.covariance
        covariance = np.reshape(covariance, (6, 6))
        self.robot_x.append(pose.position.x)
        self.robot_y.append(pose.position.y)
        quat = pose.orientation
        euler = R.from_quat([quat.x, quat.y, quat.z, quat.w]).as_euler('xyz')
        self.robot_pitch.append(euler[2])  # rotation about vertical z-axis
        print('X: %.4f \tY: %.4f \tpitch: %.4f' % (self.robot_x[-1], self.robot_y[-1], self.robot_pitch[-1]))

        if len(self.robot_x) % self. viz_step == 0:
            fig, ax = plt.subplots()
            ax.scatter(self.robot_x, self.robot_y, label='robot')
            self.confidence_ellipse(self.robot_x[-1], self.robot_y[-1], covariance[0: 2, 0: 2], ax, edgecolor='red')
            ax.arrow(self.robot_x[-1], self.robot_y[-1], .3 * math.cos(self.robot_pitch[-1]), .3 * math.sin(self.robot_pitch[-1]), head_width=0.1)
            ax.legend(loc='best')
            ax.set_xlim(0, 5.2)
            ax.set_ylim(0, 6.05)
            plt.tight_layout()
            fig.savefig(self.viz_dir + '/localization_%d.png' % (len(os.listdir(self.viz_dir))))
            plt.close()



if __name__ == '__main__':
    kalman_filter_node = KalmanFilterNode()
    sub = rospy.Subscriber(kalman_filter_node.topic, Odometry, kalman_filter_node.position_callback)
    rospy.spin()

#!/usr/bin/env python3
import pandas as pd
import matplotlib.pyplot as plt
from localization.localization.geoProject import Project
# from localization import *


class UltraWideBandNode:
    def __init__(self, id, sensors):
        self.id = id
        self.x = None
        self.y = None
        self.sensors = sensors
        self.measurements = {}
        self.x_plot = []
        self.y_plot = []

    def add_measurement(self, anchor_id, distance):
        if distance >= 0:
            self.measurements[anchor_id] = distance

    def plot_position(self, ax):
        ax.scatter(self.x_plot, self.y_plot, label=str(self.id))

    def get_position(self):
        if len(list(self.measurements.keys())) >= 3:
            P = Project(mode='2D', solver='LSE_GC')

            for i, sensor in self.sensors.iterrows():
                if sensor['type'] == 'anchor':
                    P.add_anchor(sensor['id'], (sensor['x'], sensor['y']))

            t,label=P.add_target()

            for sensor in self.measurements.keys():
                t.add_measure(sensor, self.measurements[sensor])

            P.solve()
            # Then the target location is:
            #print(t.loc)
            position = t.loc
            print('id: %d, X: %.2f, Y: %.2f' % (self.id, position.x, position.x))
            self.x_plot.append(position.x)
            self.y_plot.append(position.y)
        else:
            print('Not enough points to triangulate node...')

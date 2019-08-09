#!/usr/bin/env python3
import pandas as pd
from localization.localization.geoProject import Project
# from localization import *


class UltraWideBandNode:
    def __init__(self, id, sensors):
        self.id = id
        self.x = None
        self.y = None
        self.sensors = sensors
        self.measurements = {}

    def add_measurement(self, anchor_id, distance):
        self.measurements[anchor_id] = distance

    def get_position(self):
        if len(list(self.measurements.keys())) >= 2:
            P = Project(mode='2D', solver='LSE')

            for i, sensor in self.sensors.iterrows():
                if sensor['type'] == 'anchor':
                    P.add_anchor(sensor['id'], (sensor['x'], sensor['y']))

            t,label=P.add_target()

            for sensor in self.measurements.keys():
                t.add_measure(sensor, self.measurements[sensor])

            P.solve()
            # Then the target location is:
            print(t.loc)
        else:
            print('Not enough points to triangulate node...')

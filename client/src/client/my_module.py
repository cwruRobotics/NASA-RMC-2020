#!/usr/bin/env python
import os

import rospy
import rospkg
import sys
import rosservice
import robotInterface

from qt_gui.plugin import Plugin
from python_qt_binding import loadUi
from python_qt_binding.QtCore import Qt, QTimer, Slot
from python_qt_binding.QtWidgets import QWidget
from actionlib.msg import TestAction

node_name = 'robot_interface'
motorCommandTopic = 'motor_setpoints'
sensorValueTopic = 'sensor_value'

# --------------------------------------------------
#
# Heavily referenced from rosviz - rqt_robot_steering
# For CWRUbotix
#
# Manual Control Subteam
# ---------------------------------------------------

class MyPlugin(Plugin):
    
    def __init__(self, context):
        super(MyPlugin, self).__init__(context)
       
        # Give QObjects reasonable names
        self.setObjectName('MyPlugin')

        # Process standalone plugin command-line arguments
        from argparse import ArgumentParser
        parser = ArgumentParser()

        # Add argument(s) to the parser.
        parser.add_argument("-q", "--quiet", action="store_true",
                      dest="quiet",
                      help="Put plugin in silent mode")
       
        args, unknowns = parser.parse_known_args(context.argv())
       
        if not args.quiet:
            print('arguments: ', args)
            print('unknowns: ', unknowns)

        # ROS Publisher
        self._publisher = None
        robotInterface.initializeRobotInterface()

        # Service Proxy and Subscriber
        self._service_proxy = None
        self._subscriber = None

        # Create QWidget
        self._widget = QWidget()

        # Get path to UI file which should be in the "resource" folder of this package
        ui_file = os.path.join(rospkg.RosPack().get_path('client'), 'resource', 'MyPlugin.ui')
       
        # Extend the widget with all attributes and children from UI file
        loadUi(ui_file, self._widget)
        
        # Give QObjects reasonable names
        self._widget.setObjectName('MyPluginUi')
        
        ### Map motors to their respective UI elements
        self.motor_widgets = {
            2:self._widget.motor2_spinbox,
            3:self._widget.motor3_spinbox,
            4:self._widget.motor4_spinbox,
            5:self._widget.motor5_spinbox, 
        }

        self.sensor_widgets = {
            0:self._widget.sensor0_lineedit,
            1:self._widget.sensor1_lineedit,
            2:self._widget.sensor2_lineedit,
            3:self._widget.sensor3_lineedit,
            4:self._widget.sensor4_lineedit,
            5:self._widget.sensor5_lineedit,
            6:self._widget.sensor6_lineedit,
            9:self._widget.sensor9_lineedit,
            10:self._widget.sensor10_lineedit,
            13:self._widget.sensor13_lineedit,
            19:self._widget.sensor19_lineedit,
            23:self._widget.sensor23_lineedit,
            24:self._widget.sensor24_lineedit,
            25:self._widget.sensor25_lineedit,
            26:self._widget.sensor26_lineedit,
            27:self._widget.sensor27_lineedit,
            28:self._widget.sensor28_lineedit,
            29:self._widget.sensor29_lineedit,
            30:self._widget.sensor30_lineedit,
            31:self._widget.sensor31_lineedit,
            32:self._widget.sensor32_lineedit
        }

        self.ENABLE_DEBUGGING = False

        #For converting slider values to floats 
        self.general_speed_slider_conversion_factor = 0.1
        self.attitude_slider_conversion_factor = 0.01
       
        '''Set reasonable ranges for the motors'''
        '''deposition bucket speed is from -1 to 1'''
        self._widget.motor2_spinbox.setRange(-1, 1)
        self._widget.motor2_spinbox.setSingleStep(0.1)

        '''excavation speed is from -1 to 1'''
        self._widget.motor3_spinbox.setRange(-1, 1)
        self._widget.motor3_spinbox.setSingleStep(0.1)

        '''excavation depth is from 0 to 0.4'''
        self._widget.motor4_spinbox.setRange(0, 0.4)
        self._widget.motor4_spinbox.setSingleStep(0.05)

        '''excavation angle is from 0 to 1.57'''
        self._widget.motor5_spinbox.setRange(0, 1.57)
        self._widget.motor5_spinbox.setSingleStep(0.02)

        #Configure the slider counterpart
        self._widget.attitude_slider.setRange(0, 157)
        self._widget.attitude_slider.setSingleStep(2)
        
        ''' drive speed is from -0.6 to 0.6
            can also used to control all other motor spinbox values'''
        self._widget.general_speed_spinbox.setRange(0, 0.6)
        self._widget.general_speed_spinbox.setSingleStep(0.1)

        #Configure the slider counterpart 
        self._widget.general_speed_slider.setRange(0, 6) 
        self._widget.general_speed_slider.setSingleStep(1)

        #Configure the goal value slider 
        self._widget.goal_value_slider.setRange(0, 2)
        self._widget.goal_value_slider.setSingleStep(1)

        #All spinbox valueChanged callbacks
        self._widget.motor2_spinbox.valueChanged.connect(self.motor2_spinbox_changed)
        self._widget.motor3_spinbox.valueChanged.connect(self.motor3_spinbox_changed)
        self._widget.motor4_spinbox.valueChanged.connect(self.motor4_spinbox_changed)
        self._widget.motor5_spinbox.valueChanged.connect(self.motor5_spinbox_changed)
        self._widget.general_speed_spinbox.valueChanged.connect(self.general_spinbox_changed)

        #All slider valueChanges callbacks
        self._widget.attitude_slider.valueChanged.connect(self.motor5_slider_changed)
        self._widget.goal_value_slider.valueChanged.connect(self.goal_value_slider_changed)
        self._widget.general_speed_slider.valueChanged.connect(self.general_slider_changed)

        #All button pressed callbacks
        self._widget.emergency_stop_button.pressed.connect(self.estop_pressed)
        self._widget.w_button.pressed.connect(self.w_pressed)
        self._widget.a_button.pressed.connect(self.a_pressed)
        self._widget.s_button.pressed.connect(self.s_pressed)
        self._widget.d_button.pressed.connect(self.d_pressed)
        self._widget.dig_button.pressed.connect(self.dig_pressed)
        self._widget.dump_button.pressed.connect(self.dump_pressed)
        self._widget.update_sensors_button.pressed.connect(self._setup_timer_update_sensors)
        self._widget.stop_sensor_update_button.pressed.connect(self._stop_update_timer)
        self._widget.debug_checkbox.toggled.connect(self.debugging_checkbox_checked)

        # ROS Connection Fields
        """
        TODO: Omitted
        """
        ###

        if context.serial_number() > 1:
            self._widget.setWindowTitle(self._widget.windowTitle() + (' (%d)' % context.serial_number()))
        
        # Add widget to the user interface
        context.add_widget(self._widget)
        self._widget.setFocusPolicy(0x8)
        self._widget.setFocus()

        ### Keyboard teleop setup
        self._widget.keyPressEvent = self.keyPressEvent
        self._widget.keyReleaseEvent = self.keyReleaseEvent

        # timer to consecutively send service messages
        self._update_translate_timer = QTimer(self)
        self._update_attitude_timer = QTimer(self)
        self._update_sensors_timer = QTimer(self)
        
    def debugging_checkbox_checked(self):
        debugging_status = self._widget.debug_checkbox.isChecked()
        self.ENABLE_DEBUGGING = debugging_status

    # Keyboard Teleop with signalling
    """
    Messy, but it works, theoretically.
    """
    def keyPressEvent(self, event):
        if event.key() == Qt.Key_W:
            self.w_pressed()
        elif event.key() == Qt.Key_S:
            self.s_pressed()
        elif event.key() == Qt.Key_A:
            self.a_pressed()
        elif event.key() == Qt.Key_D:
            self.d_pressed()
        # Emergency stop, triggers for all motors. Can include one explicitly defined for locomotion though
        elif event.key() == Qt.Key_E:
            rospy.loginfo("Emergency Stopping")
            self.estop_pressed()
        
        # Deposition keys
        elif event.key() == Qt.Key_U:
            rospy.loginfo("Press U key")

        # Arrow keys to manipulate the general spinbox speed
        elif event.key() == Qt.Key_Up:
            motor_speed = self.get_general_motor_val()
            rospy.loginfo("Key up")
            self._widget.general_speed_spinbox.setValue(motor_speed + 1)

        elif event.key() == Qt.Key_Down:
            motor_speed = self.get_general_motor_val()
            rospy.loginfo("Key down")
            self._widget.general_speed_spinbox.setValue(motor_speed - 1)

    # Currently only geared toward locomotion
    def keyReleaseEvent(self, event):
        if event.key() in (Qt.Key_W, Qt.Key_S, Qt.Key_A, Qt.Key_D):
            if not event.isAutoRepeat():
                rospy.loginfo("Key released")
                robotInterface.sendDriveCommand(0, 0)
        
    def get_general_motor_val(self):
        val = float(self._widget.general_speed_spinbox.value())
        return val

    def w_pressed(self):
        motor_speed = self.get_general_motor_val()
        robotInterface.sendDriveCommand(0, motor_speed)
        if self.ENABLE_DEBUGGING:
            rospy.loginfo("w key pressed")

    def a_pressed(self):
        motor_speed = self.get_general_motor_val()
        robotInterface.sendDriveCommand(3, motor_speed)
        if self.ENABLE_DEBUGGING:
            rospy.loginfo("a key pressed")

    def s_pressed(self):
        motor_speed = self.get_general_motor_val()
        robotInterface.sendDriveCommand(1, motor_speed)
        if self.ENABLE_DEBUGGING:
            rospy.loginfo("s key pressed")

    def d_pressed(self):
        motor_speed = self.get_general_motor_val()
        robotInterface.sendDriveCommand(2, motor_speed)
        if self.ENABLE_DEBUGGING:
            rospy.loginfo("d key pressed")

    def estop_pressed(self):
        rospy.loginfo("ESTOP: Attempting to stop all motors...")
    
        # Set all known motors to value 0
        robotInterface.sendWheelSpeed(0)
        robotInterface.sendExcavationSpeed(0)
        robotInterface.sendDepositionBucketSpeed(0)
        
        # Excavation depth and conveyor angle are set to the most recent position from SensorValues
        translationPos = robotInterface.sensorValueMap.get(4)
        bcAttitudePos = robotInterface.sensorValueMap.get(5)

        rospy.loginfo("ESTOP: Setting translation position: %s and attitude: %s" % (translationPos, bcAttitudePos))
        robotInterface.sendExcavationDepth(translationPos)
        robotInterface.sendExcavationAngle(bcAttitudePos)

        # Stop any updated changes to the translation system
        self._update_translate_timer = QTimer(self)

    def dig_pressed(self):
        val = int(self._widget.goal_value_slider.value())
        robotInterface.sendDigAction(val)

    def dump_pressed(self):
        val = int(self._widget.goal_value_slider.value())
        robotInterface.sendDumpAction(val)

    """
    Unregister ROS publisher
    """
    def _unregister_publisher(self):
        if self._publisher is not None:
            self._publisher.unregister()
            self._publisher = None

        if self._service_proxy is not None:
            # TODO:Doesn't actually shutdown/unregister??
            #self._service_proxy.shutdown('Shutting down service proxy...')
            self._service_proxy = None

        #if robotInterface is not None:
        #    robotInterface.motorCommandPub.unregister()


    """
    Individual Motor Change Functions
    """
    def motor2_spinbox_changed(self):
        val = float(self.motor_widgets.get(2).value())
        robotInterface.sendDepositionBucketSpeed(val)

    def motor3_spinbox_changed(self):
        val = float(self.motor_widgets.get(3).value())
        robotInterface.sendExcavationSpeed(val)

    def motor4_spinbox_changed(self):
        val = float(self.motor_widgets.get(4).value())
        robotInterface.sendExcavationDepth(val)

    def motor5_spinbox_changed(self):
        val = float(self.motor_widgets.get(5).value())
        robotInterface.sendExcavationAngle(val)
        self._widget.attitude_slider.setValue(int(val * 100))

    def motor5_slider_changed(self):
        val = float (self._widget.attitude_slider.value() * self.attitude_slider_conversion_factor)
        self._widget.motor5_spinbox.setValue(val)

    def goal_value_slider_changed(self):
        val = self._widget.goal_value_slider.value()
        self._widget.goal_value_label.setText(str(val))

    """
    Grouped Motor Control Functions
    """
    def general_spinbox_changed(self):
        self._widget.general_speed_slider.setValue(int(self.get_general_motor_val() * 10))
        if self._widget.general_assign_checkbox.isChecked():
            motor_speed = self.get_general_motor_val()
            for motor_id, ui_widget in self.motor_widgets.items():
                ui_widget.setValue(motor_speed)

    def general_slider_changed(self):
        sliderValue = int(self._widget.general_speed_slider.value())
        self._widget.general_speed_spinbox.setValue(sliderValue * self.general_speed_slider_conversion_factor)    

    """
    Sensor value updating 
    """
    def _stop_update_timer(self):
        self._update_sensors_timer = QTimer()

    def _setup_timer_update_sensors(self):
        updatesPerSec = 100
        msUpdate = int(1000/updatesPerSec)
        self._update_sensors_timer = QTimer()
        self._update_sensors_timer.timeout.connect(self.try_update_sensors)
        self._update_sensors_timer.start(msUpdate)

    def try_update_sensors(self):
        try:
            for sensor_id, sensor_widget in self.sensor_widgets.items():
                # Try to get value
                sensorVal = robotInterface.sensorValueMap.get(sensor_id)
                sensor_widget.setText(str(sensorVal))
        except Exception as exc:
            rospy.logwarn("There was an unusual problem updating sensors: %s", exc)

    def shutdown_plugin(self):
        # TODO unregister all publishers here
        #self._update_parameter_timer.stop()
        self._unregister_publisher()

    def save_settings(self, plugin_settings, instance_settings):
        # TODO save intrinsic configuration, usually using:
        # instance_settings.set_value(k, v)
        pass

    def restore_settings(self, plugin_settings, instance_settings):
        # TODO restore intrinsic configuration, usually using:
        # v = instance_settings.value(k)
        pass

    #def trigger_configuration(self):
        # Comment in to signal that the plugin has a way to configure
        # This will enable a setting button (gear icon) in each dock widget title bar
        # Usually used to open a modal configuration dialog

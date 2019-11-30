#ifndef HWCTRL_H_
#define HWCTRL_H_

#include <ros/ros.h>
#include <ros/spinner.h>
#include <ros/callback_queue.h>
#include <std_msgs/String.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Empty.h>
#include <canbus/SetVescCmd.h>
#include <canbus/VescData.h>
#include <hwctrl/SetMotor.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <parse_csv.h>
#include <thread>

const std::string id_hddr 			= "id";
const std::string name_hddr 		= "name";
const std::string category_hddr 	= "category";
const std::string device_type_hddr 	= "device_type";
const std::string interface_hddr 	= "interface";
const std::string device_id_hddr 	= "device_id";
const std::string aux_1_hddr 		= "aux_1";
const std::string aux_2_hddr 		= "aux_2";
const std::string aux_3_hddr 		= "aux_3";
const std::string aux_4_hddr 		= "aux_4";

const std::string category_motor 	= "motor";

const std::string config_file_fname = "conf/hw_config.csv";

const std::string vesc_log_fname 	= "hwctrl/vesc_log.csv";

static std::string vesc_log_path 	= "";

#define DEFAULT_MAX_ACCEL 	30.0
#define DEFAULT_MAX_RPM 	50.0
#define MOTOR_LOOP_PERIOD 	0.005

typedef enum MotorType {
	MOTOR_NONE,
	MOTOR_VESC,
	MOTOR_BRUSHED
}MotorType;

typedef enum DeviceType {
	DEVICE_NONE,
	DEVICE_UWB,
	DEVICE_VESC,
	DEVICE_SABERTOOTH,
	DEVICE_QUAD_ENC
}DeviceType;

typedef enum InterfaceType {
	IF_NONE,
	IF_CANBUS,
	IF_UART
}InterfaceType;

typedef enum ControlType {
	CTRL_NONE,
	CTRL_RPM,
	CTRL_POSITION
}ControlType;

// class to hold info about a motor
class HwMotor{
private:
	char scratch_buf[1024];
public:
	bool online = false;
	int id;
	std::string name;
	int device_id; // could be the CAN id, or something else
	float rpm_coef;
	ControlType ctrl_type = CTRL_NONE;
	DeviceType motor_type = DEVICE_NONE;
	InterfaceType if_type = IF_NONE;
	ros::Time update_t;
	float setpoint = 0.0;
	float last_setpoint = 0.0;
	float accel_setpoint = DEFAULT_MAX_ACCEL;
	float max_rpm 	= DEFAULT_MAX_RPM;
	float max_accel = DEFAULT_MAX_ACCEL;
	std::string if_name;
	std::string to_string();
};

InterfaceType get_if_type(std::string type_str);
DeviceType get_device_type(std::string type_str);

// class to manage the interfaces to motors, be it canbus, uart, etc.
class HwMotorIf{
	std::vector<HwMotor> motors;
	bool limit_sw_states[8]; // we probably won't have a whole 8 limit switches
	int motor_ind = 0;
public:
	ros::ServiceClient vesc_client;
	bool set_motor_callback(hwctrl::SetMotor::Request& request, hwctrl::SetMotor::Response& response);
	void add_motor(HwMotor mtr);
	void get_motors_from_csv(std::string fname);
	std::string list_motors();
	void maintain_next_motor();
	void maintain_motors(); // loop to run as a thread
	int get_num_motors();
	void vesc_data_callback(const canbus::VescData& msg);
private:
	std::vector<HwMotor>::iterator motor_it;
};

void limit_switch_thread(ros::Publisher pub);

void maintain_motors_thread(HwMotorIf* motor_if);

void vesc_data_callback(const canbus::VescData& msg);

#endif

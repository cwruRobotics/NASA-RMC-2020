#ifndef VESC_H_
#define VESC_H_

#include <ros/ros.h>
#include <std_srvs/Empty.h>
#include <std_msgs/String.h>
#include <std_msgs/Float32.h>
#include <hwctrl/CanFrame.h>
#include <hwctrl/VescData.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <datatypes.h>
#include <crc.h>
#include <buffer.h>

typedef struct VescData {
	uint8_t vesc_rx_buf[256]; // should be enough to hold some data
	ros::Time timestamp;
	std::string motor_type;
	int8_t sys_id;
	int8_t can_id;
	float raw_rpm;
	float current;
	float duty_cycle;
	float temp_mos1;
	float temp_mos2;
	float current_motor;
	float current_in;
	float avg_id;
	float avg_iq;
	float duty_now;
	float rpm;
	float v_in;
	float amp_hours;
	float amp_hours_charged;
	float watt_hours;
	float watt_hours_charged;
	int32_t tachometer;
	int32_t tachometer_abs;
	int8_t fault_code;
}VescData;


// class VescCan{
// 	int can_sock, self_can_id;
// public:
// 	VescCan(int s, int id);
// 	bool set_vesc_callback(canbus::SetVescCmd::Request& request, canbus::SetVescCmd::Response& response);
// };

/**
 * takes a vesc communication buffer and fills a vector with valid can frames ready to be sent
 */
void generate_can_frames(int target_id, int self_id, uint8_t* data, int data_len, std::vector<struct can_frame> &tx_frame_queue);

/**
 * send a short buffer to VESC with target ID with specified command and no data
 */
int send_short_buf(int sock, int target_id, int self_id, int command);

/**
 * send a short buffer to VESC with target ID, specified command, and data
 * data is assumed to be 4 bytes
 */
int send_short_buf(int sock, int target_id, int self_id, int command, uint8_t* data);

/**
 * Send an arbitrary VESC comm packet
 */
int send_packet(int sock, int target_id, int self_id, uint8_t* packet, int len);

/**
 * Sets the RPM of VESC with target_id using short buffer command
 */
int set_rpm(int sock, int target_id, int self_id, float rpm);

/**
 * Sets the RPM of VESC with target_id using CAN_PACKET_SET_RPM packet type
 */
int set_rpm_can(int sock, int target_id, int self_id, float rpm);


/**
 * Sends COMM_GET_VALUES command to VESC with target_id
 */
int get_values(int sock, int target_id, int self_id);


void parse_motor_frames(std::vector<struct can_frame> &frames, boost::shared_ptr<hwctrl::VescData>& motor_msg);

// void fill_msg_from_buffer(uint8_t* vesc_rx_buf, boost::shared_ptr<hwctrl::VescData>& motor_msg);
void fill_data_from_buffer(uint8_t* rx_buf, VescData* data);

// void fill_msg_from_status_packet(uint8_t* frame_buf, boost::shared_ptr<hwctrl::VescData>& motor_msg);
void fill_data_from_status_packet(uint8_t* rx_buf, VescData* data);

////////////////////////////////////////////////////////////////////////////////
// NEW HWCTRL CODE 
////////////////////////////////////////////////////////////////////////////////
/**
 * Makes a CanFrame message the will set the RPM of VESC with target ID using CAN_PACKET_SET_RPM packet type
 */
int set_rpm_frame(int target_id, float rpm, const boost::shared_ptr<hwctrl::CanFrame>& frame);

#endif
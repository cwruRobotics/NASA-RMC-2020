#ifndef CANBUS_H_
#define CANBUS_H_

#include <ros/ros.h>
#include <std_msgs/String.h>
#include <std_msgs/Float32.h>
#include <hwctrl/UwbData.h>
#include <hwctrl/VescData.h>
#include <hwctrl/CanFrame.h>
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
#include <queue>
#include <fstream>

#include <vesc.h>

#define MAX_NUM_NODES 4
#define MAX_NUM_ANCHORS 3
#define NUM_ANCHORS MAX_NUM_ANCHORS
#define MAX_CAN_TRIES 3
#define TX_RX_BUFFER_SIZE 256
using namespace std;

using std::string;

const string id_header 				= "id";
const string type_header 			= "type";
const string x_header 				= "x";
const string y_header 				= "y";
const string anchor_str 			= "anchor";
const string node_str 				= "node";
const string node_config_fname 		= "conf/node_config.csv";
const string can_config_fname 		= "conf/hw_config.csv";
const std::string device_if_hddr 	= "interface";
const std::string device_type_hddr 	= "device_type";
const std::string can_id_hddr 		= "device_id";
const std::string device_name_hddr 	= "name";

static ros::Publisher can_tx_pub; 		// so some thread can publish can frames to be transmitted
static ros::Publisher can_rx_pub; 		// so the canbus_if object can publish frames it receives

int nNodes = 0;
int nAnchors = 0;
bool node_done 	= false;
bool uwb_range_next = true;
int anchor_frames = 0;
int uwb_timeout = 0;
ros::Duration uwb_timeout_period(0.25);

typedef canbus::UwbData UWB_msg;
typedef canbus::VescData VescData_msg;

typedef struct {
	int id;
	string type;
	float x;
	float y;
} UwbNode;

typedef struct {
	uint8_t type;
	uint8_t anchor_id;
	float distance;
	uint16_t confidence;
} DistanceFrame;

typedef struct {
	string type;
	uint8_t can_id;
	string name;
	canbus::UwbData* uwb_msg;
	canbus::VescData* vesc_msg;
}CanDevice;

class CanbusIf{
private:
	std::vector<CanDevice> devices;
	// int read_can_config(string fname, string sType);
	// int get_nodes_from_file(string fname);
	bool uwb_range_next = false;
	ros::NodeHandle nh;
	ros::Publisher can_rx_pub;
public:
	CanbusIf(ros::NodeHandle);
	int init(); 					// do all the setup of the canbus
	int sock; 						// our can interface socket
	bool sock_ready = false; 		// flag that says if the socket is opened, etc.
	ros::Rate loop_rate(100); 		// 10ms sleep in each loop
	// void uwb_timeout_cb(const ros::TimerEvent& event);
	int read_can_frames(); 			// put received frames in can_rx_queue, send all frames in can_tx_queue
	void can_tx_cb(const boost::shared_ptr<hwctrl::CanFrame>& frame);
};

void canbus_thread(CanbusIf* canbus_if);

#endif

#ifndef HWCTRL_H_
#define HWCTRL_H_

#include <ros/ros.h>
#include <ros/spinner.h>
#include <ros/callback_queue.h>
#include <ros/param.h>
#include <std_msgs/String.h>
#include <std_msgs/Bool.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Empty.h>
#include <std_msgs/Header.h>
#include <sensor_msgs/Imu.h>
#include <geometry_msgs/Vector3.h>
#include <ros/message_forward.h>
#include <hwctrl/UwbData.h>
#include <hwctrl/VescData.h>
#include <hwctrl/MotorCmd.h>
#include <hwctrl/CanFrame.h>
#include <hwctrl/SensorData.h>
#include <hwctrl/LimitSwState.h>
#include <hwctrl/MotorData.h>
#include <hwctrl/SetMotor.h>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <inttypes.h>
#include <iostream>
#include <fstream>
#include <thread>
#include <iomanip>
#include <boost/math/distributions/normal.hpp>
#include <cmath>
#include <mutex>
using boost::math::normal; // typedef provides default type is double.

#include <canbus.h>
#include <uwb.h>
#include <parse_csv.h>
#include <gpio.h>
#include <spi.h>
#include <ads1120.h>
#include <adt7310.h>
#include <lsm6ds3.h>

#define DEFAULT_MAX_ACCEL       30.0
#define DEFAULT_MAX_RPM         50.0
#define MOTOR_LOOP_PERIOD       0.005

#define NUMBER_OF_SPI_DEVICES   4
#define MAX_NUMBER_OF_SENSORS   32

#define SENSOR_SAMPLES          5

#define ADC_1_IND               0
#define ADC_2_IND               1
#define TEMP_SENSOR_IND         2
#define IMU_IND                 3

const std::string id_hddr       = "id";
const std::string name_hddr     = "name";
const std::string category_hddr   = "category";
const std::string device_type_hddr  = "device_type";
const std::string interface_hddr  = "interface";
const std::string device_id_hddr  = "device_id";
const std::string aux_1_hddr    = "aux_1";
const std::string aux_2_hddr    = "aux_2";
const std::string aux_3_hddr    = "aux_3";
const std::string aux_4_hddr    = "aux_4";

const std::string category_motor  = "motor";
const std::string category_sensor = "sensor";

const std::string config_file_fname = "config/params/hw_config.csv";

const std::string vesc_log_fname  = "hwctrl/vesc_log.csv";

static std::string vesc_log_path  = "";

const std::string spidev_path   = "/dev/spidev1.0";
const std::string sys_gpio_base = "/sys/class/gpio/";
const std::string cal_file_default  = "sensor_calibration.dat"; // this should be created in the HOME directory
const std::string adc_1_cs      = "/sys/class/gpio/gpio67/";    // gpio file for ADC 1 chip select
const std::string adc_2_cs      = "/sys/class/gpio/gpio68/";
const std::string temp_sensor_cs= "/sys/class/gpio/gpio69/";
const std::string imu_cs        = "/sys/class/gpio/gpio66/";
const std::string sys_power_on  = "/sys/class/gpio/gpio45/";    // 24V_PRESENT
const std::string ext_5V_ok     = "/sys/class/gpio/gpio26/";
const std::string limit_1_gpio  = "/sys/class/gpio/gpio60/";
const std::string limit_2_gpio  = "/sys/class/gpio/gpio48/";
const std::string limit_3_gpio  = "/sys/class/gpio/gpio49/";
const std::string limit_4_gpio  = "/sys/class/gpio/gpio117/";
const std::string limit_5_gpio  = "/sys/class/gpio/gpio115/";
const std::string limit_6_gpio  = "/sys/class/gpio/gpio20/";
const std::string over_temp_gpio= "/sys/class/gpio/gpio27/";
const std::string crit_temp_gpio= "/sys/class/gpio/gpio61/";

const std::string param_base = "/hardware";

const std::vector<std::string> sensor_param_names{
  "uwb_node_1",
  "uwb_node_2",
  "uwb_node_3",
  "uwb_node_4",
  "ebay_temperature",
  "imu",
  "adc_1",
  "adc_2",
  "limit_1",
  "limit_2",
  "limit_3",
  "limit_4"
};
const std::vector<std::string> motor_param_names{
  "port_drive",
  "starboard_drive",
  "dep",
  "exc_belt",
  "exc_translation",
  "exc_port_act",
  "exc_starboard_act"
};

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
  DEVICE_QUAD_ENC,
  DEVICE_LIMIT_SW,
  DEVICE_POT,
  DEVICE_LOAD_CELL,
  DEVICE_ADS1120,
  DEVICE_ADT7310,
  DEVICE_LSM6DS3,
  DEVICE_POWER_SENSE,
  DEVICE_BRUSHED_MOTOR
}DeviceType;

typedef enum InterfaceType {
  IF_NONE,
  IF_CANBUS,
  IF_UART,
  IF_SPI,
  IF_GPIO
}InterfaceType;

typedef enum ControlType {
  CTRL_NONE,
  CTRL_RPM,
  CTRL_POSITION
}ControlType;

////////////////////////////////////////////////////////////////////////////////
// MOTOR STUFF
////////////////////////////////////////////////////////////////////////////////

// class to hold info about a motor
class HwMotor{
private:
  char scratch_buf[256];
public:
  bool online = false;
  int id;
  std::string name;
  int device_id; // could be the CAN id, or something else
  VescData vesc_data; // struct to hold VESC data
  float rpm_coef;
  ControlType ctrl_type = CTRL_NONE;
  DeviceType motor_type = DEVICE_NONE;
  InterfaceType if_type = IF_NONE;
  float timeout   = 2.0; // how long between setpoints before we shut it down
  ros::Time data_t;   // when did we last talk to this motor
  ros::Time set_t;    // when was the last set of this motor
  ros::Time update_t; // when was the last time we updated this motor (maintained motor)
  float setpoint = 0.0;
  float last_setpoint = 0.0;
  float accel_setpoint = DEFAULT_MAX_ACCEL;
  float max_rpm   = DEFAULT_MAX_RPM;
  float max_accel = DEFAULT_MAX_ACCEL;
  ros::Timer update_timer; // timer for updating this motor
  bool update = false;
  ros::Duration update_pd;
  std::string if_name;
  std::string to_string();
  void update_cb(const ros::TimerEvent& event);
};

// class to manage the interfaces to motors, be it canbus, uart, etc.
class HwMotorIf{
private:
  uint8_t vesc_rx_buf[1024]; // 
  std::vector<HwMotor>::iterator motor_it;
  ros::NodeHandle nh;
  ros::Publisher can_tx_pub;      // publisher to publish CAN frames to send out
  ros::Publisher motor_data_pub;    // to publish motor data
  ros::Subscriber can_rx_sub;     // to get vesc data frames
  ros::Subscriber sensor_data_sub;  // to get sensor data
  ros::Subscriber limit_sw_sub;     // listen for limit switch interrupts
  ros::ServiceServer set_motor_srv;   // to provide the set_motor service
  ros::Subscriber set_motor_sub;    // set motors
  void init_motors();         // called when power sense turns on
public:
  HwMotorIf(ros::NodeHandle);
  ros::CallbackQueue cb_queue;
  ros::Rate loop_rate;    // 1ms delay in each loop
  std::vector<HwMotor> motors;
  int motor_ind = 0;
  bool sys_power_on = false; // to track current system power state
  bool vesc_update_pending = false;
  bool set_motor_callback(hwctrl::SetMotor::Request& request, hwctrl::SetMotor::Response& response);
  void set_motor_cb_alt(hwctrl::MotorCmd msg);
  void add_motor(HwMotor mtr);
  void get_motors_from_csv();
  void get_motor_configs();
  std::string list_motors();
  void maintain_next_motor();
  int get_num_motors();
  void vesc_data_callback(boost::shared_ptr<hwctrl::VescData> msg);
  void can_rx_callback(boost::shared_ptr<hwctrl::CanFrame> frame);  // to process received can frames
  void sensor_data_callback(hwctrl::SensorData data);
  void limit_sw_callback(boost::shared_ptr<hwctrl::LimitSwState> state);
  HwMotor& get_vesc_from_can_id(int can_id);
};

////////////////////////////////////////////////////////////////////////////////
// STUFF PRETAINING TO SENSORS
////////////////////////////////////////////////////////////////////////////////
// struct to hold info for a spi device
typedef struct SpiDevice {
  std::string name; // a descriptive name of the sensor
  DeviceType device_type  = DEVICE_NONE;
  bool is_setup           = false; // flag to indicate whether everything is setup
  std::string gpio_path;
  int gpio_value_handle; // file handle for controlling the state of the gpio pin
  uint8_t spi_mode        = SPI_MODE_0;
  int spi_max_speed       = SPI_DEFAULT_SPEED;
} SpiDevice;

// sensor calibration data
typedef struct Calibration {
  std::string name;
  float scale;
  float offset;
  float variance;
} Calibration;

// class to hold all info about a sensor
class SensorInfo {
public:
  SensorInfo(void); 
  int sys_id = -1; // system-wide ID to use
  int dev_id = -1;
  uint32_t seq = 0;
  std::string name; // a descriptive name of the sensor
  std::string descrip;
  bool is_setup           = false; // flag to indicate whether everything is setup
  InterfaceType if_type   = IF_NONE;
  DeviceType dev_type     = DEVICE_NONE;
  std::string gpio_path   = "";
  int gpio_value_fd       = -1;
  SpiDevice * spi_device;
  std::vector<Calibration> calibrations; // vector array of calibrations for this sensor
  float value             = 0.0;
  float scale             = 1.0;
  float offset            = 0.0;
  float variance            = 0.0;
  ImuData * imu;
  int sample_ind          = 0;
  ros::Time timestamp;
  bool update = false; // flag to indicate if it's time to update
  std::mutex * update_mtx;
  ros::Duration update_pd; // period with which to update this sensor
  ros::Timer update_timer; // timer for updating this sensor
  void set_update_flag(const ros::TimerEvent&); // callback which will set the update flag
};

// class to handle sensor stuff
class SensorIf{
private:
  ros::NodeHandle nh;
  ros::Subscriber can_rx_sub;   // get data from canbus
  ros::Timer uwb_update_timer;  // when to call the uwb_update_callback
  int uwb_ind = 0;
//  QuadEncoder quad_encoder;     // object for our quadrature encoder
  void get_sensors_from_csv();
  void get_sensor_configs();    // from parameter server
  void setup_spi_devices();
public:
  SensorIf(ros::NodeHandle);
  UwbNode& get_uwb_by_can_id(int can_id);
  SensorInfo& get_sensor_by_name(std::string name);
  ros::CallbackQueue cb_queue;
  ros::Publisher sensor_data_pub; // send data to rest of ROS system
  ros::Publisher can_tx_pub;    // send data to canbus

  ros::Publisher limit_sw1_pub;   // send out limit switch 1 state to /dumper/dep_bottom_limit_switch topic
  ros::Publisher limit_sw2_pub;   // send out limit switch 2 state to /dumper/top_limit_switch topic
  ros::Publisher limit_sw3_pub;   // send out limit switch 3 state to /excavation/exc_lower_limit_switch topic
  ros::Publisher limit_sw4_pub;   // send out limit switch 4 state to /excavation/exc_upper_limit_switch topic

  ros::Publisher uwb_data_pub;  // publish uwb  data

  ros::Publisher ebay_temperature_pub; // publisher ebay temperature, sends to ebay_temperature_pub topic

  ros::Publisher imu_data_pub;  // publisher IMU data, sends to /realsense/imu/data_raw topic

  std::vector<UwbNode> uwb_nodes; // holds all the UWB nodes on the robot
  SensorInfo sensors[MAX_NUMBER_OF_SENSORS]; // all our SensorInfo structs, superceded by sensors_vect
  std::vector<SensorInfo> sensors_vect; // supercedes the sensors array
  SpiDevice spi_devices[NUMBER_OF_SPI_DEVICES]; // all our SpiDevice structs
  ros::Duration uwb_update_period; // how long to wait before we request data from next UWB node
  ros::Rate loop_rate;    // 10ms sleep in every loop
  int spi_handle;               // for the spi interface
  void can_rx_callback      (boost::shared_ptr<hwctrl::CanFrame> frame);      // do things when
  int setup_gpio();
  void uwb_update_callback  (const ros::TimerEvent&);
  void temp_update_cb       (const ros::TimerEvent&);
  void load_cell_update_cb  (const ros::TimerEvent&);
  void imu_update_cb        (const ros::TimerEvent&);
  int n_sensors = 0; // deprecated
  void shutdown(void);
};

////////////////////////////////////////////////////////////////////////////////
// HELPER FUNCTION PROTOTYPES
////////////////////////////////////////////////////////////////////////////////
InterfaceType get_if_type(std::string type_str);
DeviceType get_device_type(std::string type_str);
float get_running_mean(float* data, int size);
bool file_exists(const char * path);
unsigned long long int file_size(const char * path);
void read_cal(std::string path, std::vector<Calibration>& cals);
void write_cal(std::string path, std::vector<Calibration>& cals);
Calibration& get_cal_by_name(std::string name, std::vector<Calibration>& cals);
std::string print_cal(Calibration& cal);
void smooth_data(float * raw_data, float * smooth_data, int size, int a);
void decimate(float * in, float * out, int in_size, int decimation_factor);
int fmax(float * data, int size);
float favg(float * data, int size);
float fstddev(float * data, int size);

void maintain_motors_thread(HwMotorIf* motor_if);
void sensors_thread(SensorIf* sensor_if);

#endif

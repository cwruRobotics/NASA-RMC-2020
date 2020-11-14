#include "threads/sensor_thread.h"

#include "hwctrl.h"

#include <ros/param.h>
#include <ros/spinner.h>

#include <string>

#include <boost/filesystem.hpp>


SensorThread::SensorThread(ros::NodeHandle nh) : m_nh(nh), m_loop_rate(1000) {
    m_sensors  .reserve(MAX_NUMBER_OF_SENSORS);
    m_uwb_nodes.reserve(4);

    m_nh.setCallbackQueue(&m_cb_queue);

    // read calibrations
    std::istringstream path_stream(std::string(std::getenv("HOME")));
	std::string cal_file_path;
	std::getline(path_stream, cal_file_path, ':'); // get the first path
	if(*(cal_file_path.end()) != '/'){
		cal_file_path.push_back('/');
	}
	cal_file_path.append(paths::cal_file_default.c_str());

	if(boost::filesystem::exists(cal_file_path.c_str())){
		ROS_INFO("Loading sensor calibrations from %s...", cal_file_path.c_str());
		std::vector<Calibration> cals;
		read_calibration(cal_file_path, cals);

		for(auto cal : cals){
			std::vector<Sensor>::iterator sensor = m_sensors.begin();
			int pos = std::string::npos;
			// if sensor name exists within calibration name
			while( (pos = cal.name.find(sensor->get_name())) == std::string::npos && sensor != m_sensors.end()){
				sensor++;
			}
			if(pos != std::string::npos){
				sensor->add_calibration(cal);
			}
		}
	}

    // initalize spi
    // needs to be available to all sensors since all sensors are on same spi line
    ROS_DEBUG("Initializing SPI bus from %s...", paths::spidev_path.c_str());
    auto spi = boost::make_shared<Spi>(paths::spidev_path);
    if(spi->has_error()) {
        ROS_ERROR("SPI init failed :(");
    } else {
        ROS_INFO("SPI bus initialization on %s presumed successful", paths::spidev_path.c_str());
    }

    configure_from_server(spi);
}

void SensorThread::configure_from_server(boost::shared_ptr<Spi> spi) {
    const std::string base = param_base + "/sensor";
    for(auto name = sensor_param_names.begin(); name != sensor_param_names.end(); ++name) {
        const std::string full_name = base + "/" + *name;

        const std::string name_param      = full_name + "/name";
		const std::string type_param      = full_name + "/type";
		const std::string can_id_param    = full_name + "/can_id";
		const std::string period_param    = full_name + "/update_period";
		const std::string gpio_param      = full_name + "/gpio";

        // Sensor sensor;
        std::string name_str;
        SensorType type;
        ros::Duration period;
        int can_id;
        boost::shared_ptr<Gpio> gpio;


        if(m_nh.hasParam(name_param)) {
            m_nh.getParam(name_param, name_str);
        }
        if(m_nh.hasParam(type_param)) {
            std::string type_str;
            m_nh.getParam(type_param, type_str);
            type = get_sensor_type_from_param(type_str);
        }
        if(m_nh.hasParam(period_param)){
            double temp;
			m_nh.getParam(period_param, temp);
			ROS_INFO(" - Found sensor update period: %.3fs", temp);
			period = ros::Duration(temp);
		}
        if(m_nh.hasParam(can_id_param)){
			m_nh.getParam(can_id_param, can_id);
			ROS_INFO(" - Found CAN id: %d", can_id);
		}
		if(m_nh.hasParam(gpio_param)){
            std::string gpio_path;
			m_nh.getParam(gpio_param, gpio_path);
			ROS_INFO(" - Found gpio file path: %s", gpio_path.c_str());
            *gpio = Gpio(gpio_path);
		}

        auto sensor = create_sensor_from_values(m_nh, name_str, type, can_id, period, gpio);
        m_sensors.push_back(*sensor); 
    }
}

boost::shared_ptr<Sensor> SensorThread::create_sensor_from_values(
    ros::NodeHandle nh, std::string name, SensorType type, uint32_t can_id, ros::Duration period, boost::shared_ptr<Gpio> gpio
)
{
    static uint8_t sys_id_idx = 0;
    boost::shared_ptr<Sensor> sensor;
    switch(type) {
        case SensorType::UWB: {
            auto node = boost::make_shared<UwbNode>(nh, name, type, sys_id_idx++, "localization_data", 1024, period, can_id);
            m_uwb_nodes.push_back(*node);
            sensor = node;
            break;
        }
        case SensorType::QUAD_ENC: {
            std::string topic("sensor_data");
            auto q = boost::make_shared<QuadEncoder>(nh, name, type, sys_id_idx++, topic, 128, period, can_id);
            sensor = q;
            break;
        }
        case SensorType::LIMIT_SW: {
            static uint8_t idx = 0;
            std::string topic("limit_sw" + std::to_string(++idx) + "_state");
            // check the last two arguments, cant find where theyre defined in hwctrl
            auto sw = boost::make_shared<LimitSwitch>(nh, name, type, sys_id_idx++, topic, 128, period, *gpio, 0, 0);
            sensor = sw;
            break;
        }
        case SensorType::POWER_SENSE: {
            std::string topic("sensor_data");
            auto ps = boost::make_shared<GenericGpioSensor>(nh, name, type, sys_id_idx++, topic, 128, period, *gpio);
            sensor = ps;
            break;
        }
        default:
            break;
    };
    return sensor;
}

void SensorThread::sleep() {
    m_loop_rate.sleep();
}

void SensorThread::shutdown() {
    // something maybe?
}

void SensorThread::setup_sensors() {
    for(Sensor sensor : m_sensors) {
        sensor.setup();
    }
}

void SensorThread::update_sensors() {
    for(Sensor sensor : m_sensors) {
        if(sensor.ready_to_update()) sensor.update();
    }
}


// run thread
void SensorThread::operator()() {
    ROS_INFO("Starting sensor_thread");
    ros::AsyncSpinner spinner(1, &m_cb_queue);

    setup_sensors();
    
    while(ros::ok()) {
        update_sensors();
        sleep();   
    }
    shutdown();
}
#include <hwctrl.h>
////////////////////////////////////////////////////////////////////////////////
// MOTOR INTERFACE STUFF
////////////////////////////////////////////////////////////////////////////////
HwMotorIf::HwMotorIf(ros::NodeHandle n)
 : loop_rate(1000) {
	this->nh = n; 	// store a copy of the node handle passed by value

	this->nh.setCallbackQueue(&(this->cb_queue)); // have this copy use this callback queue

	// this->get_motors_from_csv(); //config_file_fname
	this->get_motor_configs(); // read from parameter server

	for(auto motor : this->motors){
		if( motor.update_pd.toSec() > 0.0){
			motor.update_timer = this->nh.createTimer(motor.update_pd, &HwMotor::update_cb, &motor);
		}
	}

	// PUBLISHERS
	this->can_tx_pub 		= this->nh.advertise<hwctrl::CanFrame>("can_frames_tx", 128);
	this->motor_data_pub 	= this->nh.advertise<hwctrl::MotorData>("motor_data", 128);

	// SUBSCRIBERS
	this->can_rx_sub 		= this->nh.subscribe("can_frames_rx", 128, &HwMotorIf::can_rx_callback, this);
	this->sensor_data_sub 	= this->nh.subscribe("sensor_data", 128, &HwMotorIf::sensor_data_callback, this);
	this->limit_sw_sub 		= this->nh.subscribe("limit_switch_states", 128, &HwMotorIf::limit_sw_callback, this);

	// SET MOTOR SERVICE
	//this->set_motor_srv 	= this->nh.advertiseService("set_motor", &HwMotorIf::set_motor_callback, this);
	this->set_motor_sub 	= this->nh.subscribe("motor_setpoints", 128, &HwMotorIf::set_motor_cb_alt, this);
}

void HwMotorIf::init_motors(){
  // do motor initialization things
}

// ===== SUBSCRIBER CALLBACKS =====
/**
 * any CAN data of interest is from VESC's
 */
void HwMotorIf::can_rx_callback(boost::shared_ptr<hwctrl::CanFrame> frame){
	uint32_t rx_id = (uint32_t)frame->can_id;
	int8_t id 	   = (int8_t)(rx_id & 0xFF); // extract only the id

	if( (rx_id & ~0xFF) != 0 ){
		// this message could have been from a VESC and of interest to us
		// the only CAN cmd type that is 0 is setting duty cycle

		uint8_t cmd = (uint8_t)(rx_id >> 8);
		uint8_t* frame_data = frame->data.data(); // get the back-buffer of the vector
		
		switch(cmd){
			case CAN_PACKET_STATUS:{
				HwMotor& vesc  = this->get_vesc_from_can_id(id); // id equals the VESC's CAN ID
				if(vesc.device_id != id){
					// we don't have a matching VESC in our memory
					break;
				}
				vesc.online = true;
				VescData* vesc_data = &(vesc.vesc_data);

				// store a ROS timestamp
				vesc_data->timestamp = ros::Time::now();

				// store data with the correct vesc object
				fill_data_from_status_packet(frame_data, vesc_data);

				// publish rpm data
				hwctrl::MotorData msg;
				msg.data_type = msg.RPM;
				msg.id = vesc.id;
				msg.value = vesc.vesc_data.rpm / vesc.rpm_coef;
				msg.timestamp = vesc.vesc_data.timestamp;
				// ROS_INFO("Publish to motor_data");
				this->motor_data_pub.publish(msg);

				break;
			}
			case CAN_PACKET_FILL_RX_BUFFER:{
				if(id != 0x00){
					// this is not intended for us (our canID is 0)
					break;
				}
				// copy the CAN data into the corresponding vesc rx buffer
				memcpy(this->vesc_rx_buf + frame_data[0], frame_data + 1, frame->can_dlc - 1);
				break;
			}
			case CAN_PACKET_PROCESS_RX_BUFFER:{
				if(id != 0x00){
					// not intended for us
					break;
				}
				int ind = 0;
				uint16_t packet_len;
				uint16_t crc;
				int vesc_id = frame_data[ind++]; // which vesc sent the data

				HwMotor& vesc  = this->get_vesc_from_can_id(vesc_id); // 
				if(vesc.device_id != vesc_id){
					// we don't have a matching VESC in our memory. No need to continue.
					break;
				}

				int n_cmds 	= frame_data[ind++]; // how many commands
				packet_len 	= (frame_data[ind++] << 8);
				packet_len 	|= frame_data[ind++];
				crc 		= (frame_data[ind++] << 8);
				crc 		|= frame_data[ind++];

				uint16_t chk_crc = crc16(this->vesc_rx_buf, packet_len);
				// ROS_INFO("PROCESSING RX BUFFER\nPacket Len:\t%d\nRcvd CRC:\t%x\nComputed CRC:\t%x", packet_len, crc, chk_crc);

				if(crc != chk_crc){
					ROS_WARN("Error: VESC checksum doesn't match");
					// error in transmission
					break;
				}
				
				vesc.online = true;

				ind = 0;
				int comm_cmd = this->vesc_rx_buf[ind++];
				switch(comm_cmd){
					case COMM_GET_VALUES:{
						vesc.vesc_data.timestamp 	= ros::Time::now();
						vesc.vesc_data.motor_type 	= "vesc";
						vesc.vesc_data.can_id 		= vesc_id;

						fill_data_from_buffer(this->vesc_rx_buf, &(vesc.vesc_data));
						// publish rpm data
						hwctrl::MotorData msg;
						msg.data_type = msg.RPM;
						msg.id = vesc.id;
						msg.value = vesc.vesc_data.rpm / vesc.rpm_coef;
						msg.timestamp = vesc.vesc_data.timestamp;
						// ROS_INFO("Publish to motor_data");
						this->motor_data_pub.publish(msg);
						this->vesc_update_pending = false; // assuming only one VESC was requested from
						break;
					}
				}

				break;
			}
				// indicates the end of the cmd buffer
		}
	}else{
		// ROS_INFO("Not a vesc message");
	}
}

/**
 * callback that handles data from sensors
 */
void HwMotorIf::sensor_data_callback(hwctrl::SensorData data){
	// determine if this sensor is useful to us;
  if(data.name.compare("SYSTEM 24V STATE") == 0){
    if(data.value == 1.0){
      if(!this->sys_power_on){
        this->init_motors(); // sys power has just come on, so initialize motors
      }
      this->sys_power_on = true;
    }else{
      this->sys_power_on = false;
    }
  }
}

void HwMotorIf::limit_sw_callback(boost::shared_ptr<hwctrl::LimitSwState> state){
	// figure out which motor this corresponds to and stop it!
}

/**
 * @param can_id the can id of the VESC we're looking for
 * @return a pointer to the VESC with this CAN ID (null if not found)
 */
HwMotor& HwMotorIf::get_vesc_from_can_id(int can_id){
	int size = this->motors.size();
	int i = 0;
	bool found = false;
	while(i < size && !found){
		found = (this->motors[i].motor_type == DEVICE_VESC) && (this->motors[i].device_id == can_id);
		i++;
	}
	if(found){
		return this->motors.at(i-1);
	}else{
		return this->motors.at(size-1);
	}
	
}

/**
 * intended to be passed as an arugment to a std::thread instance
 */
void maintain_motors_thread(HwMotorIf* motor_if){
	ROS_INFO("Starting maintain_motors_thread");
	ros::AsyncSpinner spinner(1, &(motor_if->cb_queue));
	spinner.start();
	while(ros::ok()){
		motor_if->maintain_next_motor();
		motor_if->loop_rate.sleep();
	}
}

void HwMotorIf::maintain_next_motor(){
	if(!this->sys_power_on){
		return; // nothing to do until power is on
	}
	HwMotor* motors = this->motors.data();
	HwMotor* motor = &(motors[this->motor_ind]);

	if((ros::Time::now().toSec() - motor->data_t.toSec()) > motor->timeout){
		motor->online = false; // assume we are offline
	}

	switch(motor->motor_type){
		case(DEVICE_NONE):break;
		case(DEVICE_VESC):{
			// ROS_INFO("Index: %d, Setpoint: %f", this->motor_ind, motor->setpoint);
			if(motor->online){

				// figure out next velocity setpoint based on acceleration & last sent RPM
				float delta 	=  motor->setpoint - motor->last_setpoint;
				float dt 		= (float)(ros::Time::now().toSec() - motor->update_t.toSec());
				float accel 	= fabs(delta/dt); // proposed acceleration

				// if proposed acceleration is higher than requested accleration, accelerate only at requested acceleration.
				// else the delta we calculated is ok
				if(accel > fabs(motor->accel_setpoint)){
					if(delta > 0){
						delta = fabs(motor->accel_setpoint) * dt; // new delta (is positive)
					}else{
						delta = -1.0 * fabs(motor->accel_setpoint) * dt; // new delta (needs to be negative)
					}
				}
				motor->last_setpoint = motor->last_setpoint + delta; // the new setpoint based on delta

				if((ros::Time::now().toSec() - motor->set_t.toSec()) > motor->timeout){
				motor->last_setpoint = 0.0; // shut it down
				}

				float eRPM = motor->rpm_coef * motor->last_setpoint;
				boost::shared_ptr<hwctrl::CanFrame> frame_msg(new hwctrl::CanFrame());

				if(set_rpm_frame(motor->device_id, eRPM, frame_msg) == 0){ // device_id should be the can_id
					// presumed success
					// ROS_INFO("ID, eRPM: %d, %f",motor->device_id, eRPM);
					this->can_tx_pub.publish(frame_msg);
					motor->update_t = ros::Time::now();
				}else{
					ROS_WARN("I didn't think this could happen ...");
					motor->online = false;
				}
				if(motor->update && !this->vesc_update_pending){
					// send a COMM_GET_VALUES command to this VESC
					boost::shared_ptr<hwctrl::CanFrame> can_msg(new hwctrl::CanFrame()); // empty can frame msg built as a shared pointer
					can_msg->can_id = motor->device_id | (CAN_PACKET_PROCESS_SHORT_BUFFER << 8) | CAN_EFF_FLAG;
					int ind = 0;
					can_msg->data[ind++] = 0; // our id
					can_msg->data[ind++] = 0x00;
					can_msg->data[ind++] = COMM_GET_VALUES;
					can_msg->can_dlc = ind;
					this->can_tx_pub.publish(can_msg);
					motor->update = false;
					this->vesc_update_pending = true; // will get de-asserted by the can_rx_callback when we receive a full packet
					break;	
				}
			}else{
				ROS_WARN("%s (Motor %d) offline", motor->name.c_str(), motor->id);
			}

			break;}
		case(DEVICE_SABERTOOTH):{

			break;}
	}

	this->motor_ind ++;
	if(this->motor_ind >= this->motors.size()){
		this->motor_ind = 0;
	}
}

/**
 * set motors based on messages from a topic
 */
void HwMotorIf::set_motor_cb_alt(hwctrl::SetMotorMsg msg){
	int id = msg.id;
	if(id >= this->motors.size()){
		return;
	}
	ROS_DEBUG("Setting motor %d to %f at %f", id, msg.setpoint, msg.acceleration);
	HwMotor* motors = this->motors.data(); // pointer to our motor struct
	motors[id].setpoint = msg.setpoint;
	motors[id].set_t = ros::Time::now(); // used to guess if autonomy node has crashed
	if(fabs(msg.acceleration) > motors[id].max_accel || fabs(msg.acceleration) == 0.0){
		motors[id].accel_setpoint = motors[id].max_accel;
	}else {
		motors[id].accel_setpoint = msg.acceleration;
	}
}

/**
 * set motors via a service request, we might not be using this
 */
bool HwMotorIf::set_motor_callback(hwctrl::SetMotor::Request& request, hwctrl::SetMotor::Response& response){
	if(request.id >= this->motors.size()){
		return false;
	}
	int id = request.id;
	ROS_DEBUG("Setting motor %d to %f at %f", id, request.setpoint, request.acceleration);
	HwMotor* motors = this->motors.data(); // pointer to our motor struct

	motors[id].setpoint = request.setpoint;
	if(fabs(request.acceleration) > motors[id].max_accel || fabs(request.acceleration) == 0.0){
		motors[id].accel_setpoint = motors[id].max_accel;
	}else {
		motors[id].accel_setpoint = request.acceleration;
	}
	response.actual_accel = motors[id].accel_setpoint;
	response.status = 0; // need to change later to be meaningful

	return true;
}

void HwMotorIf::add_motor(HwMotor mtr){
	this->motors.push_back(mtr);
}

void HwMotorIf::get_motor_configs(){
	ROS_INFO("Reading motor configs from param server...");
	std::string base = param_base + "/motor";
	for(auto name = motor_param_names.begin(); name != motor_param_names.end(); ++name){
		std::string full_name = base + "/" + *name;
		ROS_INFO("Checking for parameters under %s/...", full_name.c_str());
		std::string name_param = full_name + "/name";
		std::string type_param = full_name + "/type";
		std::string can_id_param = full_name + "/can_id";
		std::string interface_param = full_name + "/interface";
		std::string gear_reduc_param = full_name + "/gear_reduction";
		std::string max_rpm_param = full_name + "/max_rpm";
		std::string max_accel_param = full_name + "/max_acceleration";
		std::string period_param = full_name + "/update_period";

		HwMotor motor;
		int found = 0;
		if(this->nh.hasParam(name_param)){
			this->nh.getParam(name_param, motor.name);
			ROS_INFO(" - Found motor name: %s", motor.name.c_str());
			found++;
		}
		if(this->nh.hasParam(type_param)){
			std::string type_str;
			this->nh.getParam(type_param, type_str);
			ROS_INFO(" - Found motor type: %s", type_str.c_str());
			motor.motor_type = get_device_type(type_str);
			found++;
		}
		if(this->nh.hasParam(interface_param)){
			std::string if_str;
			this->nh.getParam(interface_param, if_str);
			ROS_INFO(" - Found motor interface: %s", if_str.c_str());
			motor.if_type = get_if_type(if_str);
			found++;
		}
		if(this->nh.hasParam(can_id_param)){
			this->nh.getParam(can_id_param, motor.device_id);
			ROS_INFO(" - Found CAN id: %d", motor.device_id);
			found++;
		}
		if(this->nh.hasParam(gear_reduc_param)){
			double gearing;
			this->nh.getParam(gear_reduc_param, gearing);
			motor.rpm_coef = (float)gearing;
			ROS_INFO(" - Found gear reduction: %.2f", motor.rpm_coef);
			found++;
		}
		if(this->nh.hasParam(max_rpm_param)){
			double max_rpm;
			this->nh.getParam(max_rpm_param, max_rpm);
			motor.max_rpm = (float)max_rpm;
			ROS_INFO(" - Found max RPM: %.2f", motor.max_rpm);
			found++;
		}
		if(this->nh.hasParam(max_accel_param)){
			double max_acc;
			this->nh.getParam(max_accel_param, max_acc);
			motor.max_accel = (float)max_acc;
			ROS_INFO(" - Found max acceleration: %.2f", motor.max_accel);
			found++;
		}
		if(this->nh.hasParam(period_param)){
			double update_pd;
			this->nh.getParam(period_param, update_pd);
			motor.update_pd = ros::Duration(update_pd);
			ROS_INFO(" - Found motor update period: %.3fs", update_pd);
		}

		switch(motor.motor_type){
			case DEVICE_VESC:{
				motor.ctrl_type = CTRL_RPM;
			}
			case DEVICE_BRUSHED_MOTOR:{
				motor.ctrl_type = CTRL_POSITION;
			}
			case DEVICE_SABERTOOTH:{
				motor.ctrl_type = CTRL_POSITION;
			}
		}
		if(found > 0){
			this->motors.push_back(motor);
		}
	}
}

void HwMotorIf::get_motors_from_csv(){
	std::string ros_package_path(std::getenv("ROS_PACKAGE_PATH"));
	std::istringstream path_stream(ros_package_path);

	std::string src_dir_path;
	std::getline(path_stream, src_dir_path, ':'); // get the first path
	if(*(src_dir_path.end()) != '/'){
		src_dir_path.push_back('/');
	}
	std::string config_file_path = src_dir_path.append(config_file_fname);

	std::vector<std::vector<std::string>> csv = read_csv(config_file_path);
	int line_num = 0;
	int id_ind, name_ind, category_ind, device_type_ind, interface_ind, device_id_ind, aux_1_ind, aux_2_ind, aux_3_ind, aux_4_ind;
	for(auto line = csv.begin(); line != csv.end(); ++line){
		if(line_num == 0){
			for(int i = 0; i < (*line).size(); i++){
				if((*line)[i].compare(id_hddr)==0){
					id_ind = i;
				}else if((*line)[i].compare(name_hddr)==0){
					name_ind = i;
				}else if((*line)[i].compare(category_hddr)==0){
					category_ind = i;
				}else if((*line)[i].compare(device_type_hddr)==0){
					device_type_ind = i;
				}else if((*line)[i].compare(interface_hddr)==0){
					interface_ind = i;
				}else if((*line)[i].compare(device_id_hddr)==0){
					device_id_ind = i;
				}else if((*line)[i].compare(aux_1_hddr)==0){
					aux_1_ind = i;
				}else if((*line)[i].compare(aux_2_hddr)==0){
					aux_2_ind = i;
				}else if((*line)[i].compare(aux_3_hddr)==0){
					aux_3_ind = i;
				}else if((*line)[i].compare(aux_4_hddr)==0){
					aux_4_ind = i;
				}else{
					// do nothing
				}
			}
		}else{
			if((*line)[category_ind].compare(category_motor) == 0){
				// make a motor struct and populate with data
				HwMotor motor;
				motor.id 			= std::stoi((*line)[id_ind]);
				motor.name 			= (*line)[name_ind];
				motor.device_id 	= std::stoi((*line)[device_id_ind]);
				motor.motor_type 	= get_device_type((*line)[device_type_ind]);
				motor.if_type 		= get_if_type((*line)[interface_ind]);
				motor.rpm_coef 		= std::stof((*line)[aux_2_ind]); // this will be our gear reduction
				motor.max_rpm 		= std::stof((*line)[aux_3_ind]); // our max rpm
				motor.max_accel		= std::stof((*line)[aux_4_ind]); // our max acceleration

				if(motor.motor_type == DEVICE_VESC){
					motor.ctrl_type = CTRL_RPM;
				}else{
					motor.ctrl_type = CTRL_POSITION;
				}
				this->motors.push_back(motor);
			}

		}
		line_num++;
	}
	this->motor_it = this->motors.begin();
}

int HwMotorIf::get_num_motors(){
	return this->motors.size();
}

std::string HwMotorIf::list_motors(){
	std::string retval;
	for(auto mtr = this->motors.begin(); mtr != this->motors.end(); ++mtr){
		retval.append((*mtr).to_string());
	}
	return retval;
}

////////////////////////////////////////////////////////////////////////////////
//
// 		HwMotor things
//
////////////////////////////////////////////////////////////////////////////////
std::string HwMotor::to_string(){
	snprintf(this->scratch_buf, sizeof(this->scratch_buf), "\n==========\nMotor\t: %s\n\rID\t: %d\n",
		this->name.c_str(), 
		this->id);
	return std::string(this->scratch_buf);
}

/**
 * called by a timer to update relevant parameters of the motor
 */
void HwMotor::update_cb(const ros::TimerEvent& event){
	this->update = true;
}
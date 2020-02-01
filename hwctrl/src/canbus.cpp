#include <canbus.h>

void canbus_thread(CanbusIf* canbus_if){
	while(ros::ok()){
		int frames_sent = canbus_if->read_can_frames();
		canbus_if->loop_rate.sleep(); //
	}
}

/**
 * Constructor for the CanbusIf object
 * initializes the can socket, creates publisher and subscriber for CanFrame messages
 */
CanbusIf::CanbusIf(ros::NodeHandle n){
	this->nh = n;

	int canbus_init = this->init(); // do setup things

	if(canbus_init != 0){
		ROS_INFO("Canbus init failed: Error code %d", canbus_init);
	}else{
		ROS_INFO("Canbus init success");
	}

	// PUBLISHERS
	this->can_rx_pub 	= this->nh.advertise<hwctrl::CanFrame>("can_frames_rx", 128);

	// SUBSCRIBERS
	this->can_tx_sub 	= this->nh.subscribe("can_frames_tx", 128, &CanbusIf::can_tx_cb, this);
}

int CanbusIf::init(){
	struct sockaddr_can addr;
	struct can_frame frame;
	struct can_frame rx_frame;
	struct can_frame tx_frame;
	struct ifreq ifr;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 10000; // 10ms CAN bus read timeout

	const char* ifname = "can1";

	// initialize the CAN socket
	if((this->sock = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0){
		ROS_INFO("Error while opening CAN socket");
		this->sock_ready = false;
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(this->sock, SIOCGIFINDEX, &ifr);

	addr.can_family = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	ROS_INFO("%s at index %d", ifname, ifr.ifr_ifindex);

	setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

	// socket bind
	if(bind(this->sock, (struct sockaddr *)&addr, sizeof(addr)) < 0){
		ROS_INFO("Error in CAN socket bind");
		this->sock_ready = false;
		return -2;
	}
	this->sock_ready = true;
	return 0; // success
}

int CanbusIf::read_can_frames(){
	if(!this->sock_ready){
		return -1;
	}
	int retval = 0;
	int n_bytes = 0;
	struct can_frame rx_frame;
	// BRING RECEIVED FRAMES INTO USER SPACE
	while((nbytes = read(canbus_if->sock, &rx_frame, sizeof(struct can_frame))) > 0){
		boost::shared_ptr<hwctrl::CanFrame> frame_msg(new hwctrl::CanFrame());
		frame_msg->can_id = rx_frame.can_id;
		frame_msg->can_dlc = rx_frame.can_dlc;
		int i = 0;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];
		i++;
		frame_msg->data[i] = rx_frame[i];

		can_rx_pub.publish(frame_msg); 	// publish message immediately
		retval++;
	}
	return retval;
}

void CanbusIf::can_tx_cb(const boost::shared_ptr<hwctrl::CanFrame>& frame){
	struct can_frame f;
	f.can_id = frame->can_id;
	f.can_dlc = frame->can_dlc;
	i = 0;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];
	i++;
	f.data[i] = frame->data[i];

	// WRITE CAN FRAME TO SOCKET IMMEDIATELY
	if(this->sock_ready){
		int n_bytes = write(this->sock, &f, sizeof(f));
	}
}

/*
int CanbusIf::get_nodes_from_file(string fname, string sType){
	return 0;

	ifstream node_data(fname.c_str());

	string line;

	int retval;

	if(node_data.is_open()){

		while(getline(node_data, line, '\n')){
			char* type = '\0';//strstr(line.c_str(), sType.c_str());
			if(type != NULL){
				retval++;
			}
		}
	}

	node_data.close();

	return retval;
}

int CanbusIf::read_can_config(std::string fname){
	std::string ros_package_path(std::getenv("ROS_PACKAGE_PATH"));
	std::istringstream path_stream(ros_package_path);

	std::string src_dir_path;
	std::getline(path_stream, src_dir_path, ':'); // get the first path
	if(*(src_dir_path.end()) != '/'){
		src_dir_path.push_back('/');
	}
	fname = src_dir_path.append(fname);
	int retval = 0;
	std::ifstream config_file;
	config_file.open(fname.c_str(), ios::in);
	std::string line;
	std::string word;

	std::vector<std::string> words;

	int line_num = 0;
	int can_id_ind 		= -1;
	int device_if_ind 	= -1;
	int device_type_ind = -1;
	int device_name_ind = -1;
	if(config_file.is_open()){
		while(std::getline(config_file, line)){
			line.push_back(','); // make sure we can read the last word
			std::istringstream line_stream(line);
			while(std::getline(line_stream, word, ',')){
				words.push_back(word);
			}
			if(line_num == 0){
				// figure out which ID corresponds to which column
				for(int i = 0; i < words.size(); i++){
					if(can_id_ind < 0 && words[i].compare(can_id_hddr) == 0){
						can_id_ind = i;
					}else if(device_type_ind < 0 && words[i].compare(device_type_hddr) == 0){
						device_type_ind = i;
					}else if(device_if_ind < 0 && words[i].compare(device_if_hddr) == 0){
						device_if_ind = i;
					}else if(device_name_ind < 0 && words[i].compare(device_name_hddr) == 0){
						device_name_ind = i;
					}
				}
			}else{
				if(can_id_ind < 0 || device_type_ind < 0){
					return 2;
				}
				if(words[device_if_ind].compare("can") == 0){
					CanDevice device;
					device.can_id 	= std::stoi(words[can_id_ind]);
					device.type 	= words[device_type_ind];
					this->devices.push_back(device);
				}
			}
			line_num++;

			words.clear();
		}
	}else{
		ROS_INFO("File couldn't be opened :(");
		return 1;
	}

	config_file.close();

	return 0;
}
*/

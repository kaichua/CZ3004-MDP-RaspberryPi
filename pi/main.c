/*Version 5.6
TESTED (WORKING):
Removed addMarker function.
Added writeHub function to centralized all the write2 functions.
Queue able to write all information to respective devices.
Able to split instructions into the respective numbers.
Added check for length of command for serial
Added checkPhotoDir()
Modified activate_camera() 
updated writehub logic with r receipient and to return "rTAKEN" when picture has been taken

Changes to from 5.5
-> Modified the thread to include image taking

Issues:
-> To find another way to take images from picamera
--->Unable to handle asynchronise taking of images
--->Image taking takes way too long
*/

#include "settings.h"


//Standard libraries to be imported
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <signal.h>
#include <errno.h>
#include <dirent.h>
#include <limits.h> 
#include <time.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <pthread.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sdp.h>
#include <bluetooth/sdp_lib.h>
#include <wiringPi.h>
#include <wiringSerial.h>


#define NUM_THREADS 7
#define QSIZE 100
#define PI_DIR "pi_photos"
#define LABEL_DIR "label_photos"  
#define WORK_DIR "/home/pi/Desktop/"
#define LABEL_TXT "labels.txt"

/*Global Variables*/
//Main Variables
int tcp_status, bt_status, serial_status;
pthread_mutex_t lock, flock; 

//TCP Variables
struct sockaddr_in servaddr, tcp_client;
socklen_t tcp_opt = sizeof(tcp_client);
int tcp_sockfd, clientconn, algo_sourceaddr, isSetOption = 1;
int clientconn2;

//BT Variables
uint32_t svc_uuid_int[] = { 0x00001101, 0x00001000, 0x80000080, 0x5F9B34FB }; //Unique UUID 01110000-0010-0000-8000-0080fb349b5f
int bt_sock, client;

//Serial Variables
int fd_serial;

//File Reading for Image Label Variables
char final_labels[20][15];
int label_size = 0;

//Queue Variables
struct Queue{
    int front, rear, size;
    unsigned capacity;
    char** array;
};

struct Queue* t_queue;
struct Queue* b_queue;
struct Queue* s_queue;
/*Global Variables End*/

/*Section 1 TCP Functions*/
int tcp_connect();
void tcp_disconnect(int sock);
void tcp_reconnect();
void* readTCP(void* args);
char* tcp_read();
int tcp_send(char* msg);
void* send2TCP(void* args);
/*Section 1 TCP Functions End*/

/*Section 2 BT Functions*/
int bt_connect();
sdp_session_t* register_service(uint8_t rfcomm_channel);
void bt_disconnect();
void bt_reconnect();
void* readBT(void* args);
char* bt_read();
void* send2BT(void* args);
int bt_send(char* msg);
/*Section 2 BT Functions End*/

/*Section 3 Serial Functions*/
int serial_connect();
void serial_disconnect();
void serial_reconnect();
void* readSerial(void* args);
char* serial_read();
void* send2Serial(void* args);
int serial_send(char* msg);
/*Section 3 Serial Functions End*/

/*Section 4 Image Functions*/
void checkPhotoDir();
bool camera_activate();
void* readLabels(void* args);
void read_labels();
void getLabels(char buf[]);
void processLabels(char** results, int num_labels);
void sendImageResults();
/*Section 4 Image Functions End*/

/*Section 5 Misc Functions*/
void distributeCommand(char buf[], char source);
void writeHub(char* wpointer, char source);
void all_disconnect(int sig);
/*Section 5 Misc Functions End*/

/*Section 6 Queue Functions*/
struct Queue* createQueue(unsigned capacity);
int isFull(struct Queue* queue);
int isEmpty(struct Queue* queue);
void enqueue(struct Queue* queue, char* item);
char* dequeue(struct Queue* queue);
int front(struct Queue* queue);
int rear(struct Queue* queue);
/*Section 6 Queue Functions End*/

//Main Functions Start
int main() {
	signal(SIGINT, all_disconnect); //Ctrl+C to terminate the entire program properly
	printf("===== Now initializing connections =====\n");
	system("sudo hciconfig hci0 piscan");	//Turn on BT discovery
	checkPhotoDir();				//Check for the directories need in cwd

	//Create the respective queues for each devices
	t_queue = createQueue(QSIZE);
	b_queue = createQueue(QSIZE);
	s_queue = createQueue(QSIZE);

    /*Service Configuration Section*/
    tcp_status = bt_status = 1; 	//Default should be 0
	serial_status = 1; 				//Default should be 0

	serial_status = serial_connect();
    bt_status = bt_connect();
	tcp_status = tcp_connect();

    /*Service Configuration Section*/

	while (!tcp_status || !bt_status || !serial_status) {
		if (!tcp_status) {
			printf("TCP failed to accept client... Retrying in 2s....");
			sleep(2);
			tcp_disconnect(tcp_sockfd);
			tcp_status = tcp_connect();
		}
		else if (!bt_status) {
			printf("BT connection failed... Retrying in 2s....");
			sleep(2);
			bt_disconnect();
			bt_status = bt_connect();
		}
		else if (!serial_status) {
			printf("Serial connection failed... Retrying in 2s....");
			sleep(2);
			serial_disconnect();
			serial_status = serial_connect();
		}
	}

	//Creates and joins the respective threads to the main program
	pthread_t* thread_group = malloc(sizeof(pthread_t) * NUM_THREADS);
	pthread_create(&thread_group[0], NULL, readTCP, "\0");
	pthread_create(&thread_group[1], NULL, send2TCP, "\0");
	pthread_create(&thread_group[2], NULL, readBT, "\0");
	pthread_create(&thread_group[3], NULL, send2BT, "\0");
	pthread_create(&thread_group[4], NULL, readSerial, "\0");
	pthread_create(&thread_group[5], NULL, send2Serial, "\0");
	pthread_create(&thread_group[6], NULL, readLabels, "\0");

	int i;
	for (i = 0; i < NUM_THREADS; i++) {
		pthread_join(thread_group[i], NULL);
	}

	while (1) {
		sleep(1);
	}

	return 0;
}

/*Section 1 Start*/
//To create and establish tcp connection
int tcp_connect() {

	//Creates an IPv4 two-way endpoint connection for communication
	tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	setsockopt(tcp_sockfd, SOL_SOCKET, SO_REUSEADDR, (char*)& isSetOption, sizeof(isSetOption));
	if (tcp_sockfd == -1) {
		perror("tcp_connect: Error encountered when creating TCP socket: ");
		return 0;
	}
	else {
		printf("tcp_connect: TCP socket has been created successfully!\n");
	}

	//Clears the memory of the sockaddr_in variable
	bzero(&servaddr, sizeof(servaddr));

	//Assigns the IP and Port number to the struct for binding
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
	servaddr.sin_port = htons(SERVER_PORT);

	//Binds the newly created socket to the given IP and verification
	if ((bind(tcp_sockfd, (SA*)& servaddr, sizeof(servaddr))) != 0) {
		perror("tcp_connect: Error encountered when binding trying to bind: ");
	}
	else {
		printf("tcp_connect: TCP Socket successfully binded....\n");
	}

	//Configure server to listen for incoming connections
	if (listen(tcp_sockfd, 2) != 0) {
		perror("tcp_connect: Error encountered when listening for TCP connections: ");
		return 0;
	}
	else {
		printf("tcp_connect: TCP Server is now listening...\n");
	}

    //Can shift to become part of the multi thread connection
	//Accept the incoming data packet from client and verfication
	clientconn = accept(tcp_sockfd, (SA*)& tcp_client, &tcp_opt);
	if (clientconn < 0) {
		perror("tcp_connect: Error encountered when accepting TCP clients: ");
		return 0;
	}
	else {
		printf("tcp_connect: TCP Server has successfully accepted the client...\n");
	}

	return 1;
}

//To disconnect a TCP socket
void tcp_disconnect(int sock) {
	if (!close(sock)) {
		printf("tcp_disconnect: TCP connection id %d closed successfully!\n", sock);
	}
	else {
		perror("tcp_disconnect: Error encountered when trying to close TCP connection: ");
	}
}

//Purpose is to reconnect client
void tcp_reconnect() {
	int conn = 0;

	while (!conn) {
		printf("tcp_reconnect: Attempting to restart tcp connection.....\n");
		tcp_disconnect(tcp_sockfd);
		conn = tcp_connect();
		sleep(1);
	}

	printf("tcp_reconnect: TCP server connection successfully started!\n");
}

//To setup thread to read from TCP port
void* readTCP(void* args) {
	char read_buf[MAX];
	char* rpointer;

	while (1) {
		rpointer = tcp_read();
		if (rpointer) {
			strcpy(read_buf, rpointer);
			memset(rpointer, '\0', MAX);
			distributeCommand(read_buf, 't');
		}
		else {
			perror("readTCP: Error encountered when receiving data from tcp_read: ");
		}
	}
}

//To read from the TCP port if data is available
char* tcp_read() {
	int count = 0;
	char tcp_buf[MAX];
	char* p;
	int bytes_read = 0;

	while (1) {
		bytes_read = read(clientconn, &tcp_buf[count], (MAX - count));
		count += bytes_read;
		if (bytes_read > 0) {
			if (strlen(tcp_buf) == 0) {
				continue;
			}
			else if (tcp_buf[0] == '@') {
				if (tcp_buf[count - 1] != '!') {
					continue;
				}
				else {
					printf("tcp_read: Received [%s] from tcp client connection\n", tcp_buf);
					tcp_buf[count] = '\0';
					p = tcp_buf;
					return p;
				}
			}
			else {
				printf("tcp_read: Invalid string [%s] received, please send a new command\n", tcp_buf);
				return '\0';
			}
		}
		else {
			perror("tcp_read: Error encountered when trying to read from TCP: ");
			tcp_reconnect();
			return '\0';
		}
	}
}

//To setup thread to start writing data received from TCP to devices 
void* send2TCP(void* args) {
	char* q;

	while (1) {
		if (!isEmpty(t_queue)) {
			pthread_mutex_lock(&lock); 
			q = dequeue(t_queue);

			writeHub(q, 't');
			pthread_mutex_unlock(&lock); 
		}
	}
}

//To send data to device through TCP port
int tcp_send(char* msg) {
	int bytes_write = 0;
	char send[MAX];

	if (strlen(msg) > 0) {
		strcpy(send, msg);
		strcat(send, "\n");
		bytes_write = write(clientconn, send, strlen(send));
		if (bytes_write > 0) {
			printf("tcp_send: RPi send message [%s] to PC.\n", send);
			fflush(stdout);
			return 1;
		}else{
            perror("tcp_send: Encountered error when RPi tried to send to TCP: ");
        }
		return 0;
	}
}
/*Section 1 End*/

/*Section 2 Start*/
//To create and establish rfcomm connection
int bt_connect() {
	//Initailise connection variables
	char buf[MAX];
	struct sockaddr_rc loc_addr, rem_addr;
	socklen_t opt = sizeof(rem_addr);

	//Registers the service
	sdp_session_t* session = register_service(BT_PORT);

	//Creates an RFCOMM bluetooth socket for communication
	bt_sock = socket(AF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
	if (bt_sock == -1) {
		perror("bt_connect: Error encountered when creating BT socket: ");
		return 0;
	}
	else {
		printf("bt_connect: Creation of BT socket successful....\n");
	}

	//Clears the memory of the loc_addr variable
	bzero(&loc_addr, sizeof(loc_addr));

	//Assigns the respective bluetooth variables for binding
	loc_addr.rc_family = AF_BLUETOOTH;
	loc_addr.rc_bdaddr = *BDADDR_ANY;                   //Sets which local bluetooth device to be used
	loc_addr.rc_channel = (uint8_t)BT_PORT;                  //Sets which RFCOMM channel to be used


	//Binds socket to port 1 of the first available local bluetooth adapter
	if ((bind(bt_sock, (SA*)& loc_addr, sizeof(loc_addr))) != 0) {
		perror("bt_connect: Error encountered when trying to bind BT socket: ");
		return 0;
	}
	else {
		printf("bt_connect: Binding of BT socket successful....\n");
	}

	//Configure server to listen for incoming connections
	if (listen(bt_sock, 1) != 0) {
		perror("bt_connect: Error encountered when listing for BT connections: ");
		return 0;
	}
	else {
		printf("bt_connect: Bluetooth Server is now listening for connections...\n");
	}

	//Accepts the incoming data packet from client
	client = accept(bt_sock, (SA*)& rem_addr, &opt);
	if (client < 0) {
		perror("bt_connect: Error encountered when trying to accept BT clients....: ");
		return 0;
	}
	else {
		printf("bt_connect: BT Server has accepted the client successfully...\n");
	}

	ba2str(&rem_addr.rc_bdaddr, buf);
	fprintf(stderr, "bt_connect: Accepted connection from %s\n", buf);
	memset(buf, 0, sizeof(buf));

	return 1;
}

//To create and establish service discovery protocol
sdp_session_t* register_service(uint8_t rfcomm_channel) {
	//Variables initialised for the registeration of sdp server
	const char* service_name = "Group 17 Bluetooth server";
	const char* svc_dsc = "MDP Server";
	const char* service_prov = "Group 17";

	uuid_t root_uuid, l2cap_uuid, rfcomm_uuid, svc_uuid, svc_class_uuid;

	sdp_list_t* l2cap_list = 0,
		*rfcomm_list = 0,
		*root_list = 0,
		*proto_list = 0,
		*access_proto_list = 0,
		*svc_class_list = 0,
		*profile_list = 0;

	sdp_data_t* channel = 0;
	sdp_profile_desc_t profile;
	sdp_record_t record = { 0 };
	sdp_session_t* session = 0;

	char str[256] = "";

	//This function uses a depreciated method which is not support in Bluez 5 hence we execute the following
	system("sudo chmod 777 /var/run/sdp");

	//Set the general service ID
	sdp_uuid128_create(&svc_uuid, &svc_uuid_int);
	sdp_set_service_id(&record, svc_uuid);
	sdp_uuid2strn(&svc_uuid, str, 256);
	printf("register_service: Registering UUID %s\n", str);

	//Set the service class
	sdp_uuid16_create(&svc_class_uuid, SERIAL_PORT_SVCLASS_ID);
	svc_class_list = sdp_list_append(0, &svc_class_uuid);
	sdp_set_service_classes(&record, svc_class_list);


	//Set the Bluetooth profile information
	sdp_uuid16_create(&profile.uuid, SERIAL_PORT_PROFILE_ID);
	profile.version = 0x0100;
	profile_list = sdp_list_append(0, &profile);
	sdp_set_profile_descs(&record, profile_list);


	//Allows the service record to be publicly browsable
	sdp_uuid16_create(&root_uuid, PUBLIC_BROWSE_GROUP);
	root_list = sdp_list_append(0, &root_uuid);
	sdp_set_browse_groups(&record, root_list);

	//Set l2cap information
	sdp_uuid16_create(&l2cap_uuid, L2CAP_UUID);
	l2cap_list = sdp_list_append(0, &l2cap_uuid);
	proto_list = sdp_list_append(0, l2cap_list);

	//Register the RFCOMM channel for RFCOMM sockets
	sdp_uuid16_create(&rfcomm_uuid, RFCOMM_UUID);
	channel = sdp_data_alloc(SDP_UINT8, &rfcomm_channel);
	rfcomm_list = sdp_list_append(0, &rfcomm_uuid);
	sdp_list_append(rfcomm_list, channel);
	sdp_list_append(proto_list, rfcomm_list);

	access_proto_list = sdp_list_append(0, proto_list);
	sdp_set_access_protos(&record, access_proto_list);

	//Set the name, provider, and description
	sdp_set_info_attr(&record, service_name, service_prov, svc_dsc);

	//Connect to the local SDP server, register the service record, and disconnect
	session = sdp_connect(BDADDR_ANY, BDADDR_LOCAL, SDP_RETRY_IF_BUSY);
	sdp_record_register(session, &record, 0);

	//Cleanup
	sdp_data_free(channel);
	sdp_list_free(l2cap_list, 0);
	sdp_list_free(rfcomm_list, 0);
	sdp_list_free(root_list, 0);
	sdp_list_free(access_proto_list, 0);
	sdp_list_free(svc_class_list, 0);
	sdp_list_free(profile_list, 0);

	return session;
}

//To disconnect a bt client
void bt_disconnect() {
	if (!close(client) && !close(bt_sock)) {
		printf("bt_disconnect: Bluetooth connection has closed successfully!\n");
	}
	else {
		perror("bt_disconnect: Error encountered when trying to close bluetooth connection: ");
	}
}

//Purpose is to reconnect client
void bt_reconnect() {
	int conn = 0;

	while (!conn) {
		printf("bt_reconnect: Attempting to restart bluetooth server.....\n");
		bt_disconnect();
		conn = bt_connect();
		sleep(1);
	}

	printf("bt_reconnect: Bluetooth services have successfully reconnected!\n");
}

//To setup thread to read from rfcomm channel
void* readBT(void* args) {
	char read_buf[MAX];
	char* rpointer;

	while (1) {
		rpointer = bt_read();
		if (rpointer) {
			strcpy(read_buf, rpointer);
			memset(rpointer, '\0', MAX);
			distributeCommand(read_buf, 'b');
		}
		else {
			perror("readBT: Error encountered when receiving data from bt_read:  ");
		}
	}
}

//To read from the Bt port if data is available
char* bt_read() {
	int count = 0;
	char bt_buf[MAX];
	char* p;
	int bytes_read = 0;

	while (1) {
		bytes_read = read(client, &bt_buf[count], MAX - count);
		count += bytes_read;
		if (bytes_read > 0) {
			if (strlen(bt_buf) == 0) {
				continue;
			}
			else if (bt_buf[0] == '@') {
				if (bt_buf[count - 1] != '!') {
					continue;
				}
				else {
					printf("bt_read: Received [%s] from BT client connection\n", bt_buf);
					bt_buf[count] = '\0';
					p = bt_buf;
					return p;
				}
			}
			else {
				printf("bt_read: Invalid string [%s] received, please send a new command\n", bt_buf);
				return '\0';
			}
		}
		else {
			perror("bt_read: Error encountered when trying to read from BT: ");
			bt_reconnect();
			return '\0';
		}
	}

}

//To setup thread to start writing data received from Bt to devices
void* send2BT(void* args) {
	char* q;

	while (1) {
		if (!isEmpty(b_queue)) {
			pthread_mutex_lock(&lock); 
			q = dequeue(b_queue);
			writeHub(q, 'b');
			pthread_mutex_unlock(&lock); 
		}
	}
}

//To send data to device through Bt port
int bt_send(char* msg) {
	int bytes_write = 0;
	char send[MAX];

	if (strlen(msg) > 0) {
		strcpy(send, msg);
		strcat(send, "\n");
		bytes_write = write(client, msg, strlen(msg));
		if (bytes_write > 0) {
			printf("bt_send: RPi send message [%s] to BT.\n", msg);
			fflush(stdout);
			return 1;
		}else{
            perror("bt_send: Encountered error when RPi tried to send to BT: ");
        }
	}
	return 0;
}
/*Section 2 End*/

/*Section 3 Start*/
//To verify the use of serial connection
int serial_connect() {
	fd_serial = serialOpen(SERIAL_PORT, BAUD);
	if (fd_serial == -1) {
		perror("serial_connect: Error encountered when establishing serial port connection: ");
		return 0;
	}
	else {
		printf("serial_connect: Serial port connection %s with %d established successfully.\n", SERIAL_PORT, BAUD);
		return 1;
	}
}

//To unverify the use of serial connection
void serial_disconnect() {
	serialClose(fd_serial);
	printf("serial_disconnect: Serial port connection closed successfully!\n");
}

//Purpose is to reconnect client
void serial_reconnect() {
	int conn = 0;

	while (!conn) {
		printf("serial_reconnect: Attempting to re-establish connection to serial port.....\n");
		serial_disconnect();
		conn = serial_connect();
		sleep(1);
	}

	printf("serial_reconnect: Serial connection successfully reconnected!\n");
}

//To setup thread to read from serial port
void* readSerial(void* args) {
	char read_buf[MAX];
	char* rpointer;

	while (1) {
		rpointer = serial_read();
		if (rpointer) {
			strcpy(read_buf, rpointer);
			memset(rpointer, '\0', MAX);
			distributeCommand(read_buf, 's');
		}
		else {
			perror("readSerial: Error encountered when receiving data from serial_read: ");
		}
	}
}

//To read from the serial port if data is available
char* serial_read() {
	int count = 0;
	char serial_buf[MAX];
	char newChar;
	char* p;

	while (1) {
		int bytes_read = serialDataAvail(fd_serial);
		if (bytes_read > 0) {
			newChar = serialGetchar(fd_serial);
			if (newChar == '\n') {
				if (serial_buf[0] == '@') {
					if (serial_buf[(count - 2)] == '!') {
						serial_buf[(count - 1)] = '\0';
						printf("serial_read: Received [%s] from SERIAL client connection\n", serial_buf);
						p = serial_buf;
						return p;
					}
					else {
						continue;
					}
				}
				else {
					printf("serial_read: Invalid string [%s] received, please send a new command\n", serial_buf);
					return 0;
				}
			}
			else {
				serial_buf[count] = newChar;
				count++;
			}
		}
		else if (bytes_read < 0) {
			perror("serial_Read: Error encountered when trying to read from serial: ");
			serial_reconnect();
			return 0;
		}
	}
}

//To setup thread to start writing data received from serial port to devices
void* send2Serial(void* args) {
	char* q;

	while (1) {
		if (!isEmpty(s_queue)) {
			pthread_mutex_lock(&lock); 
			q = dequeue(s_queue);
			// printf("[%s] dequeued into serial queue\n", q);
			writeHub(q, 's');
			pthread_mutex_unlock(&lock); 
		}
	}
}

//To send data to device through serial port
int serial_send(char* msg) {
	char send[MAX];

	if (strlen(msg) > 0) {
		strcpy(send, msg);
		strcat(send, "\n");
		serialPuts(fd_serial, send);
        printf("serial_send: RPi send message [%s] to serial.\n", msg);
        fflush(stdout);
        return 1;
	}
	return 0;
}
/*Section 3 End*/

/*Section 4 Start*/
//To check if the necessary folders are available if not it will create
void checkPhotoDir(){
	//This function will make two directories needed for imaging if not available
    char dirList[2][20] = {PI_DIR, LABEL_DIR};
    char path[] = WORK_DIR;
    char emptyDir[1024];
    DIR* dir;
    int i;

    //This function will remove the existing folders and create the new folders
    for(i=0; i<2; i++){
        dir = opendir(dirList[i]);
        if(dir){
            strcpy(emptyDir, "sudo rm -r ");
            strcat(emptyDir, dirList[i]);
            system(emptyDir);
            closedir(dir);
        }
		strcat(path, dirList[i]);
		mkdir(path, 0777);
		strcpy(path, WORK_DIR);
		
    }
}

//To activate the Pi camera module and wait for confirmation of snapshot
bool camera_activate(char* coordinates) {
	//This function will run the python script that will take photos and stream to a tcp server on the PC.
	char buf[5];
	char cmp[5] = "done";
	FILE* stream;

    char command[1024] = "sudo /usr/bin/python3.4 snapshot.py ";
    strcat(command, coordinates);

	stream = popen(command , "r");
	if (stream) {
		while (!feof(stream)) {
			if (fgets(buf, 5, stream)) {
				if (!strcmp(buf, cmp)) {
					printf("Image has been successfully taken!\n");
					pclose(stream);
					return true;
				}
				else {
					printf("An error might have occurred during the execution of python file.\n");
					pclose(stream);
					return false;
				}
			}
		}
	}
	else {
		perror("Unable to open stream with error: ");
        return false;
	}
}

//To setup thread to run the OS command to list the files available in LABEL_DIR into a txt file
void* readLabels(void* args){
    //To get the items within a folder into a text file, e.g. system("ls label_photos > label_photos/labels.txt")
    char command[] = "ls ";
    strcat(command, LABEL_DIR);
    strcat(command, " > ");
    strcat(command, "/home/pi/Desktop/");
    strcat(command, LABEL_TXT);

    while(1){
        //To run every 1seconds to check for data available within the labels folder
        sleep(1);
        pthread_mutex_lock(&flock);
        system(command);        //To generate the labels.txt
        pthread_mutex_unlock(&flock);
        read_labels();
    }           
}

//To read the txt file
void read_labels(){
    //To read the txt file of the current contents within the directory
    char r_buf[MAX];
    int count = 0, fsize = 0;
    FILE *fptr;
    char filename[] = "/home/pi/Desktop/";

    strcat(filename, LABEL_TXT);

    pthread_mutex_lock(&flock);
    fptr = fopen(filename, "r");
    if(!fptr){
        perror("An error has occurred when trying to read from txt file: ");
    }else{
		fseek(fptr, 0, SEEK_END);
		fsize = ftell(fptr);
		if(fsize > 0){
			fseek(fptr, 0, 0);
        	while(fscanf(fptr, "%c", r_buf+count) != EOF){count+=1; continue;}
			fclose(fptr);
			pthread_mutex_unlock(&flock);
			getLabels(r_buf);
		}else{
			fclose(fptr);
			pthread_mutex_unlock(&flock);
		}
    }
	memset(r_buf, '\0', MAX);
}

//Retrieves the labels from txt
void getLabels(char buf[]){
    //Labels should be labels, x coordinates, y_coordinates e.g. 10_11_9
    const char s[3] = "\n";
    char* labels[10];
    int i = 0, count = 0, len = 0, new_len = 0;
	char* point;

	point = strtok(buf, s);

	if (!point) {
		perror("getLabels: Error encountered when splitting received data: ");
	}
	else {
		while (point != NULL) {
				if(strlen(point) < 13){
					len = strlen(point);
					*(point+(len-4)) = '\0';
					new_len = strlen(point);
					if(strcmp(point+(new_len-3),"x_x") != 0){	//Checks for false positive
						labels[count] = point;
						printf("Value:%s\n", labels[count]);
						count+=1;
					}
				}
				point = strtok(NULL, s);
		}
	} 

   processLabels(labels, count);
}

//Verifies whether new labels are available to be sent
void processLabels(char** results, int num_labels){
    int i, j;
	bool send = false, add = true;
	
	//Updates the label list for new labels to be sent to BT
	if(label_size == 0){
		for(i = 0; i < num_labels; i++){
			strcpy(final_labels[label_size],results[i]);
			label_size += 1;
			send = true;	
		}
	}else{
        for(i = 0; i < num_labels; i++){
            for(j = 0; j < label_size; j++){
                int res = strcmp(results[i], final_labels[j]);
                if(res == 0){
                    add = false;
                    break;
                }
            }
            if(add){
				strcpy(final_labels[label_size],results[i]);
                label_size += 1;
				send = true;
            }
			add = true;
        }
    }

	//Formats the results into correct format
	if(send){
		sendImageResults();
		send = false;
	}

}

//To properly format results before sending it to Bt
void sendImageResults(){
	char imageString[1024] = "@bIMG";
	int j;

	for(j = 0; j < label_size; j++){
		strcat(imageString, "|");
		strcat(imageString, final_labels[j]);
	}
	strcat(imageString, "!");

	for(j = 0; j < strlen(imageString); j++){
		if(*(imageString + j) == '_'){
			*(imageString + j) = ',';
		}
	}

	distributeCommand(imageString, 'b');
}
/*Section 4 End*/

/*Section 5 Start*/
//To seperate instructions receieved into seperate ones in the event when they are concatenated when received
void distributeCommand(char buf[], char source) {
	const char s[2] = "!";
	char* point;

	point = strtok(buf, s);

	if (!point) {
		perror("distributeCommand: Error encountered when splitting received data: ");
	}
	else {
		while (point != NULL) {
			pthread_mutex_lock(&lock);
			if (source == 't') {
				enqueue(t_queue, point);
			}
			else if (source == 'b') {
				enqueue(b_queue, point);
			}
			else if (source == 's') {
				enqueue(s_queue, point);
			}
			pthread_mutex_unlock(&lock); 
			point = strtok(NULL, s);
		}
	}

}

//To determine where the instructions received should be route to
void writeHub(char* wpointer, char source) {
	if (wpointer) {
		if (strlen(wpointer) > 0) {
			if (tolower(wpointer[1]) == 't') {
				if (source == 's') {
					*(wpointer + 1) = 's';
				}
				else if (source == 'b') {
					*(wpointer + 1) = 'b';
				}

				tcp_send(wpointer + 1);
				//printf("Pretends to executes sending to TCP client (send2TCP) with message: %s\n", wpointer+1);
			}
			else if (tolower(wpointer[1]) == 'b') {

				bt_send((void*)wpointer + 2);
				//printf("Pretends to executes sending to BT client (send2BT) with message: %s\n", wpointer + 2);
			}
			else if (tolower(wpointer[1]) == 's') {

				serial_send((void*)wpointer + 2);
				//printf("Pretends to executes sending to Serial client (send2Serial) with message: %s\n", wpointer + 2);
			}
			else if (tolower(wpointer[1]) == 'r') {
				while(1){
					if(camera_activate(wpointer + 2)){tcp_send("rTAKEN"); break;}
				}
			}
			else {
				printf("writeHub: Incorrect format provided, message [%s] will be dropped!\n", wpointer);
			}
		}
	}
	else {
		perror("writeHub: Error encountered when receiving data to be routed: ");
	}

}

//To initiate a closure on all connection ports
void all_disconnect(int sig) {
	tcp_disconnect(tcp_sockfd);
	bt_disconnect();
	serial_disconnect();
	exit(EXIT_SUCCESS);
}
/*Section 4 End*/

/*Section 5 Start*/
//Function to create Queues of Queue 
struct Queue* createQueue(unsigned capacity) 
{ 
    struct Queue* queue = (struct Queue*) malloc(sizeof(struct Queue)); 
    queue->capacity = capacity; 
    queue->front = queue->size = 0;  
    queue->rear = capacity - 1;  // This is important, see the enqueue 
    queue->array = (char**)malloc(queue->capacity*sizeof(char*));
    return queue; }

//Checks if the queue is full
int isFull(struct Queue* queue) 
{  return (queue->size == queue->capacity);  } 
  
// Checks whether the queue is empty
int isEmpty(struct Queue* queue) 
{  return (queue->size == 0); } 
  
//Adds an item to the end of the queue
void enqueue(struct Queue* queue, char* item) 
{ 
    if (!isFull(queue)){ 
        queue->rear = (queue->rear + 1)%queue->capacity; 
        queue->array[queue->rear] = item;
        queue->size = queue->size + 1; 
        printf("Value [%s] enqueued to queue\n", item);
    } } 
  
//Returns and removes the first item in the queue
char* dequeue(struct Queue* queue) 
{ 
    if (!isEmpty(queue)){ 
        char* item = queue->array[queue->front]; 
        queue->front = (queue->front + 1)%queue->capacity; 
        queue->size = queue->size - 1; 
		printf("dequeue: Value [%s] dequeued from queue\n", item);
        return item; 
    }

    return 0; } 

//Returns the first item in the queue
int front(struct Queue* queue) 
{ 
    if (!isEmpty(queue)){ 
        return queue->array[queue->front]; 
    }
    return INT_MIN; } 
  
//Returns the last item in the queue
int rear(struct Queue* queue) 
{ 
    if (!isEmpty(queue)){ 
        return queue->array[queue->rear]; 
    }
    return INT_MIN;}

/*Section 5 End*/

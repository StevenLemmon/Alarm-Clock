#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_config.h"
#include "user_interface.h"
#include "mem.h"
#include "uart.h"

#define PACKET_SIZE		(2*1024)
#define uart_recvTaskPrio		0
#define uart_recvTaskQueueLen	10

os_event_t	uart_recvTaskQueue[uart_recvTaskQueueLen];
LOCAL os_timer_t check_timer;
LOCAL struct espconn user_tcp_conn;
LOCAL struct espconn* serverEspconn;
LOCAL struct _esp_tcp user_tcp;
ip_addr_t tcp_server_ip;

const char* requestIp = "Requesting IP";

/*
* Callback function for data recieved
*/
LOCAL void ICACHE_FLASH_ATTR user_tcp_recv_cb(void* arg, char* usrdata, unsigned short length) {
   os_printf("TCP received successful.\n");
}

/*
* Callback function for data snet
*/
LOCAL void ICACHE_FLASH_ATTR user_tcp_sent_cb(void* arg) {
   os_printf("TCP sent successful.\n");
}

/*
* Callback function for disconnect
*/
LOCAL void ICACHE_FLASH_ATTR user_tcp_discon_cb(void* arg) {
   os_printf("TCP disconnect successful.\n");
}

/*
 * Used to send data to the host
 */
LOCAL void ICACHE_FLASH_ATTR user_sent_data(struct espconn* hostEspconn) {
	char packetBuff[40] = {0};
	os_sprintf(packetBuff, "%s", requestIp);
	espconn_sent(hostEspconn, packetBuff, os_strlen(packetBuff));
}

/*
 * Connect callback function for when an incoming TCP connection connected
 */
LOCAL void ICACHE_FLASH_ATTR user_tcp_connect_cb(void* arg) {
	serverEspconn = arg;

	//Set up callback functions
	espconn_regist_recvcb(serverEspconn, user_tcp_recv_cb);
	espconn_regist_sentcb(serverEspconn, user_tcp_sent_cb);
	espconn_regist_disconcb(serverEspconn, user_tcp_discon_cb);
}

/*
 * Reconnect callback function for when an error occures
 */
LOCAL void ICACHE_FLASH_ATTR user_tcp_recon_cb(void* arg, sint8 err) {
	//Currently only tries to connect again.  Look into making better
	//reconnect functionality 
	os_printf("TCP error:%d.\n", err);
	struct espconn* hostEspconn = arg;
	espconn_connect(hostEspconn);

}

/*
 * Checks if we correctly recieved an IP or not for TCP
 */
void ICACHE_FLASH_ATTR user_check_ip_tcp(void) {
	struct ip_info ipconfig;
	
	//turn off timer
	os_timer_disarm(&check_timer);
	
	//Get the information of the station
	wifi_get_ip_info(STATION_IF, &ipconfig);

	//Checks to see if we got everything we need
	if(wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
		os_printf("\nGot TCP IP\n");
		//Connect to the TCP server
		user_tcp_conn.proto.tcp = (esp_tcp*)os_zalloc(sizeof(esp_tcp));
		user_tcp_conn.type = ESPCONN_TCP;
		user_tcp_conn.state = ESPCONN_NONE;

		//192.168.4.1 is the default IP for any ESP8266 acting as an 
		//access point.  
		const char esp_tcp_server_ip[4] = {192, 168, 4, 1};

		char tcp_local_ip[4] = {0, 0, 0, 0};
		tcp_local_ip[0] = ip4_addr1(&ipconfig.ip);
		tcp_local_ip[1] = ip4_addr2(&ipconfig.ip);
		tcp_local_ip[2] = ip4_addr3(&ipconfig.ip);
		tcp_local_ip[3] = ip4_addr4(&ipconfig.ip);

		os_memcpy(user_tcp_conn.proto.tcp->remote_ip, esp_tcp_server_ip, 4);
		os_memcpy(user_tcp_conn.proto.tcp->local_ip, tcp_local_ip, 4);

		user_tcp_conn.proto.tcp->remote_port = 2345;
		user_tcp_conn.proto.tcp->local_port = espconn_port();

		//Register a connect and reconnect callback function 
		espconn_regist_connectcb(&user_tcp_conn, user_tcp_connect_cb);
		espconn_regist_reconcb(&user_tcp_conn, user_tcp_recon_cb);
		sint8 ret = espconn_connect(&user_tcp_conn);
	} else {
		//Restart timer
		os_timer_setfn(&check_timer, (os_timer_func_t *)user_check_ip_tcp, NULL);
		os_timer_arm(&check_timer, 100, 0);	
	}
}

/*
 * Sets up the wifi interface for the ESP8266
 */
LOCAL void ICACHE_FLASH_ATTR user_wifi_init() {
	//Set up the network name and password that we are connecting to
	struct station_config stationConf;		
	wifi_station_get_config(&stationConf);

	os_memset(stationConf.ssid, 0, 32);
	os_memset(stationConf.password, 0, 64);

	os_memcpy(stationConf.ssid, "SSIDTest", 8);
	os_memcpy(stationConf.password, "12345678", 8);

	//Do not need MAC address
	stationConf.bssid_set = 0;

	wifi_station_set_config(&stationConf);
	wifi_station_connect();

	//Set a timer to check if we successfully recieved an IP or not
	os_timer_disarm(&check_timer);
	os_timer_setfn(&check_timer, (os_timer_func_t *)user_check_ip_tcp, NULL);
	os_timer_arm(&check_timer, 100, 0);

}

/*
 * This funciton gets called everytime something comes in on serial
 */
void ICACHE_FLASH_ATTR uart_rx_task(os_event_t* events) {
	user_sent_data(serverEspconn);

	//Zero means everything was recieved normally
	if(events->sig == 0) {
		//Get the length of the data
		uint8_t rxLength = (READ_PERI_REG(UART_FIFO(UART0)) >> UART_RXFIFO_CNT_S) & UART_RXFIFO_CNT;

		char rxChar;
		uint8_t i = 0;
		for(i=0; i<rxLength; i++) {
			rxChar = READ_PERI_REG(UART_FIFO(UART0)) && 0xFF;

			//Got the message about alarm, send message to server to start
			
		}
	}
}

//Init function 
void ICACHE_FLASH_ATTR user_init() {
	//Setup UART and use 9600 BAUD for it
	uart_init(9600, 9600);

	//Init GPIO pins and set GPIO2 to output mode
	gpio_init();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);

	//Set ESP8266 to station mode
	wifi_set_opmode(STATION_MODE);

	//Connect to the server ESP8266
	user_wifi_init();
}


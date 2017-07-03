#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "ip_addr.h"
#include "espconn.h"
#include "user_config.h"
#include "user_interface.h"
#include "mem.h"

#define SERVER_LOCAL_PORT	2345

LOCAL os_timer_t check_timer;
LOCAL struct espconn esp_conn;
LOCAL struct espconn udpEspconn;
LOCAL struct _esp_tcp esptcp;

const char* requestIp = "Requesting IP";
LOCAL uint16_t server_timeover = 60*60*12;

/*
* Callback function for data recieved
*/
LOCAL void ICACHE_FLASH_ATTR user_tcp_recv_cb(void* arg, char* usrdata, unsigned short length) {
   os_printf("TCP recieved successful.\n");
	int i=0;
	for(i=0; i<10; i++) {
	if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
		gpio_output_set(0, BIT2, BIT2, 0);
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
	os_delay_us(50000);	
	}

}

/*
* Callback function for data snet
*/
LOCAL void ICACHE_FLASH_ATTR tcp_server_sent_cb(void* arg) {
   os_printf("TCP sent successful.\n");
}

/*
* Callback function for disconnect
*/
LOCAL void ICACHE_FLASH_ATTR tcp_server_discon_cb(void* arg) {
   os_printf("TCP disconnect successful.\n");
}

/*
 * Reconnect callback function for when an error occures
 */
LOCAL void ICACHE_FLASH_ATTR tcp_server_recon_cb(void* arg, sint8 err) {
	//TODO: Set up reconnect stuff
}

/*
 * TODO: Figure out what this does
 */
LOCAL void tcp_server_multi_send(void) {
	struct espconn* esp_connCopy = &esp_conn;

	remot_info* remoteInfo = NULL;
	uint8 count = 0;
	sint8 value = ESPCONN_OK;
	
	if(espconn_get_connection_info(esp_connCopy, &remoteInfo, 0) == ESPCONN_OK) {
		char* pbuf = "tcp_server_multi_send\n";
		for(count = 0; count < esp_connCopy->link_cnt; count++) {
			esp_connCopy->proto.tcp->remote_ip[0] = remoteInfo[count].remote_ip[0];
			esp_connCopy->proto.tcp->remote_ip[1] = remoteInfo[count].remote_ip[1];
			esp_connCopy->proto.tcp->remote_ip[2] = remoteInfo[count].remote_ip[2];
			esp_connCopy->proto.tcp->remote_ip[3] = remoteInfo[count].remote_ip[3];

			espconn_sent(esp_connCopy, pbuf, os_strlen(pbuf));
		}
	}
}

/*
 * Setup the callback functions for receive, reconnect, and disconnect
 */
LOCAL void ICACHE_FLASH_ATTR tcp_server_listen(void* arg) {
	struct espconn* clientEspconn = arg;
	
	espconn_regist_recvcb(clientEspconn, user_tcp_recv_cb);
	espconn_regist_reconcb(clientEspconn, tcp_server_recon_cb);
	espconn_regist_disconcb(clientEspconn, tcp_server_discon_cb);
	espconn_regist_sentcb(clientEspconn, tcp_server_sent_cb);

	os_printf("TCP connected.\n\n");
	/*int i=0;
	for(i=0; i<10; i++) {
	if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
		gpio_output_set(0, BIT2, BIT2, 0);
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
	os_delay_us(50000);	
	}*/

	//tcp_server_multi_send();
}

/*
 * Initialize TCP server
 */
void ICACHE_FLASH_ATTR user_tcp_init(uint32 port) {
	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = (esp_tcp*)os_zalloc(sizeof(esp_tcp));
	esp_conn.proto.tcp->local_port = port;
	espconn_regist_connectcb(&esp_conn, tcp_server_listen);

	os_printf("\n\nTCP server Init\n\n");

	sint8 ret = espconn_accept(&esp_conn);
	os_printf("\n\nAccept return=%d\n\n", ret);
	espconn_regist_time(&esp_conn, server_timeover, 0);
}

/*
 * Checks if we correctly recieved an IP or not
 */
void ICACHE_FLASH_ATTR user_check_ip(void) {
	struct ip_info ipconfig;
	
	//turn off timer
	os_timer_disarm(&check_timer);
	
	//Get the information of the station
	wifi_get_ip_info(STATION_IF, &ipconfig);

	//Checks to see if we got everything we need
	if(wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
		//user_tcpserver_init(SERVER_LOCAL_PORT);
		if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
			//gpio_output_set(0, BIT2, BIT2, 0);
		} else {
			//gpio_output_set(BIT2, 0, BIT2, 0);
		}	
	} else {
		if(!(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD || 
			wifi_station_get_connect_status() == STATION_NO_AP_FOUND || 
			wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) 
		{
			//Restart timer
			os_timer_setfn(&check_timer, (os_timer_func_t *)user_check_ip, NULL);
			os_timer_arm(&check_timer, 100, 0);	
		}
	}
}

/*
 * Sets up the wifi interface for the ESP8266
 */
LOCAL void ICACHE_FLASH_ATTR user_wifi_init() {
	//Set station mode - we will talk to a WiFi router
	//wifi_set_opmode(STATION_MODE);

	//Set up the network name and password that we are connecting to
	struct station_config stationConf;
	os_memset(stationConf.ssid, 0, 32);
	os_memset(stationConf.password, 0, 64);

	os_memcpy(&stationConf.ssid, SSID, 32);
	os_memcpy(&stationConf.password, SSID_PASSWORD, 64);

	//Do not need MAC address
	stationConf.bssid_set = 0;

	wifi_station_set_config(&stationConf);

	//Set a timer to check if we successfully recieved an IP or not
	os_timer_disarm(&check_timer);
	os_timer_setfn(&check_timer, (os_timer_func_t *)user_check_ip, NULL);
	os_timer_arm(&check_timer, 100, 0);
}

/*
 * Function that is called everytime something is recieved through UDP
 */
LOCAL void ICACHE_FLASH_ATTR user_udp_recv(void* arg, char* data, unsigned short length) {
	char messageBuffer[40] = {0};
	struct ip_info ipconfig;
	/*int i=0;
	for(i=0; i<10; i++) {
	if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
		gpio_output_set(0, BIT2, BIT2, 0);
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
	os_delay_us(50000);	
	}*/

	if(data == NULL)
		return;
	
	//Checking to see what mode the ESP8266 is in that is sending the packet
	if(wifi_get_opmode() != STATION_MODE) {
		wifi_get_ip_info(SOFTAP_IF, &ipconfig);
		
		//Check to see if we get anything assuming it's a softAP
		if(!ip_addr_netcmp((struct ip_addr*)udpEspconn.proto.udp->remote_ip, &ipconfig.ip, &ipconfig.netmask)) {
			//It's not, so it is in station mode
			wifi_get_ip_info(STATION_IF, &ipconfig);
		}
	} else {
		wifi_get_ip_info(STATION_IF, &ipconfig);
	}

	//Check to make sure the message we recieved is requesting an IP then send IP
	if(length == os_strlen(requestIp) && os_strcmp(data, requestIp, os_strlen(requestIp)) == 0) {
	int i=0;
	for(i=0; i<10; i++) {
	if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
		gpio_output_set(0, BIT2, BIT2, 0);
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
	os_delay_us(50000);	
	}
		
	//TODO:Look at arg to see if the return IP is there.
		struct espconn* clientEspconn = arg;
		udpEspconn.proto.udp->remote_ip[0] = clientEspconn->proto.udp->local_ip[0];
		udpEspconn.proto.udp->remote_ip[1] = clientEspconn->proto.udp->local_ip[1];
		udpEspconn.proto.udp->remote_ip[2] = clientEspconn->proto.udp->local_ip[2];
		udpEspconn.proto.udp->remote_ip[3] = clientEspconn->proto.udp->local_ip[3];

		udpEspconn.proto.udp->remote_port = 1122;

		os_sprintf(messageBuffer, "IP:" IPSTR, IP2STR(&ipconfig.ip));
		length = os_strlen(messageBuffer);

		espconn_sent(&udpEspconn, messageBuffer, length);
	}
}

/*
 * Sets up a UDP server so clients can get the IP address of this server
 */
LOCAL void ICACHE_FLASH_ATTR user_udp_init() {
	udpEspconn.type = ESPCONN_UDP;
	udpEspconn.proto.udp = (esp_udp *)os_zalloc(sizeof(esp_udp));
	udpEspconn.proto.udp->local_port = 1122;
	
	/*int i=0;
	for(i=0; i<100; i++) {
	if(GPIO_REG_READ(GPIO_OUT_ADDRESS) & BIT2) {
		gpio_output_set(0, BIT2, BIT2, 0);
	} else {
		gpio_output_set(BIT2, 0, BIT2, 0);
	}
	os_delay_us(50000);	
	}*/

	//Register a function that will be called everything a UDP packet it received
	espconn_regist_recvcb(&udpEspconn, user_udp_recv);
	espconn_create(&udpEspconn);
}

/*
 * Sets up ESP8266 so you can connect to it
 */
void ICACHE_FLASH_ATTR user_set_softap_config(void) {
	struct softap_config config;

	wifi_softap_get_config(&config);

	os_memset(config.ssid, 0 , 32);
	os_memset(config.password, 0, 64);
	os_memcpy(config.ssid, "SSIDTest", 8);
	os_memcpy(config.password, "12345678", 8);
	config.authmode = AUTH_WPA_WPA2_PSK;
	config.ssid_len = 8;
	config.ssid_hidden = 0;
	config.max_connection = 3;

	wifi_softap_set_config(&config);
}

//Init function 
void ICACHE_FLASH_ATTR user_init() {
	//uart_init(115200, 115200);
	uart_div_modify(0, UART_CLK_FREQ/9600);

	gpio_init();

	//Set GPIO2 to output mode
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);

	wifi_set_opmode(SOFTAP_MODE);
	user_set_softap_config();

	//user_udp_init();
	user_tcp_init(2345);

	//user_wifi_init();
}


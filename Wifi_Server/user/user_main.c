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
 * Setup the callback functions for receive, reconnect, and disconnect
 */
LOCAL void ICACHE_FLASH_ATTR tcp_server_listen(void* arg) {
	struct espconn* clientEspconn = arg;
	
	//Set up callback functions for the TCP server.
	//Since I am only waiting to receive something form the client,
	//I am just initializing the receive callback, but I am leaving 
	//the others for future development
	espconn_regist_recvcb(clientEspconn, user_tcp_recv_cb);
	//espconn_regist_reconcb(clientEspconn, tcp_server_recon_cb);
	//espconn_regist_disconcb(clientEspconn, tcp_server_discon_cb);
	//espconn_regist_sentcb(clientEspconn, tcp_server_sent_cb);

	os_printf("TCP connected.\n\n");
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

	//Start TCP server listening on choosen port
	sint8 ret = espconn_accept(&esp_conn);
	//espconn_regist_time(&esp_conn, server_timeover, 0);
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
	//Setup UART to 9600 baud, this is for debugging print statements
	uart_div_modify(0, UART_CLK_FREQ/9600);

	//Init GPIO and set GPIO2 as output. This is for debugging.
	gpio_init();
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //Set GPIO2 low
    gpio_output_set(0, BIT2, BIT2, 0);dd

	//Set ESP8266 as an access point
	wifi_set_opmode(SOFTAP_MODE);

	//Set up access point
	user_set_softap_config();

	//Setup TCP server
	user_tcp_init(2345);
}


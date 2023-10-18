/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <stdlib.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <esp_idf_version.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include <sys/socket.h>
#include "lwip/sockets.h"
#include <lwip/netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include "esp_http_server.h"
#include "driver/gpio.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID      "MyESP32"
#define EXAMPLE_WIFI_PASS      ""
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       5





#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
// Features supported in 4.1+
#define ESP_NETIF_SUPPORTED
#endif


#define EXAMPLE_ESP_MAXIMUM_RETRY  10



#if CONFIG_ESP_WIFI_AUTH_OPEN
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static int s_retry_num = 0;
esp_err_t ret;

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
//static void udp_server_task(void *pvParameters);

//const char *device_id = "TX312";
char device_id[20];

#define GPIO_PIN_1 18
#define GPIO_PIN_2 19
#define GPIO_PIN_3 23
#define GPIO_PIN_4 22

int numClients = 0;
#define MAX_CLIENTS 3
//#define INET_ADDRSTRLEN 16 // Size for IPv4 addresses

#define SERVER_IP "192.168.0.124" // Replace with your server's IP address
#define SERVER_PORT 8585
char device_ip_str[16];  // Assuming a maximum IPv4 address length
char device_id_str[32];
char clientIPs[MAX_CLIENTS][INET_ADDRSTRLEN];



#define BROADCAST_IP "192.168.0.255" // Broadcast IP address
#define BROADCAST_PORT 8888
#define broadcast_PORT 8181
#define tcp_res_br 8001//port for the tcp response to the udp broadcast

int mode=0;


#define EXPECTED_DEVICE_ID "RX312"
#define BUFFER_SIZE 1024
static const char *TAG = "GPIO_tx";

static int last_gpio_state = -1;

void wifi_init_sta();


const char* html_form =
		"<!DOCTYPE html>\n"
		"<html>\n"
		"<head>\n"
		"    <title>Wi-Fi Configuration</title>\n"
		"    <style>\n"
		"        body {\n"
		"            background-image: linear-gradient(45deg, #FFFFFF, rgb(134, 145, 189));\n"
		"            color: #0000ff;\n"
		"            font-family: Arial, sans-serif;\n"
		"            text-align: center; \n"
		"        }\n"
		"        h1 {\n"
		"            color: #0000ff;\n"
		"        }\n"
		"        form {\n"
		"            max-width: 400px;\n"
		"            margin: 0 auto;\n"
		"            text-align: left; \n"
		"        }\n"
		"        label {\n"
		"            display: block;\n"
		"            margin-top: 10px;\n"
		"            color: #0000ff; \n"
		"            font-weight: bold;\n"
		"        }\n"
		"        input[type=\"text\"],\n"
		"        input[type=\"password\"],\n"
		"        input[type=\"range\"],\n"
		"        input[type=\"color\"] {\n"
		"            width: 100%;\n"
		"            padding: 10px;\n"
		"            margin-top: 5px;\n"
		"            background-color: transparent;\n"
		"            border: 1px solid #0000ff; \n"
		"        }\n"
		"        input[type=\"color\"] {\n"
		"            width: 50px; \n"
		"            height: 50px;\n"
		"            padding: 0; \n"
		"            border: none; \n"
		"            border-radius: 5px; \n"
		"        }\n"
		"        input[type=\"submit\"] {\n"
		"            background-color: #0000ff; \n"
		"            color: #ffffff;\n"
		"            border: none;\n"
		"            padding: 10px 20px;\n"
		"            margin-top: 10px;\n"
		"            cursor: pointer;\n"
		"        }\n"
		"        input[type=\"submit\"]:hover {\n"
		"            background-color: #0033cc; \n"
		"        }\n"
		"        span#rangeValue {\n"
		"            display: inline-block;\n"
		"            width: 50px;\n"
		"            text-align: center;\n"
		"            margin-left: 10px;\n"
		"            color: #0000ff; \n"
		"        }\n"
		"    </style>\n"
		"</head>\n"
		"<body>\n"
		"    <h1>Wi-Fi Configuration</h1>\n"
		"    <form method=\"POST\" action=\"/connect\">\n"
		"        <label for=\"ssid\">SSID:</label>\n"
		"        <input type=\"text\" id=\"ssid\" name=\"ssid\"><br><br>\n"
		"        <label for=\"password\">PASSWORD:</label>\n"
		"        <input type=\"password\" id=\"password\" name=\"password\"><br><br>\n"
		"        <label for=\"device_id\">DEVICE ID:</label>\n"
		"        <input type=\"text\" id=\"device\" name=\"device\" min=\"0\" max=\"100\"><br><br>\n"
		"        <label for=\"colour\">COLOUR:</label>\n"
		"        <input type=\"color\" id=\"colour\" name=\"colour\"><br><br>\n"
		"        <label for=\"value\">VALUE:</label>\n"
		"        <input type=\"range\" id=\"range\" name=\"range\" oninput=\"updateRangeValue(this.value)\"><span id=\"rangeValue\">0</span><br><br>\n"
		"        <input type=\"submit\" value=\"Update\">\n"
		"    </form>\n"
		"    <script>\n"
		"        function updateRangeValue(value) {\n"
		"            document.getElementById(\"rangeValue\").textContent = value;\n"
		"        }\n"
		"    </script>\n"
		"</body>\n"
		"</html>\n";

esp_err_t root_handler(httpd_req_t *req);
esp_err_t config_handler(httpd_req_t *req);

httpd_config_t config = HTTPD_DEFAULT_CONFIG();
httpd_handle_t server;
void wifi_stop_softap(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());
    ESP_ERROR_CHECK(esp_event_loop_delete_default());

}
esp_netif_ip_info_t ip_info;


//static bool station_mode_enabled = true; // Flag to control mode switching
void stop_http_server(void)
{
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
    }
}

esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_send(req, html_form, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t config_handler(httpd_req_t *req)
{
	const char *response = "WiFi credentials submitted succesfully....";
	httpd_resp_send(req, response, strlen(response));
    char ssid[32];
    char password[64];
    char device_name[64];
    char colour[64];
    char value[64];

    if (req->method == HTTP_POST) {
        memset(ssid, 0, sizeof(ssid));
        memset(password, 0, sizeof(password));

        int total_len = req->content_len;
        int cur_len = 0;
        int received;

        char data_buffer[total_len + 1];

        while (cur_len < total_len) {
            received = httpd_req_recv(req, data_buffer + cur_len, total_len - cur_len);
            if (received <= 0) {
                if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                    ESP_LOGE(TAG, "HTTP socket timeout");
                } else {
                    ESP_LOGE(TAG, "HTTP socket error");
                }
                return ESP_FAIL;
            }

            cur_len += received;

        }

        data_buffer[cur_len] = '\0';
        ESP_LOGI(TAG, "recieved: '%s', ", data_buffer);

        char *ssid_pos = strstr(data_buffer, "ssid=");
        char *password_pos = strstr(data_buffer, "password=");
        char *device_pos = strstr(data_buffer, "device=");
        char *colour_pos = strstr(data_buffer, "colour=");
        char *value_pos = strstr(data_buffer, "range=");

        if (ssid_pos != NULL && password_pos != NULL) {
            ssid_pos += strlen("ssid=");
            password_pos += strlen("password=");
            device_pos+=strlen("device=");
            colour_pos+=strlen("colour=");
            value_pos+=strlen("range=");

            char *ssid_end = strchr(ssid_pos, '&');
            char *password_end = strchr(password_pos, '&');
            char*device_end= strchr(device_pos,'&');
            char*colour_end =strchr(colour_pos,'&');
            char*value_end =strchr(value_pos,'&');

            if (ssid_end != NULL) {
                *ssid_end = '\0';
            }

            if (password_end != NULL) {
                *password_end = '\0';
            }
            if (device_end != NULL){
            	*device_end ='\0';
            }
            if(colour_end != NULL){
            	*colour_end ='\0';
            }
            if(value_end != NULL){
            	*value_end ='\0';
            }

            strncpy(ssid, ssid_pos, sizeof(ssid) - 1);
            strncpy(password, password_pos, sizeof(password) - 1);
            strncpy(device_name, device_pos, sizeof(device_name) - 1);
            strncpy(colour, colour_pos, sizeof(colour) - 1);
            strncpy(value, value_pos, sizeof(value) - 1);
        }
    }

    ESP_LOGI(TAG, "device: '%s', colour: '%s',value: '%s'", device_name,colour,value);

    if (strlen(ssid) > 0 && strlen(password) > 0&& strlen(device_name) > 0&& strlen(colour) > 0&& strlen(value) > 0) {
        nvs_handle_t nvs_handle;
        esp_err_t nvs_err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
        if (nvs_err == ESP_OK) {
            nvs_err = nvs_set_str(nvs_handle, "ssid", ssid);
            if (nvs_err == ESP_OK) {
                nvs_err = nvs_set_str(nvs_handle, "password", password);
                if (nvs_err == ESP_OK) {
                	nvs_err = nvs_set_str(nvs_handle, "device", device_name);
                	 if (nvs_err == ESP_OK) {
                		 nvs_err = nvs_set_str(nvs_handle, "colour", colour);
                		 if (nvs_err == ESP_OK) {
                			 nvs_err = nvs_set_str(nvs_handle, "value", value);
                			 if (nvs_err == ESP_OK) {
                				 nvs_err = nvs_commit(nvs_handle);
                	         }
                		}
                    }
                }
            }

            nvs_close(nvs_handle);
        }

        if (nvs_err == ESP_OK) {
            ESP_LOGI(TAG, " saving SSID and password to NVS");

        }



        return ESP_OK;
    }



   wifi_stop_softap();
   stop_http_server();

    return ESP_OK;
}


void start_http_server(void)
{
    if (server == NULL) {
        if (httpd_start(&server, &config) == ESP_OK) {
            ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
            httpd_uri_t root_uri = {
                .uri       = "/",
                .method    = HTTP_GET,
                .handler   = root_handler,
                .user_ctx  = NULL
            };
            httpd_register_uri_handler(server, &root_uri);
            httpd_uri_t connect_uri = {
                .uri       = "/connect",
                .method    = HTTP_POST,
                .handler   = config_handler,
                .user_ctx  = NULL
            };
            httpd_register_uri_handler(server, &connect_uri);
        }
    }
}


//event handler for AP
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_WIFI_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                    .required = false,
            },
        },
    };
    if (strlen(EXAMPLE_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Code to fetch and print the SoftAP IP address

    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("AP_STA_G"), &ip_info);
    ESP_LOGI(TAG, "SoftAP IP Address: " IPSTR, IP2STR(&ip_info.ip));

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS, EXAMPLE_ESP_WIFI_CHANNEL);


    //xTaskCreate(udp_server_task, "udp_server", 4096, (void*)AF_INET, 5, NULL);
}


//event handler for Station
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");

        //wifi_init_softap();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}


void wifi_init_sta(const char *ssid, const char *password)
{
    s_wifi_event_group = xEventGroupCreate();
//
//    ESP_ERROR_CHECK(esp_netif_init());
//
//    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
//
//    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (pasword len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
	     * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
      strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
      wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
      wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

      ESP_LOGI(TAG, "Ssid: '%s', password: '%s'", wifi_config.sta.ssid, wifi_config.sta.password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",wifi_config.sta.ssid, wifi_config.sta.password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:, password:");
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}


void start_wifi(int ret){
	char SSID[32];
	char PASS[64];

	memset(SSID, 0, sizeof(SSID));
	memset(PASS, 0, sizeof(PASS));

	nvs_handle_t nvs_handle;
	ret = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
	       if (ret == ESP_OK) {
	           size_t ssid_len = sizeof(SSID);
	           size_t pass_len = sizeof(PASS);

	           ret = nvs_get_str(nvs_handle, "ssid", SSID, &ssid_len);
	           if (ret == ESP_OK) {
	               ret = nvs_get_str(nvs_handle, "password", PASS, &pass_len);
	                 }

	           nvs_close(nvs_handle);
	       }

	       if (ret == ESP_OK && strlen(SSID) > 0 && strlen(PASS) > 0) {
	               // Initialize and connect to Wi-Fi using stored credentials
	               wifi_init_sta(SSID, PASS);
	           } else {
	               // No stored credentials, you may prompt the user to enter them
	               printf("No stored credentials. Please set SSID and Password.\n");
	           }
}

 void send_tcp_data(char ipAddresses[][INET_ADDRSTRLEN], char* message) {
    for (int i = 0; i < numClients; i++) {
        int clientSocket;
        struct sockaddr_in clientAddr;

        if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            ESP_LOGE("CLIENT", "Failed to create socket");
            continue;
        }

        memset(&clientAddr, 0, sizeof(clientAddr));
        clientAddr.sin_family = AF_INET;
        clientAddr.sin_port = htons(SERVER_PORT);
        inet_pton(AF_INET, ipAddresses[i], &clientAddr.sin_addr);

        if (connect(clientSocket, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) == -1) {
            ESP_LOGE("CLIENT", "Failed to connect to IP: %s", ipAddresses[i]);
            close(clientSocket);
            continue;
        }

        if (send(clientSocket, message, strlen(message), 0) == -1) {
            ESP_LOGE("CLIENT", "Failed to send data to IP: %s", ipAddresses[i]);
        }

        close(clientSocket);
    }
}




 void udp_broadcast_task() {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        vTaskDelete(NULL);
    }

    struct sockaddr_in broadcast_addr;
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(BROADCAST_PORT);
    broadcast_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    //const char *message = "Hello, Broadcast World!";



    while (1) {
        int err = sendto(sock, device_id, strlen(device_id), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr));
        if (err < 0) {
            ESP_LOGE(TAG, "Error sending broadcast message");
        } else {
            ESP_LOGI(TAG, "Broadcast message sent: %s", device_id);
        }

        vTaskDelay(pdMS_TO_TICKS(6000)); // Send a broadcast message every 5 seconds
//        if (device_ip_str != NULL) {
//               // End the task
//        	  // send_tcp_data("ONN02");
//               vTaskDelete(NULL);
//           }
    }

    close(sock);
    vTaskDelete(NULL);
}




void tcp_receive_task(void *pvParameters) {
		char buffer[BUFFER_SIZE];
	    int serverSocket, clientSocket;
	    struct sockaddr_in serverAddr, clientAddr;
	    socklen_t addrLen = sizeof(clientAddr);



	    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
	        ESP_LOGE("SERVER", "Failed to create socket");
	        vTaskDelete(NULL);
	    }

	    memset(&serverAddr, 0, sizeof(serverAddr));
	    serverAddr.sin_family = AF_INET;
	    serverAddr.sin_addr.s_addr = INADDR_ANY;
	    serverAddr.sin_port = htons(8989);

	    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
	        ESP_LOGE("SERVER", "Failed to bind");
	        vTaskDelete(NULL);
	    }

	    if (listen(serverSocket, 5) == -1) {
	        ESP_LOGE("SERVER", "Failed to listen");
	        vTaskDelete(NULL);
	    }

	    ESP_LOGI("SERVER", "TCP server listening on port %d", 8989);

	   // int numClients = 0;

	    while (numClients < MAX_CLIENTS) {
	        if ((clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &addrLen) == -1)) {
	            ESP_LOGE("SERVER", "Failed to accept connection");
	            continue;
	        }

	        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIPs[numClients], sizeof(clientIPs[numClients]));
	        ESP_LOGI("SERVER", "Client %d connected from IP: %s", numClients + 1, clientIPs[numClients]);

	        int bytesRead = recv(clientSocket, buffer, 128 - 1, 0);
	               if (bytesRead >= 0) {
	                   buffer[bytesRead] = '\0'; // Null-terminate the received data
	                   ESP_LOGI("SERVER", "Received data from Client %d: %s", numClients + 1, buffer);

	                   char response[] = "Received your message. Thank you!";
	                   send(clientSocket, response, strlen(response), 0);
	               }

	        numClients++;
	        close(clientSocket);
	    }
	    ESP_LOGI("SERVER", "Saved IP Addresses:");
	       for (int i = 0; i < numClients; i++) {
	           ESP_LOGI("SERVER", "Client %d IP: %s", i + 1, clientIPs[i]);
	       }

	    close(serverSocket);
	    vTaskDelete(NULL);
}


void send_udp_broadcast(const char *message) {
    int udp_socket;
    struct sockaddr_in server_addr;
    int broadcast_enable = 1;

    // Create a UDP socket
    if ((udp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        perror("Failed to create socket");
        exit(1);
    }

    // Enable broadcast on the socket
    if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable)) == -1) {
        perror("Failed to enable broadcast");
        close(udp_socket);
        exit(1);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(broadcast_PORT);
    server_addr.sin_addr.s_addr = inet_addr(BROADCAST_IP);

    // Send the UDP broadcast message
    if (sendto(udp_socket, message, strlen(message), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Failed to send broadcast message");
    }

    close(udp_socket);
}

void tcp_server_task(void *pvParameters) {
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        ESP_LOGE(TAG, "Unable to create server socket");
        vTaskDelete(NULL);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(tcp_res_br);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Unable to bind server socket");
        close(server_socket);
        vTaskDelete(NULL);
    }

    if (listen(server_socket, 5) < 0) {
        ESP_LOGE(TAG, "Unable to listen on server socket");
        close(server_socket);
        vTaskDelete(NULL);
    }

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_socket < 0) {
            ESP_LOGE(TAG, "Unable to accept client connection");
            continue;
        }

        char recv_buffer[1024];  // Adjust the buffer size as needed

        while (1) {
            int bytes_received = recv(client_socket, recv_buffer, sizeof(recv_buffer) - 1, 0);
            if (bytes_received < 0) {
                ESP_LOGE(TAG, "Error receiving data");
                break;
            } else if (bytes_received == 0) {
                //ESP_LOGI(TAG, "Client disconnected");
                break;
            }

            recv_buffer[bytes_received] = '\0';  // Null-terminate the received data

            // Process the received data
            ESP_LOGI(TAG, "Received data: %s", recv_buffer);

            // You can add your data processing logic here
        }

        close(client_socket);
    }

    close(server_socket);
    vTaskDelete(NULL);
}




// Function to monitor GPIO pins
void gpio_monitor_task(void *arg) {
    int gpio_pin = (int)arg;
    int last_gpio_state = -1;

    while (1) {
        int gpio_state = gpio_get_level(gpio_pin);

        if (gpio_state != last_gpio_state) {
            char data[16];
            if (gpio_state == 1) {
                printf("GPIO pin %d is ON\n", gpio_pin);
                snprintf(data, sizeof(data), "ONN%d", gpio_pin);
            } else {
                printf("GPIO pin %d is OFF\n", gpio_pin);
                snprintf(data, sizeof(data), "OFF%d", gpio_pin);
            }
            if(mode==1){
            	send_tcp_data(clientIPs, data);
            }
            else{
            	send_udp_broadcast(data);

            }
            last_gpio_state = gpio_state;

        }

        vTaskDelay(600 / portTICK_PERIOD_MS);
    }
}





void gpio_init(void){
	    gpio_config_t io_conf;
	    io_conf.intr_type = GPIO_INTR_DISABLE;  // Disable interrupts on these pins
	    io_conf.mode = GPIO_MODE_INPUT;             // Set the pins as inputs
	    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;    // Disable internal pull-up resistors
	    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;  // Disable internal pull-down resistors
	    io_conf.pin_bit_mask = (1ULL << GPIO_PIN_1) | (1ULL << GPIO_PIN_2) | (1ULL << GPIO_PIN_3) | (1ULL << GPIO_PIN_4);//| (1ULL << GPIO_PIN_2) | (1ULL << GPIO_PIN_3) | (1ULL << GPIO_PIN_4
	    gpio_config(&io_conf);

}

void get_device_id(int ret){
	   nvs_handle_t my_handle;
	    ret = nvs_open("wifi_config", NVS_READWRITE, &my_handle);
	    if (ret != ESP_OK) {
	        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(ret));
	    } else {
	        // Read the device ID from NVS
	        //char device_id[20]; // Assuming a device ID string of 19 characters or less
	        size_t required_size;
	        ret = nvs_get_str(my_handle,  "device", NULL, &required_size);
	        if (ret == ESP_OK) {
	            ret = nvs_get_str(my_handle,  "device", device_id, &required_size);
	            if (ret == ESP_OK) {
	                printf("Device ID: %s\n", device_id);
	            } else {
	                printf("Error (%s) reading device ID from NVS!\n", esp_err_to_name(ret));
	            }
	        } else {
	            printf("Error (%s) reading device ID size from NVS!\n", esp_err_to_name(ret));
	        }

	        // Close the NVS handle
	        nvs_close(my_handle);
	    }
}



void app_main(void)
{
    //Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();
    start_http_server();
    // // Delay for 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));
    start_wifi(ret);
    gpio_init();
    get_device_id(ret);

    if(mode==1){
    	xTaskCreate(udp_broadcast_task, "udp_broadcast_task", 4096, NULL, 5, NULL);
    	xTaskCreate(tcp_receive_task, "tcp_receive", 4096, NULL, 5, NULL);

    	vTaskDelay(pdMS_TO_TICKS(20000));
    }else{
    	//    send_tcp_data("OFF02");
    	ESP_LOGI(TAG, "starting broadcast mode........!");
    	vTaskDelay(pdMS_TO_TICKS(10000));
    	xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    }




    // Create tasks to monitor each GPIO pin
    last_gpio_state = gpio_get_level(GPIO_PIN_1);
    xTaskCreate(gpio_monitor_task, "GPIO Monitor 1", 2048, (void *)GPIO_PIN_1, 5, NULL);

    last_gpio_state = gpio_get_level(GPIO_PIN_2);
    xTaskCreate(gpio_monitor_task, "GPIO Monitor 2", 2048, (void *)GPIO_PIN_2, 5, NULL);

    last_gpio_state = gpio_get_level(GPIO_PIN_3);
    xTaskCreate(gpio_monitor_task, "GPIO Monitor 3", 2048, (void *)GPIO_PIN_3, 5, NULL);

    last_gpio_state = gpio_get_level(GPIO_PIN_4);
    xTaskCreate(gpio_monitor_task, "GPIO Monitor 4", 2048, (void *)GPIO_PIN_4, 5, NULL);

   }



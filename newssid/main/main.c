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
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_vfs_fat.h"
#include "driver/sdmmc_host.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <esp_idf_version.h>
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_tls.h"
#include <sys/socket.h>
#include <netdb.h>
#include "lwip/sockets.h"
//#include "lwip/netdb.h"
#include <lwip/netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>
#include "esp_http_server.h"

/* The examples use WiFi configuration that you can set via project configuration menu.

   If you'd rather not, just change the below entries to strings with
   the config you want - ie #define EXAMPLE_WIFI_SSID "mywifissid"
*/
#define EXAMPLE_WIFI_SSID      "MyESP32"
#define EXAMPLE_WIFI_PASS      ""
#define EXAMPLE_ESP_WIFI_CHANNEL   6
#define EXAMPLE_MAX_STA_CONN       5
#define WIFI_CONNECT_TIMEOUT 10

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
// Features supported in 4.1+
#define ESP_NETIF_SUPPORTED
#endif
#define EXAMPLE_ESP_MAXIMUM_RETRY  5



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

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
//static void udp_server_task(void *pvParameters);
static const char *TAG = "wifi softAP";
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
//const char* html_resp =
//    "<!DOCTYPE html>"
//    "<html>"
//    "<head>"
//    "    <title>Wi-Fi Configuration</title>"
//    "</head>"
//    "<body>"
//    "    <h1>Wi-Fi Credentials are submitted sucessffuly</h1>"
//    "</body>"
//    "</html>";

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

//#define PORT 6767

//static bool station_mode_enabled = true; // Flag to control mode switching
void stop_http_server(void)
{
    if (server != NULL) {
        httpd_stop(server);
        server = NULL;
    }
}
/* Cleanup function to release SoftAP-related resources */


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

//        vTaskDelay(1000 / portTICK_PERIOD_MS);

//         wifi_init_sta(SSID, PASS);



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
void wifi_connect_task(void *pvParameters) {
    int retries = 0;
    while (1) {
        if (retries < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "Connecting to Wi-Fi AP...");
            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdTRUE, pdFALSE, pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT * 1000));

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "Connected to Wi-Fi AP.");
                break; // Wi-Fi connected successfully, exit the loop.
            } else {
                ESP_LOGW(TAG, "Connection failed. Retrying...");
            }
            retries++;
        } else {
            // Failed to connect within the specified number of retries.
            ESP_LOGI(TAG, "Connection failed after %d retries. Switching to SoftAP mode.", retries);
            wifi_stop_softap(); // Stop the AP mode
            stop_http_server();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            wifi_init_softap(); // Switch to SoftAP mode
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            start_http_server();

            break;
        }
    }
    vTaskDelete(NULL);
}



void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_softap();
    start_http_server();



   // // Delay for 30 seconds
    vTaskDelay(pdMS_TO_TICKS(50000));

    // Stop the AP mode
//     vTaskDelay(1000 / portTICK_PERIOD_MS);

		char SSID[32];
		char PASS[64];
//		char device_name[64];
//		char colour[64];
//		char value[64];
       memset(SSID, 0, sizeof(SSID));
       memset(PASS, 0, sizeof(PASS));
//       memset(device_name, 0, sizeof(device_name));
//       memset(colour, 0, sizeof(colour));
//       memset(value, 0, sizeof(value));

       nvs_handle_t nvs_handle;
       ret = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
       if (ret == ESP_OK) {
           size_t ssid_len = sizeof(SSID);
           size_t pass_len = sizeof(PASS);
//           size_t device_name_len = sizeof(device_name);
//           size_t colour_len = sizeof(colour);
//           size_t value_len = sizeof(value);
           ret = nvs_get_str(nvs_handle, "ssid", SSID, &ssid_len);
           if (ret == ESP_OK) {
               ret = nvs_get_str(nvs_handle, "password", PASS, &pass_len);
//               if (ret == ESP_OK) {
//                             ret = nvs_get_str(nvs_handle, "device_name", device_name, &device_name_len);
//                             if (ret == ESP_OK) {
//                                           ret = nvs_get_str(nvs_handle, "colour", colour, &colour_len);
//                                           if (ret == ESP_OK) {
//                                                         ret = nvs_get_str(nvs_handle, "value", value, &value_len);
//                                                     }
//                                       }
//                         }
           }

           nvs_close(nvs_handle);
       }

       if (ret == ESP_OK && strlen(SSID) > 0 && strlen(PASS) > 0) {
               // Initialize and connect to Wi-Fi using stored credentials
               wifi_init_sta(SSID, PASS);
               xTaskCreate(wifi_connect_task, "wifi_connect_task", 4096, NULL, 5, NULL);
           } else {
               // No stored credentials, you may prompt the user to enter them
               printf("No stored credentials. Please set SSID and Password.\n");
           }




}

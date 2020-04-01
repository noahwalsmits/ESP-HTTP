#include "http_main.h"

/* The wifi can be set using idf.py menuconfig */
#define HTTP_WIFI_SSID CONFIG_WIFI_SSID
#define HTTP_WIFI_PASS CONFIG_WIFI_PASSWORD

static const char *TAG = "HTTP";

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
/* Bit used for the connection event */
const int EVENT_CONNECTED_BIT = BIT0;

/* Constants for the website to connect to */
#define WEB_SERVER "example.com"
#define WEB_PORT 80
#define WEB_URL "http://example.com/"

static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    "Host: "WEB_SERVER"\r\n"
    "User-Agent: esp-idf/1.0 esp32\r\n"
    "\r\n";

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    ESP_LOGI(TAG, "started");
    http_init_wifi();

    //const char * params = "http://example.com/ example.com"; //url + host
    xTaskCreate(&http_get_request_task, "http_get_request_task", 4096, NULL, 5, NULL);
}

/* initialises a wifi connection (from example) */
static void http_init_wifi() 
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = HTTP_WIFI_SSID,
            .password = HTTP_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

/* event handler for wifi connection (from example) */
static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        ESP_LOGI(TAG, "event handler: start");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, EVENT_CONNECTED_BIT);
        ESP_LOGI(TAG, "event handler: got IP");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, EVENT_CONNECTED_BIT);
        ESP_LOGI(TAG, "event handler: disconnected");
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* Does a http request and disconnects the wifi after completion
(mostly from example, exceptions marked with empty comment at end of line) */
static void http_get_request_task(void *pvParameters) 
{
    // char * parameters = (*((char*) pvParameters)); //
    // const char * WEB_URL = parameters[0]; //
    // const char * WEB_SERVER = parameters[1]; //
    // static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n" //
    //     "Host: "WEB_SERVER"\r\n" //
    //     "User-Agent: esp-idf/1.0 esp32\r\n" //
    //     "\r\n"; //

    const struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *result;
    struct in_addr *addr;
    int s, r;
    char recv_buf[64];

    while(1) {
        //Wait for connection event
        xEventGroupWaitBits(wifi_event_group, EVENT_CONNECTED_BIT, false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");

        //DNS lookup
        int err = getaddrinfo(WEB_SERVER, "80", &hints, &result);
        if(err != 0 || result == NULL) { //check if DNS lookup failed
            ESP_LOGE(TAG, "DNS lookup failed err=%d res=%p", err, result);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        addr = &((struct sockaddr_in *)result->ai_addr)->sin_addr;
        ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));

        //Socket allocation
        s = socket(result->ai_family, result->ai_socktype, 0);
        if(s < 0) { //check if socket allocation failed
            ESP_LOGE(TAG, "... Failed to allocate socket.");
            freeaddrinfo(result);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... allocated socket");

        //Connect to socket
        if(connect(s, result->ai_addr, result->ai_addrlen) != 0) { //check if connection failed
            ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
            close(s);
            freeaddrinfo(result);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... connected to socket");
        freeaddrinfo(result);

        //Send request to socket
        if (write(s, REQUEST, strlen(REQUEST)) < 0) { //check if sending failed
            ESP_LOGE(TAG, "... socket send failed");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... socket send success");

        //Set receiving timeout
        struct timeval receiving_timeout;
        receiving_timeout.tv_sec = 5;
        receiving_timeout.tv_usec = 0;
        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) { //check if setting failed
            ESP_LOGE(TAG, "... failed to set socket receiving timeout");
            close(s);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            continue;
        }
        ESP_LOGI(TAG, "... set socket receiving timeout success");
        ESP_LOGI(TAG, "HTTP response:");//

        //Read HTTP response
        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(s, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);

        ESP_LOGI(TAG, "... done reading from socket");
        close(s);
        ESP_LOGI(TAG, "deleting task and disconnecting");//
        ESP_ERROR_CHECK( esp_wifi_stop() );//
        vTaskDelete(NULL);//
    }
}
#ifndef HTTP_MAIN
#define HTTP_MAIN

#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

static void http_init_wifi();
static esp_err_t event_handler(void *ctx, system_event_t *event);
static void http_get_request_task(void *pvParameters);

#endif
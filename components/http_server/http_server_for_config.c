#include <stdio.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "nvs.h"
#include "cJSON.h"

#define TAG "http_server"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static esp_err_t root_get_handler(httpd_req_t *req)
{
    extern const uint8_t index_html_start[] asm("_binary_index_html_start");
    extern const uint8_t index_html_end[] asm("_binary_index_html_end");
    const size_t index_html_size = (index_html_end - index_html_start);
    httpd_resp_send(req, (const char *)index_html_start, index_html_size);
    return ESP_OK;
}

static esp_err_t read_get_handler(httpd_req_t *req)
{
    char param[32];
    char value[64];
    if (httpd_req_get_url_query_str(req, param, sizeof(param)) == ESP_OK)
    {
        char name[32];
        if (httpd_query_key_value(param, "name", name, sizeof(name)) == ESP_OK)
        {
            ESP_LOGI(TAG, "Found URL query parameter -> name=%s", name);
            if (nvs_read_string(name, value, "default", sizeof(value)) != 0)
            {
                ESP_LOGE(TAG, "Parameter %s not found", name);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
            httpd_resp_send(req, value, strlen(value));
            return ESP_OK;
        }
    }
    ESP_LOGE(TAG, "No URL query parameter found");
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

static esp_err_t write_post_handler(httpd_req_t *req)
{
    char buf[128];
    int ret, remaining = req->content_len;

    while (remaining > 0)
    {
        if ((ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf)))) <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            return ESP_FAIL;
        }
        remaining -= ret;
    }

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    const cJSON *name = cJSON_GetObjectItemCaseSensitive(json, "name");
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(json, "value");

    if (cJSON_IsString(name) && cJSON_IsString(value))
    {
        ESP_LOGI(TAG, "Writing parameter %s with value %s", name->valuestring, value->valuestring);
        if (nvs_write_string(name->valuestring, value->valuestring) != 0)
        {
            httpd_resp_send_500(req);
            cJSON_Delete(json);
            return ESP_FAIL;
        }
        httpd_resp_send(req, "Parameter written successfully", strlen("Parameter written successfully"));
    }
    else
    {
        httpd_resp_send_500(req);
    }

    cJSON_Delete(json);
    return ESP_OK;
}

httpd_handle_t start_http_server(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Register the root GET handler
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &root_uri);

        // Register the read GET handler
        httpd_uri_t read_uri = {
            .uri = "/read",
            .method = HTTP_GET,
            .handler = read_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &read_uri);

        // Register the write POST handler
        httpd_uri_t write_uri = {
            .uri = "/write",
            .method = HTTP_POST,
            .handler = write_post_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &write_uri);
    }
    else
    {
        ESP_LOGE(TAG, "Error starting server!");
        return NULL;
    }
    return server;
}

void stop_http_server(httpd_handle_t server)
{
    httpd_stop(server);
}
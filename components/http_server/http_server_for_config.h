#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <esp_err.h>
#include "esp_http_server.h"
// Function to initialize the HTTP server
esp_err_t http_server_init(void);

// Function to handle HTTP requests
void http_server_handle_request(void);

httpd_handle_t start_http_server(void);

#endif // HTTP_SERVER_H
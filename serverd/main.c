#include "cronos.h"

void echo_time(http_connection_t * conn)
{
    conn->response->ptr = malloc(1024);
    sprintf(conn->response->ptr, "{\"time\": %.f}", timems());
    http_send_response(conn);
}

BEGIN_ROUTES(web_routes)
    ROUTE_EX(HTTP_GET, "/ping", echo_time)
END_ROUTES

int main(int argc, char * argv)
{
    Config config;
    config.web_port = 9999;
    config.web_ip   = strdup("0.0.0.0");
    config.web_allow_origin = strdup("*");
    config.requests  = 0;

    webserver_init(&config, web_routes);
    uv_run(uv_default_loop(), UV_RUN_DEFAULT); /* main loop! */

    return 0;
}

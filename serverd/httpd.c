/**
 * Copyright (c) 2014, César Rodas <crodas@php.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Notifd nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "cronos.h"

static Config * config;
static uv_loop_t    * uv_loop;
static http_parser_settings parser_settings;
static long long process_id = 0;
static long long counter = 0;

/**
 *  Generate Unique ID that is used for each message,
 *  it was inspired by MongoID's
 */
void get_unique_id(char * uuid)
{
    struct timeval tp;
    long long time = (long long) timems();
    long long rnd0;

    gettimeofday(&tp, NULL);
    srand(tp.tv_usec);

    if (!process_id) {
        process_id = random() & 0xFFFFFF;
    }

    if (!counter || counter == 0xFFFFFE) {
        counter = random() & 0xFFFFFF;
    }

    sprintf(uuid,  "%08llx%06llx%04llx%06llx", 
        time & 0xFFFFFFFF, process_id, random() & 0xFFFF, ++counter);
}

double timems()
{
    struct timeval tp;
    if (gettimeofday(&tp, NULL)) {
        return (double)-1;
    }
    return (double)(tp.tv_sec) * 1000LL + (tp.tv_usec) / 1000;
}

void http_stream_on_alloc(uv_handle_t* client, size_t suggested_size, uv_buf_t* buf)
{
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void _conn_destroy(void * ptr)
{
    http_connection_t * conn = (http_connection_t *) ptr;
    zfree(conn->timeout);

    if (conn->request->data) {
        conn->request->free_data(conn);
    }

    if (conn->request) {
        dictRelease(conn->request->headers);
        zfree(conn->request->url);
        zfree(conn->request->uri);
        dictRelease(conn->request->args);
        zfree(conn->request);
    }
    if (conn->response) {
        dictRelease(conn->response->headers);
        zfree(conn->response->ptr);
        zfree(conn->response);
    }
    zfree(conn);
}

static void _timer_close_listener(uv_handle_t* handle)
{
    FETCH_CONNECTION_UV;
    DECREF(conn);
}

static void http_stream_on_close(uv_handle_t* handle)
{
    FETCH_CONNECTION_UV;
    if (conn->timeout) {
        uv_close((uv_handle_t*)conn->timeout, _timer_close_listener);
    }
    DECREF(conn);
}

void http_stream_on_read(uv_stream_t* handle, ssize_t nread, uv_buf_t* buf)
{
    size_t parsed;
    FETCH_CONNECTION_UV;

    if (nread >= 0) {
         parsed = http_parser_execute(&conn->parser, &parser_settings, buf->base, nread);

         if (parsed < nread) {
            uv_close((uv_handle_t*)&handle, http_stream_on_close);
         }
     } else {
        uv_close((uv_handle_t*) &conn->stream, http_stream_on_close);
     }
     zfree(buf->base);
}

static void on_connect(uv_stream_t* stream, int status)
{
    MALLOC(http_connection_t, conn);
    get_unique_id(conn->id); 
    conn->request   = NULL;
    conn->response  = NULL;
    conn->time      = timems();
    conn->routes    = (struct http_routes *)stream->data;
    conn->timeout   = NULL;
    conn->replied   = 0;
    REFERENCE_INIT(conn, _conn_destroy);

    uv_tcp_init(uv_loop, &conn->stream);
    http_parser_init(&conn->parser, HTTP_REQUEST);

    conn->write_req.data = conn;
    conn->parser.data = conn;
    conn->stream.data = conn;

    /* TODO: Use the return values from uv_accept() and
     * uv_read_start() */
    uv_accept(stream, (uv_stream_t*)&conn->stream);
    uv_read_start(
        (uv_stream_t*)&conn->stream,
        http_stream_on_alloc,
        http_stream_on_read
    );
}

WEB_SIMPLE_EVENT(message_begin)
{
    FETCH_CONNECTION;
    MALLOC_EX(http_request_t, conn->request);
    conn->request->headers = dictString(NULL);
    conn->request->data         = NULL;
    conn->request->free_data    = NULL;

    /* Presize the dict to avoid rehashing */
    dictExpand(conn->request->headers, 5);

    conn->last_was_value = 0;
    conn->current_header_key_length   = 0;
    conn->current_header_value_length = 0;
    return 0;
}

static void http_build_header_ptr(http_response_t * response)
{
    int i, l1, l2;
    sprintf(
        response->header_ptr, 
        "HTTP/%d.%d %d OK\r\n",
        response->http_major, response->http_minor, response->status
    );

    i = strlen(response->header_ptr);

    FOREACH(response->headers, key, value)
        l1 = strlen(key);
        l2 = strlen(value);
        if (i + 3 +  l1 + l2 >= 4098) break;
        strncat(response->header_ptr + i, key, l1);
        i += l1;
        strncat(response->header_ptr + i, ": ", 2);
        i += 2;
        strncat(response->header_ptr + i, value, l2);
        i += l2;
        strncat(response->header_ptr + i, "\r\n", 2);
        i += 2;
    END
    strncat(response->header_ptr + i, "\r\n", 2);

    response->header_len = i + 2;
}

static void http_server_sent_body(uv_write_t* handle, int status)
{
    FETCH_CONNECTION_UV
    uv_close((uv_handle_t*)handle->handle, http_stream_on_close);
}


static void http_server_sent_header(uv_write_t* handle, int status)
{
    FETCH_CONNECTION_UV
    uv_buf_t resbuf;
    dictEntry * de;

    assertWithInfo(conn->replied == 1);
    conn->replied = 2;

    if (!conn->response->ptr) {
        conn->response->ptr = strdup("");
    }


    resbuf = uv_buf_init(conn->response->ptr, strlen(conn->response->ptr));

    uv_write(&conn->write_req, (uv_stream_t*) &conn->stream, &resbuf, 1, http_server_sent_body);
}

int http_send_response(http_connection_t * conn)
{
    uv_buf_t resbuf;
    char time[90];

    assertWithInfo(conn->replied == 0);

    conn->replied = 1;
    sprintf(time, "%.f ms", timems() - conn->time);
    HEADER("X-Connection-Id", conn->id);
    HEADER("X-Response-Time", time);
    HEADER("Access-Control-Allow-Origin", config->web_allow_origin);

    http_build_header_ptr(conn->response);
    resbuf = uv_buf_init(conn->response->header_ptr, conn->response->header_len);

    uv_write(&conn->write_req, (uv_stream_t*) &conn->stream, &resbuf, 1, http_server_sent_header);

    return 0;
}

int check_ip(const char * ipstr, http_connection_t * conn)
{
    struct sockaddr_in ip_addr, compare_addr;
    size_t namelen = sizeof(struct sockaddr_in);

    uv_tcp_getpeername(&conn->stream, &ip_addr, &namelen);
    uv_ip4_addr(ipstr, 0, &compare_addr);
    
    if (compare_addr.sin_family == ip_addr.sin_family) {
        return memcmp(&ip_addr.sin_addr,
            &compare_addr.sin_addr,
            sizeof compare_addr.sin_addr) == 0;
    }

    return 0;
}

http_response_t * new_response(http_request_t * req)
{
    MALLOC(http_response_t, response);
    config->requests++;
    response->http_major    = req->http_major;
    response->http_minor    = req->http_minor;
    response->status        = 200;
    response->headers       = dictString(NULL);
    response->ptr           = NULL;
    return response;
}

WEB_SIMPLE_EVENT(message_complete)
{
    FETCH_CONNECTION;
    int found = 0;
    http_routes_t * routes = (http_routes_t *)conn->routes;


    conn->response = new_response(conn->request);

    HEADER("Content-Type", "application/json");
    HEADER("Connection", "Close")
    HEADER("X-Powered-By", "WebPubSub");

    int rlen = strlen(conn->request->uri);
    for (; routes->handler; routes++) {
        if (routes->method == HTTP_ANY || routes->method == conn->request->method) {
            int len = strlen(routes->path);
            int prefix = 0;

            if (routes->path[len-1] == '*') {
                prefix = 1;
                len--;
            } 
            
            if ((!prefix && len != rlen) || (prefix && len > rlen)) {
                /* they don't match */
                continue;
            }

            if (strncmp(conn->request->uri, routes->path, (size_t)len) == 0) {
                routes->handler( conn );
                found = 1;
                break;
            }
                
        }
    }

    if (!found) {
        http_send_response(conn);
    }

    return 0;
}

static void _requestSetHeader(http_connection_t * conn)
{
    if (conn->last_was_value && conn->current_header_key_length > 0) {
        TOUPPER(conn->current_header_key);
        TOLOWER(conn->current_header_value);
        dictAdd(conn->request->headers, conn->current_header_key, conn->current_header_value);
        conn->current_header_key_length = 0;
        conn->current_header_value_length = 0;
    }
}

WEB_EVENT(header_field) 
{
    FETCH_CONNECTION;

    _requestSetHeader(conn);

    memcpy((char *)&conn->current_header_key[conn->current_header_key_length], at, length);
    conn->current_header_key_length += length;
    conn->current_header_key[conn->current_header_key_length] = '\0';
    conn->last_was_value = 0;

    return 0;
}

WEB_EVENT(header_value) 
{
    FETCH_CONNECTION;
    if (!conn->last_was_value && conn->current_header_value_length > 0) {
        /* Start of a new header */
        conn->current_header_value_length = 0;
    }

    memcpy((char *)&conn->current_header_value[conn->current_header_value_length], at, length);
    conn->current_header_value_length += length;
    conn->current_header_value[conn->current_header_value_length] = '\0';
    conn->last_was_value = 1;

    return 0;
}

WEB_EVENT(headers_complete)
{
    FETCH_CONNECTION;
    _requestSetHeader(conn); 

    http_request_t * req =  conn->request;
    
    req->http_major = parser->http_major;
    req->http_minor = parser->http_minor;
    req->method     = parser->method;

    return 0;
}

void parse_query_string(dict * args, char * query)
{
    char * key = NULL, *value = NULL;
    char * skey, *svalue;

    while (*query) {
        if (key == NULL) {
            key = query;
        }
        switch (*query) {
        case '=':
            value = query+1;
            break;
        case '&':
            skey = strndup(key, value - key - 1);
            svalue = strndup(value, query - value);
            dictAdd(args, skey, svalue); 
            zfree(skey);
            zfree(svalue);
            key     = NULL;
            value   = NULL;
            break;
        }
        query++;
    }

    if (key) {
        if (!value) value = query;
        skey = strndup(key, value - key - 1);
        svalue = strndup(value, query - value);
        dictAdd(args, skey, svalue); 
        zfree(skey);
        zfree(svalue);
    }

    #if 0
    FOREACH(args, skey, svalue)
        printf("%s => %s\n", skey, svalue);
    END
    #endif
}

#define MESSAGE_CHECK_URL(u, variable, fn)                      \
do {                                                            \
  char ubuf[513];                                               \
                                                                \
  if ((u)->field_set & (1 << (fn))) {                           \
    memcpy(ubuf, url + (u)->field_data[(fn)].off,               \
      MIN(512, (u)->field_data[(fn)].len));                     \
    ubuf[(u)->field_data[(fn)].len] = '\0';                     \
  } else {                                                      \
    ubuf[0] = '\0';                                             \
  }                                                             \
                                                                \
  if (fn == UF_QUERY) {                                         \
    variable = dictString(NULL);                                \
    parse_query_string(variable, ubuf);                         \
  } else {                                                      \
    variable = strdup(ubuf);                                    \
  }                                                             \
} while(0)



WEB_EVENT(url)
{
    FETCH_CONNECTION;

    struct http_parser_url u;

    char *url = (char *)malloc(sizeof(char) * length + 1);
    strncpy(url, at, length);
    url[length] = '\0';

    if (http_parser_parse_url(at, length, 0, &u) != 0) {
        fprintf(stderr, "\n\n*** failed to parse URL %s ***\n\n", url);
        zfree(url);
        uv_close((uv_handle_t*) &conn->stream, http_stream_on_close);
        return -1;
    }

    conn->request->url = url;

    MESSAGE_CHECK_URL(&u, conn->request->uri, UF_PATH);
    MESSAGE_CHECK_URL(&u, conn->request->args, UF_QUERY);

    return 0;
}

WEB_EVENT(body)
{
    FETCH_CONNECTION;
    if (length > 0) {
        http_request_t * req;
        req = conn->request;

        // we do not care about $_POST
        uv_close((uv_handle_t*)conn->timeout, _timer_close_listener);
    }
}

int webserver_init(Config * _config, http_routes_t * routes)
{
    DECLARE_WEB_EVENT(header_field);
    DECLARE_WEB_EVENT(header_value);
    DECLARE_WEB_EVENT(headers_complete);
    DECLARE_WEB_EVENT(message_begin);
    DECLARE_WEB_EVENT(message_complete);
    DECLARE_WEB_EVENT(body);
    DECLARE_WEB_EVENT(url);

    config = _config;


    #ifdef PLATFORM_POSIX
    signal(SIGPIPE, SIG_IGN);
    #endif // PLATFORM_POSIX

    uv_loop = uv_default_loop();
    uv_tcp_init(uv_loop, &config->web_server);
    uv_ip4_addr(config->web_ip, config->web_port, &config->web_bind);
    config->web_server.data = (void *)routes;

    uv_tcp_bind(&config->web_server, (struct sockaddr *)&config->web_bind, 0);
    uv_listen((uv_stream_t*)&config->web_server, 128, on_connect);
}


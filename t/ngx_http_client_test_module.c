#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include "../ngx_client.h"


static char *ngx_http_client_test(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);


static ngx_command_t  ngx_http_client_test_commands[] = {

    { ngx_string("http_client_test"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_client_test,
      0,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_client_test_module_ctx = {
    NULL,                                   /* preconfiguration */
    NULL,                                   /* postconfiguration */

    NULL,                                   /* create main configuration */
    NULL,                                   /* init main configuration */

    NULL,                                   /* create server configuration */
    NULL,                                   /* merge server configuration */

    NULL,                                   /* create location configuration */
    NULL                                    /* merge location configuration */
};


ngx_module_t  ngx_http_client_test_module = {
    NGX_MODULE_V1,
    &ngx_http_client_test_module_ctx,       /* module context */
    ngx_http_client_test_commands,          /* module directives */
    NGX_HTTP_MODULE,                        /* module type */
    NULL,                                   /* init master */
    NULL,                                   /* init module */
    NULL,                                   /* init process */
    NULL,                                   /* init thread */
    NULL,                                   /* exit thread */
    NULL,                                   /* exit process */
    NULL,                                   /* exit master */
    NGX_MODULE_V1_PADDING
};


static void
ngx_http_client_test_connected(ngx_client_session_t *s)
{
    ngx_buf_t                  *b;
    size_t                      len;
    ngx_chain_t                 out;
    ngx_http_request_t         *r;
    ngx_event_t                *wev;

    ngx_log_error(NGX_LOG_ERR, s->log, 0, "client connected");

    r = s->data;
    wev = s->peer.connection->write;

    len = sizeof("nginx client test\n") - 1;
    b = ngx_create_temp_buf(s->pool, len);

    if (b == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    b->last = ngx_copy(b->last, "nginx client test\n", len);
    b->last_buf = 1;

    out.buf = b;
    out.next = NULL;

    ngx_client_write(s, &out);

    ngx_handle_write_event(wev, 0);
}

static void
ngx_http_client_test_recv(ngx_client_session_t *s)
{
    ngx_buf_t                  *b;
    ngx_int_t                   n;
    ngx_connection_t           *c;
    ngx_str_t                   recv;
    ngx_http_request_t         *r;

    c = s->peer.connection;
    r = s->data;

    b = ngx_create_temp_buf(s->pool, 4096);
    if (b == NULL) {
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        return;
    }

    n = c->recv(c, b->pos, b->end - b->last);
    if (n == NGX_AGAIN) {
        ngx_log_error(NGX_LOG_ERR, s->log, 0, "client recv NGX_AGAIN");
        return;
    }

    if (n == NGX_ERROR || n == 0) {
        ngx_log_error(NGX_LOG_ERR, s->log, 0, "client recv NGX_ERROR");
        ngx_http_finalize_request(r, NGX_HTTP_INTERNAL_SERVER_ERROR);
        ngx_client_close(s);
        return;
    }

    b->last += n;

    recv.data = b->pos;
    recv.len = b->last - b->pos;

    ngx_log_error(NGX_LOG_ERR, s->log, 0, "client recv %d: %V, %z",
            n, &recv, recv.len);

    ngx_http_finalize_request(r, NGX_HTTP_FORBIDDEN);
    ngx_client_close(s);
    return;
}

static void
ngx_http_client_test_send(ngx_client_session_t *s)
{
    ngx_log_error(NGX_LOG_ERR, s->log, 0, "client send");
}

static void
ngx_http_client_test_closed(ngx_client_session_t *s)
{
    ngx_log_error(NGX_LOG_ERR, s->log, 0, "client closed");
}

static ngx_int_t
ngx_http_client_test_handler(ngx_http_request_t *r)
{
    ngx_client_session_t           *s;
    ngx_client_init_t              *ci;
    ngx_str_t                       echo;

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0, "client test handler");

    if (ngx_http_arg(r, (u_char *) "echo", sizeof("echo") - 1, &echo)
            != NGX_OK)
    {
        return NGX_HTTP_BAD_REQUEST;
    }

    ci = ngx_pcalloc(r->pool, sizeof(ngx_client_init_t));
    ci->server.len = sizeof("127.0.0.1:10000") - 1;
    ci->server.data = ngx_pcalloc(r->pool, ci->server.len);
    ngx_memcpy(ci->server.data, "127.0.0.1:10000", ci->server.len);
    //ci->timeout = 2000;
    //ci->max_retries = 3;
    //ci->type = SOCK_STREAM;
    //ci->recvbuf = 4096;
    ci->log = r->connection->log;

    ci->connected = ngx_http_client_test_connected;
    ci->recv = ngx_http_client_test_recv;
    ci->send = ngx_http_client_test_send;
    ci->closed = ngx_http_client_test_closed;

    s = ngx_client_connect(ci);
    if (s == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    s->data = r;

    ++r->count;

    return NGX_DONE;
}


static char *
ngx_http_client_test(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_client_test_handler;

    return NGX_CONF_OK;
}
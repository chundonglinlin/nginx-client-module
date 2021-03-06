/*
 * Copyright (C) AlexWoo(Wu Jie) wj19840501@gmail.com
 */


#include "ngx_http_client.h"
#include "ngx_rbuf.h"


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
ngx_http_client_test_recv_body(void *request, ngx_http_request_t *hcr)
{
    ngx_http_request_t             *r;
    ngx_http_client_ctx_t          *ctx;
    ngx_chain_t                    *cl = NULL;
    ngx_chain_t                   **ll;
    ngx_int_t                       rc;

    r = request;
    ctx = hcr->ctx[0];

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
            "http client test recv body");

    rc = ngx_http_client_read_body(hcr, &cl, 4096);

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
            "http client test recv body, rc %i %i, %O",
            rc, ngx_errno, ctx->rbytes);

    if (rc == 0) {
        goto done;
    }

    if (rc == NGX_ERROR) {
        goto done;
    }

    if (rc == NGX_DONE) {
        for (ll = &cl; (*ll)->next; ll = &(*ll)->next);

        (*ll)->buf->last_buf = 1;

        rc = NGX_OK;
    }

    ngx_http_output_filter(r, cl);
    goto ok;

done:
    ngx_http_finalize_request(r, rc);
    ngx_http_client_finalize_request(hcr, 1);

ok:
    ngx_put_chainbufs(cl);
    ngx_http_run_posted_requests(r->connection);
}

static void
ngx_http_client_test_recv(void *request, ngx_http_request_t *hcr)
{
    ngx_http_request_t             *r;
    ngx_http_client_ctx_t          *ctx;
    static ngx_str_t                content_type = ngx_string("Content-Type");
    static ngx_str_t                connection = ngx_string("Connection");
    static ngx_str_t                unknown = ngx_string("Unknown");
    ngx_str_t                      *ct, *con;

    r = request;
    ctx = hcr->ctx[0];

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "http client test recv");

    r->headers_out.status = 200;

    ctx->read_handler = ngx_http_client_test_recv_body;

    ngx_http_send_header(r);

    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
            "status_code: %ui   http_version: %ui",
            ngx_http_client_status_code(hcr),
            ngx_http_client_http_version(hcr));

    ct = ngx_http_client_header_in(hcr, &content_type);
    con = ngx_http_client_header_in(hcr, &connection);
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Content-Type: %V", ct);
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "Connection: %V", con);

    if (ngx_http_client_header_in(hcr, &unknown) == NULL) {
        ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, "no header Unknown");
    }

    ngx_http_client_test_recv_body(request, hcr);
}


static ngx_int_t
ngx_http_client_test_handler(ngx_http_request_t *r)
{
    ngx_http_request_t         *hcr;
    static ngx_str_t            request_url = ngx_string("http://127.0.0.1/");

    ngx_log_error(NGX_LOG_ERR, r->connection->log, 0,
            "http client test handler");

    hcr = ngx_http_client_create_request(&request_url, NGX_HTTP_CLIENT_GET,
            NGX_HTTP_CLIENT_VERSION_11, NULL, r->connection->log,
            ngx_http_client_test_recv, NULL);

    if (ngx_http_client_send(hcr, NULL, r, r->connection->log) != NGX_OK) {
        return NGX_HTTP_BAD_GATEWAY;
    }

    ++r->count;

    return NGX_DONE;
}


static char *
ngx_http_client_test(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t   *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_client_test_handler;

    return NGX_CONF_OK;
}

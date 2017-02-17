#include <zcoap.h>
#include <iostream>
#include <string>

int main(int argc, char **argv) {
    std::cout << "Service is started" << std::endl;
    ZCoap *_coap = new ZCoap();
    coap_context_t  *ctx;
    char *group = NULL;
    fd_set readfds;
    struct timeval tv, *timeout;
    int result;
    coap_tick_t now;
    coap_queue_t *nextpdu;
    char addr_str[NI_MAXHOST] = "::";
    char port_str[NI_MAXSERV] = "5683";
    int opt;
    coap_log_t log_level = LOG_WARNING;
    clock_offset = time(NULL);

    while ((opt = getopt(argc, argv, "A:g:p:v:")) != -1) {
        switch (opt) {
        case 'A' :
            strncpy(addr_str, optarg, NI_MAXHOST-1);
            addr_str[NI_MAXHOST - 1] = '\0';
            break;
        case 'g' :
            group = optarg;
            break;
        case 'p' :
            strncpy(port_str, optarg, NI_MAXSERV-1);
            port_str[NI_MAXSERV - 1] = '\0';
            break;
        case 'v' :
            break;
        default:
            exit( 1 );
        }
    }

    coap_set_log_level(log_level);

    ctx = _coap->get_context(addr_str, port_str);
    if (!ctx)
        return -1;

    _coap->init_resources(ctx);

    /* join multicast group if requested at command line */
    if (group) {
        _coap->join(ctx, group);
    }
    signal(SIGINT, _coap->handle_sigint);

    while ( !quit ) {
        FD_ZERO(&readfds);
        FD_SET( ctx->sockfd, &readfds );

        nextpdu = coap_peek_next( ctx );

        coap_ticks(&now);
        while (nextpdu && nextpdu->t <= now - ctx->sendqueue_basetime) {
            coap_retransmit( ctx, coap_pop_next( ctx ) );
            nextpdu = coap_peek_next( ctx );
        }

        if ( nextpdu && nextpdu->t <= COAP_RESOURCE_CHECK_TIME ) {
            /* set timeout if there is a pdu to send before our automatic timeout occurs */
            tv.tv_usec = ((nextpdu->t) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
            tv.tv_sec = (nextpdu->t) / COAP_TICKS_PER_SECOND;
            timeout = &tv;
        } else {
            tv.tv_usec = 0;
            tv.tv_sec = COAP_RESOURCE_CHECK_TIME;
            timeout = &tv;
        }
        result = select( FD_SETSIZE, &readfds, 0, 0, timeout );

        if ( result < 0 ) {         /* error */
            if (errno != EINTR)
                perror("select");
        } else if ( result > 0 ) {  /* read from socket */
            if ( FD_ISSET( ctx->sockfd, &readfds ) ) {
                coap_read( ctx );       /* read received data */
                /* coap_dispatch( ctx );  /\* and dispatch PDUs from receivequeue *\/ */
            }
        } else {      /* timeout */
            if (time_resource) {
                time_resource->dirty = 1;
            }
        }

#ifndef WITHOUT_ASYNC
        /* check if we have to send asynchronous responses */
        _coap->check_async(ctx, ctx->endpoint, now);
#endif /* WITHOUT_ASYNC */

#ifndef WITHOUT_OBSERVE
        /* check if we have to send observe notifications */
        coap_check_notify(ctx);
#endif /* WITHOUT_OBSERVE */
    }

    coap_free_context(ctx);

    return 0;
}

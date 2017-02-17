#include "zcoap.h"
#include <zmqtt.h>
class ZCoap::Impl {
public:
    Impl() {
        init_mqtt();
    }
public:
    static void handle_sigint(int signum) {
        quit = 1;
    }
    static void check_async(coap_context_t *ctx,
                const coap_endpoint_t *local_if,
                coap_tick_t now) {
        coap_pdu_t *response;
        coap_async_state_t *tmp;

        size_t size = sizeof(coap_hdr_t) + 13;

        if (!async || now < async->created + (unsigned long)async->appdata)
            return;

        response = coap_pdu_init(async->flags & COAP_ASYNC_CONFIRM
                                 ? COAP_MESSAGE_CON
                                 : COAP_MESSAGE_NON,
                                 COAP_RESPONSE_CODE(205), 0, size);
        if (!response) {
            debug("check_async: insufficient memory, we'll try later\n");
            async->appdata =
                    (void *)((unsigned long)async->appdata + 15 * COAP_TICKS_PER_SECOND);
            return;
        }

        response->hdr->id = coap_new_message_id(ctx);

        if (async->tokenlen)
            coap_add_token(response, async->tokenlen, async->token);

        coap_add_data(response, 4, (unsigned char *)"done");

        if (coap_send(ctx, local_if, &async->peer, response) == COAP_INVALID_TID) {
            debug("check_async: cannot send response for message %d\n",
                  response->hdr->id);
        }
        coap_delete_pdu(response);
        coap_remove_async(ctx, async->id, &tmp);
        coap_free_async(async);
        async = NULL;
    }

    void init_mqtt() {
        mqtt = new ZMqtt(mqtt_clientId, mqtt_host, mqtt_port);
        mqtt->preSubscribe(mqtt_topic, 0);
        mqtt->autoReconnect(true);
        mqtt->beginConnect();
        mqtt->connect();
    }

    void post_to_mqtt(const unsigned char * buffer_payload) {
        std::string msg((char*)buffer_payload);
        mqtt->publish(mqtt_topic, msg);
    }

    static void hnd_post_esp32(coap_context_t *ctx ,
                 struct coap_resource_t *resource ,
                 const coap_endpoint_t *local_interface ,
                 coap_address_t *peer ,
                 coap_pdu_t *request,
                 str *token ,
                 coap_pdu_t *response) {
        coap_tick_t t;
        size_t size;
        unsigned char *data;
        response->hdr->code =
                my_clock_base ? COAP_RESPONSE_CODE(204) : COAP_RESPONSE_CODE(201);

        resource->dirty = 1;
        coap_get_data(request, &size, &data);

        if(size > 0) {
            //push data to mqtt server
            Impl _this;
            _this.post_to_mqtt(data);
        }
    }

    static void init_resources(coap_context_t *ctx) {
        coap_resource_t *r;
        /* store clock base to use in /time */
        my_clock_base = clock_offset;
        r = coap_resource_init((unsigned char *)"esp32", 5, COAP_RESOURCE_FLAGS_NOTIFY_CON);
        coap_register_handler(r, COAP_REQUEST_POST, hnd_post_esp32);
        coap_add_resource(ctx, r);
        time_resource = r;
    }

    static void usage( const char *program, const char *version) {
        const char *p;
        p = strrchr( program, '/' );
        if ( p )
            program = ++p;

        fprintf( stderr, "%s v%s -- a small CoAP implementation\n"
                         "(c) 2010,2011,2015 Olaf Bergmann <bergmann@tzi.org>\n\n"
                         "usage: %s [-A address] [-p port]\n\n"
                         "\t-A address\tinterface address to bind to\n"
                         "\t-g group\tjoin the given multicast group\n"
                         "\t-p port\t\tlisten on specified port\n"
                         "\t-v num\t\tverbosity level (default: 3)\n",
                 program, version, program );
    }

    static coap_context_t * get_context(const char *node, const char *port) {
        coap_context_t *ctx = NULL;
        int s;
        struct addrinfo hints;
        struct addrinfo *result, *rp;

        memset(&hints, 0, sizeof(struct addrinfo));
        hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
        hints.ai_socktype = SOCK_DGRAM; /* Coap uses UDP */
        hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

        s = getaddrinfo(node, port, &hints, &result);
        if ( s != 0 ) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
            return NULL;
        }

        /* iterate through results until success */
        for (rp = result; rp != NULL; rp = rp->ai_next) {
            coap_address_t addr;

            if (rp->ai_addrlen <= sizeof(addr.addr)) {
                coap_address_init(&addr);
                addr.size = rp->ai_addrlen;
                memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);

                ctx = coap_new_context(&addr);
                if (ctx) {
                    /* TODO: output address:port for successful binding */
                    goto finish;
                }
            }
        }

        fprintf(stderr, "no context available for interface '%s'\n", node);

    finish:
        freeaddrinfo(result);
        return ctx;
    }

    static int join(coap_context_t *ctx, char *group_name){
        struct ipv6_mreq mreq;
        struct addrinfo   *reslocal = NULL, *resmulti = NULL, hints, *ainfo;
        int result = -1;

        /* we have to resolve the link-local interface to get the interface id */
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_DGRAM;

        result = getaddrinfo("::", NULL, &hints, &reslocal);
        if (result < 0) {
            fprintf(stderr, "join: cannot resolve link-local interface: %s\n",
                    gai_strerror(result));
            goto finish;
        }

        /* get the first suitable interface identifier */
        for (ainfo = reslocal; ainfo != NULL; ainfo = ainfo->ai_next) {
            if (ainfo->ai_family == AF_INET6) {
                mreq.ipv6mr_interface =
                        ((struct sockaddr_in6 *)ainfo->ai_addr)->sin6_scope_id;
                break;
            }
        }

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET6;
        hints.ai_socktype = SOCK_DGRAM;

        /* resolve the multicast group address */
        result = getaddrinfo(group_name, NULL, &hints, &resmulti);

        if (result < 0) {
            fprintf(stderr, "join: cannot resolve multicast address: %s\n",
                    gai_strerror(result));
            goto finish;
        }

        for (ainfo = resmulti; ainfo != NULL; ainfo = ainfo->ai_next) {
            if (ainfo->ai_family == AF_INET6) {
                mreq.ipv6mr_multiaddr =
                        ((struct sockaddr_in6 *)ainfo->ai_addr)->sin6_addr;
                break;
            }
        }

        result = setsockopt(ctx->sockfd, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                            (char *)&mreq, sizeof(mreq));
        if (result < 0)
            perror("join: setsockopt");

    finish:
        freeaddrinfo(resmulti);
        freeaddrinfo(reslocal);

        return result;
    }
private:
    ZMqtt * mqtt;
    std::string mqtt_clientId = "";
    std::string mqtt_host = "localhost";
    int mqtt_port = 1883;
    std::string mqtt_topic = "esp32";
};

ZCoap::ZCoap() :d_ptr(new Impl)
{
}

coap_context_t * ZCoap::get_context(const char *node, const char *port) {
    return d_ptr->get_context(node, port);
}

void ZCoap::init_resources(coap_context_t *ctx) {
    d_ptr->init_resources(ctx);
}

int ZCoap::join(coap_context_t *ctx, char *group_name) {
    return d_ptr->join(ctx, group_name);
}

void ZCoap::handle_sigint(int signum) {
    ZCoap::Impl::handle_sigint(signum);
}

void ZCoap::check_async(coap_context_t *ctx,
                const coap_endpoint_t *local_if,
                coap_tick_t now) {
    d_ptr->check_async(ctx,local_if, now);
}
void ZCoap::init_mqtt() {
    d_ptr->init_mqtt();
}

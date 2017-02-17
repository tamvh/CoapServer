#ifndef ZCOAP_H
#define ZCOAP_H

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <memory>
#include <include/coap/coap.h>

#define COAP_RESOURCE_CHECK_TIME 2
#define INDEX "IOT LAB"

static int quit = 0;
static time_t clock_offset;
static time_t my_clock_base = 0;
static coap_resource_t *time_resource = NULL;
static coap_async_state_t *async = NULL;
class ZCoap
{
public:
    ZCoap();
    virtual ~ZCoap() {}
public:
    coap_context_t * get_context(const char *node, const char *port);
    void init_resources(coap_context_t *ctx);
    int join(coap_context_t *ctx, char *group_name);
    static void handle_sigint(int signum);
    void check_async(coap_context_t *ctx,
                    const coap_endpoint_t *local_if,
                    coap_tick_t now);
    void init_mqtt();
private:
    class Impl;
    std::shared_ptr<Impl> d_ptr;
};

#endif // ZCOAP_H

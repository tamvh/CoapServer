#ifndef ZMQTT_H
#define ZMQTT_H
#include <vector>
#include <string>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <string.h>
#include <sys/param.h>
extern "C" {
    #include "MQTTClient.h"
    #include "MQTTClientPersistence.h"
}
class ZMqtt
{
public:
    ZMqtt();
    ZMqtt(const std::string& clientId,
          const std::string& addr,
          int port);
    virtual ~ZMqtt() {}
public:
    virtual void onConnack(uint8_t retCode);
    void onMsg(std::string& topic, std::string& msg);

    void preSubscribe(std::string& topic, int qos);
    void autoReconnect(bool value) {}
    void beginConnect() {}
    void connect();

    bool isConnected();
    bool publish(const std::string& topic, const std::string& message);
protected:
    static void mqttDelivered(void *context, int dt);
    static int mqttMessageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
    static void mqttConnectionLost(void *context, char *cause);
private:
    std::string clientId_;
    std::string hostBroker_;
    int portBroker_;
    std::vector<std::string> topicList_;
    std::vector<uint8_t> qosList_;
    MQTTClient client_;
    bool isConnected_;
};
#endif // ZMQTT_H

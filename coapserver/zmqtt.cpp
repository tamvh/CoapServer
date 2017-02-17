#include "zmqtt.h"
using namespace std;
ZMqtt::ZMqtt()
{
}
ZMqtt::ZMqtt(
        const std::string& clientId,
        const std::string& hostBroker,
        int portBroker) :
        clientId_(clientId),
        hostBroker_(hostBroker),
        portBroker_(portBroker),
        isConnected_(false) {
    std::cout << "Creating mqtt client" << endl;
    std::string serverURI = "tcp://" + hostBroker + ":" + std::to_string(portBroker);
    MQTTClient_create(&client_, serverURI.c_str(), clientId_.c_str(),
            MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(
            client_,
            this,
            &ZMqtt::mqttConnectionLost,
            &ZMqtt::mqttMessageArrived,
            &ZMqtt::mqttDelivered);
}

void ZMqtt::mqttDelivered(void *context, int dt) {
    ZMqtt *this_ = static_cast<ZMqtt*> (context);
    if (!this_) {
        return;
    }
    std::cout << "Message delivered:" << dt << endl;
}

int ZMqtt::mqttMessageArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    ZMqtt *this_ = static_cast<ZMqtt*> (context);
    if (!this_) {
        return 0;
    }

    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = (char*) message->payload;
    for (i = 0; i < message->payloadlen; i++) {
        putchar(*payloadptr++);
    }
    putchar('\n');
    std::string topic(topicName);
    std::string msg((char*) message->payload, message->payloadlen);
    this_->onMsg(topic, msg);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void ZMqtt::mqttConnectionLost(void *context, char *cause) {
    std::cout << "\nConnection lost\n";
    std::cout << "     cause: " << cause << std::endl;
}

bool ZMqtt::isConnected() {
    return MQTTClient_isConnected(client_);
}

void ZMqtt::onConnack(uint8_t retCode) {
    std::cout << "onConnack" << std::endl;
    if (!topicList_.empty() && !qosList_.empty()) {
        std::cout << "Subscribe to topic: " << topicList_.front() << std::endl;
        for (uint32_t i = 0; i < topicList_.size(); i++) {
            std::string topic = topicList_[i];
            int qos = qosList_[i];
            MQTTClient_subscribe(client_, topic.c_str(), qos);
        }
    }
}

void ZMqtt::onMsg(std::string& topic, std::string& msg) {
    cout << "Topic: " << topic << ": " << msg << endl;
}

void ZMqtt::preSubscribe(std::string& topic, int qos) {
    if (isConnected()) {
        MQTTClient_subscribe(client_, topic.c_str(), qos);
    } else {
        topicList_.push_back(topic);
        qosList_.push_back(qos);
    }
}

void ZMqtt::connect() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    conn_opts.keepAliveInterval = 60;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client_, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }

    isConnected_ = true;
    onConnack(0);
}

bool ZMqtt::publish(const std::string& topic, const std::string& message) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload = (void *) message.c_str();
    pubmsg.payloadlen = message.length();
    pubmsg.qos = 0;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client_, topic.c_str(), &pubmsg, &token);
    return token;
}

#ifndef ZIGBEESERVER_H
#define ZIGBEESERVER_H

#include <vector>
#include <string>
#include <queue>
#include <functional>
#include "HardwareSerial.h"
#include <algorithm>
#include <sstream>
// #include <iomanip>

struct Device {
    std::string id;
    std::string status;
};

class ZigbeeServer
{
public:
    ZigbeeServer();
    void begin();
    void loop();
    void addDevice(const char *id);
    void onMessage(std::function<void(const char *id, const char *data)> callback);
    void onChange(std::function<void()> callback);
    static ZigbeeServer* getInstance();
    void checkDevice(const char *id);
    void sendCommand(const char *id, const char *cmd);
    void sendCommand(const char *id, const char *secrect_key, const char *cmd);
    void broadcastMessage();

    std::vector<Device> deviceList;

private:
    void initZigbee();
    void handleIncomingMessage(const std::string& message);
    HardwareSerial *_zigbeeSerial;

    static ZigbeeServer *_instance;
    std::queue<std::string> messageQueue;
    std::function<void(const char *id, const char *data)> messageCallback;
    std::function<void()> onChangeCallback;
};

#endif // ZIGBEESERVER_H

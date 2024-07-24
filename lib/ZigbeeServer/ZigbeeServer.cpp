#include "ZigbeeServer.h"
#include <algorithm> // Thêm dòng này để sử dụng std::find_if

ZigbeeServer* ZigbeeServer::_instance = nullptr;

ZigbeeServer::ZigbeeServer() : _zigbeeSerial(&Serial1)
{
}

void ZigbeeServer::begin() {
    ESP_LOGI("ZigbeeServer", "Starting...");
    initZigbee();
    broadcastMessage();
    xTaskCreatePinnedToCore(
        [](void *pvParameters)
        {
            ZigbeeServer *zigbeeServer = static_cast<ZigbeeServer *>(pvParameters);
            for (;;)
            {
                zigbeeServer->loop();
                vTaskDelay(10 / portTICK_PERIOD_MS); // Giảm tần suất kiểm tra
            }
        },
        "ZigbeeServerTask",
        10000,
        this,
        1,
        NULL,
        0 // Chạy trên core 0
    );
}

void ZigbeeServer::loop() {
    static std::string incomingMessage; // Thêm static để lưu trữ tạm thời dữ liệu nhận được
    while (_zigbeeSerial->available()) {
        char c = _zigbeeSerial->read();
        incomingMessage += c;
        if (c == '\n') {
            if (!incomingMessage.empty()) {
                handleIncomingMessage(incomingMessage);
                ESP_LOGI("ZigbeeServer", "Received: %s", incomingMessage.c_str());
                incomingMessage.clear();
            }
        }
    }
    while (!messageQueue.empty()) {
        std::string command = messageQueue.front();
        messageQueue.pop();
        _zigbeeSerial->println(command.c_str());
    }
}

void ZigbeeServer::addDevice(const char *id) {
    Device device;
    device.id = id;
    deviceList.push_back(device);
}

void ZigbeeServer::onMessage(std::function<void(const char *id, const char *data)> callback) {
    messageCallback = callback;
}

void ZigbeeServer::onChange(std::function<void()> callback) {
    onChangeCallback = callback;
}

ZigbeeServer* ZigbeeServer::getInstance() {
    if (_instance == nullptr) {
        _instance = new ZigbeeServer();
    }
    return _instance;
}

void ZigbeeServer::checkDevice(const char *id) {
    sendCommand(id, "CHECK");
}

void ZigbeeServer::sendCommand(const char *id, const char *cmd) {
    std::string message = std::string("ID:") + id + ",CMD:" + cmd;
    messageQueue.push(message);
}

void ZigbeeServer::broadcastMessage() {
    _zigbeeSerial->println("BRD:DISC");
}

void ZigbeeServer::initZigbee() {
    _zigbeeSerial->begin(9600, SERIAL_8N1, 16, 17); // Thay đổi RX_PIN và TX_PIN theo cấu hình của bạn
}

void ZigbeeServer::handleIncomingMessage(const std::string& message) {
    size_t pos = message.find(",DATA:");
    if (pos != std::string::npos) {
        std::string id = message.substr(3, pos - 3); // Skip "ID:"
        std::string data = message.substr(pos + 6); // Skip ",DATA:"
        
        // Kiểm tra xem id có tồn tại trong deviceList hay không
        auto it = std::find_if(deviceList.begin(), deviceList.end(), [&id](const Device& device) {
            return device.id == id;
        });

        if (it == deviceList.end()) {
            // Nếu không, thêm mới vào deviceList
            addDevice(id.c_str());
            if(onChangeCallback) {
                onChangeCallback();
            }
        } else {
            if (messageCallback) {
                messageCallback(id.c_str(), data.c_str());
            }
        }
    } 
}

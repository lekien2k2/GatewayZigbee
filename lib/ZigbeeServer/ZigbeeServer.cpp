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

void ZigbeeServer::sendCommand(const char *id, const char *secrect_key, const char *cmd) {
    std::string message = std::string("ID:") + id +",SECRECT_KEY:"+ secrect_key +",CMD:" + cmd;
    messageQueue.push(message);
}

void ZigbeeServer::broadcastMessage() {
    _zigbeeSerial->println("CMD:BRD:DISC");
}

void ZigbeeServer::initZigbee() {
    _zigbeeSerial->begin(9600, SERIAL_8N1, 16, 17); // Thay đổi RX_PIN và TX_PIN theo cấu hình của bạn
    // _zigbeeSerial->println("AT+ZSET:ROLE=COORD");
    // delay(1000);
    // _zigbeeSerial->println("AT+PANID=1234");
    // delay(1000);
    // _zigbeeSerial->println("AT+START");

    
}
uint32_t calculateCRC32(const char* data, size_t length) {
    uint32_t crc = 0xffffffff;
    while (length--) {
        uint8_t c = *data++;
        for (uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if (c & i) {
                bit = !bit;
            }
            crc <<= 1;
            if (bit) {
                crc ^= 0x04c11db7;
            }
        }
    }
    return ~crc;
}

bool checkCRC32(const std::string& data_with_crc) {
    size_t pos = data_with_crc.rfind(",CRC:");
    if (pos == std::string::npos) {
        return false;
    }

    std::string data = data_with_crc.substr(0, pos);
    std::string received_crc = data_with_crc.substr(pos + 5);

    uint32_t calculated_crc = calculateCRC32(data.c_str(), data.length());
    
    char crcString[9];
    snprintf(crcString, sizeof(crcString), "%08X", calculated_crc);
    
//     ESP_LOGI("ZigbeeServer", "Full input: %s", data_with_crc.c_str());
// ESP_LOGI("ZigbeeServer", "Data part: %s", data.c_str());
// ESP_LOGI("ZigbeeServer", "CRC part: %s", received_crc.c_str());

// ESP_LOGI("ZigbeeServer", "Calculated CRC length: %d", strlen(crcString));
// ESP_LOGI("ZigbeeServer", "Received CRC length: %d", received_crc.length());

received_crc = received_crc.substr(0, 8);
    return std::string(crcString) == std::string(received_crc);
}

void ZigbeeServer::handleIncomingMessage(const std::string& message) {
    if (!checkCRC32(message)) {
        ESP_LOGE("ZigbeeServer", "Invalid CRC");
        return;
    }

    size_t pos = message.find(",DATA:");
    if (pos != std::string::npos) {
        std::string id = message.substr(3, pos - 3); // Skip "ID:"

        std::string data = message.substr(pos + 6, message.find(",CRC:") - pos - 6); // Skip ",DATA:" và loại bỏ phần ",HASH:"

        // Kiểm tra xem id có tồn tại trong deviceList hay không
        auto it = std::find_if(deviceList.begin(), deviceList.end(), [&id](const Device& device) {
            return device.id == id;
        });

        if (it == deviceList.end()) {
            // Nếu không, thêm mới vào deviceList
            addDevice(id.c_str());
            if (onChangeCallback) {
                onChangeCallback();
            }
        } else {
            if (messageCallback) {
                messageCallback(id.c_str(), data.c_str());
            }
        }
    }
    else if (message.find("CMD:BRD:DISC") != std::string::npos) {
        // Xử lý khi nhận được broadcast message
        
    }
    else {
        ESP_LOGE("ZigbeeServer", "Invalid message: %s", message.c_str());
    }
}
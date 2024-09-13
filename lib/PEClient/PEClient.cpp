#include "PEClient.h"

PEClient *PEClient::_instance = nullptr;

PEClient::PEClient(const char *wifiSSID, const char *wifiPassword, const char *mqttServer, int mqttPort, const char *clientId, const char *username, const char *password)
    : _ssid(wifiSSID), _password(wifiPassword), _mqttServer(mqttServer), _mqttPort(mqttPort), _clientId(clientId), _username(username), _passwordMqtt(password), _client(_espClient)
{
    _client.setServer(_mqttServer, _mqttPort);
    _client.setCallback(callback);

    _sendMetricTopic = "v1/devices/";
    _sendMetricTopic += _clientId;
    _sendMetricTopic += "/metrics";

    _sendAttributeTopic = "v1/devices/";
    _sendAttributeTopic += _clientId;
    _sendAttributeTopic += "/attributes";

    _instance = this;
}

void PEClient::begin()
{
    // Serial.begin(115200);
    initWiFi();
    xTaskCreatePinnedToCore(
        [](void *pvParameters)
        {
            PEClient *peClient = static_cast<PEClient *>(pvParameters);
            for (;;)
            {
                peClient->loop();
                vTaskDelay(10);
            }
        },
        "PEClientTask",
        10000,
        this,
        1,
        NULL,
        1 // Chạy trên core 1
    );
}

void PEClient::loop()
{
    if (!_client.connected())
    {
        reconnect();
    }
    _client.loop();
}

boolean PEClient::connected()
{
    return _client.connected();
}

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == SYSTEM_EVENT_STA_DISCONNECTED) {
        Serial.println("WiFi lost connection");
        // Kiểm tra lý do ngắt kết nối
        if(info.prov_fail_reason == WIFI_REASON_AUTH_FAIL) {
            Serial.println("Authentication failed");
            ESP.restart();
        }
        else if(info.prov_fail_reason == WIFI_REASON_NO_AP_FOUND) {
            Serial.println("No AP found");
            ESP.restart();
        }
        else if(info.prov_fail_reason == WIFI_REASON_AUTH_EXPIRE) {
            Serial.println("Authentication expired");
            ESP.restart();
        }
        else if(info.prov_fail_reason == WIFI_REASON_ASSOC_EXPIRE) {
            Serial.println("Association expired");
            ESP.restart();
        }
        else if(info.prov_fail_reason == WIFI_REASON_HANDSHAKE_TIMEOUT) {
            Serial.println("Handshake timeout");
        }
        else if(info.prov_fail_reason == WIFI_REASON_CONNECTION_FAIL) {
            Serial.println("Connection failed");
        }
        else if(info.prov_fail_reason == WIFI_REASON_NOT_AUTHED) {
            Serial.println("Not authenticated");
        }
        else if(info.prov_fail_reason == WIFI_REASON_NOT_ASSOCED) {
            Serial.println("Not associated");
        }
        else if(info.prov_fail_reason == WIFI_REASON_ASSOC_LEAVE) {
            Serial.println("Association leave");
        }
        else if(info.prov_fail_reason == WIFI_REASON_ASSOC_NOT_AUTHED) {
            Serial.println("Association not authenticated");
        }
        else if(info.prov_fail_reason == WIFI_REASON_DISASSOC_PWRCAP_BAD) {
            Serial.println("Disassociation power capability bad");
        }
        else if(info.prov_fail_reason == WIFI_REASON_DISASSOC_SUPCHAN_BAD) {
            Serial.println("Disassociation supported channel bad");
        }
        else if(info.prov_fail_reason == WIFI_REASON_IE_INVALID) {
            Serial.println("Invalid IE");
        }
        else if(info.prov_fail_reason == WIFI_REASON_MIC_FAILURE) {
            Serial.println("MIC failure");
        }
        else if(info.prov_fail_reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT) {
            Serial.println("4-way handshake timeout");
        }
        else if(info.prov_fail_reason == WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT) {
            Serial.println("Group key update timeout");
        }
        else if(info.prov_fail_reason == WIFI_REASON_IE_IN_4WAY_DIFFERS) {
            Serial.println("IE in 4-way differs");
        }
        else if(info.prov_fail_reason == WIFI_REASON_GROUP_CIPHER_INVALID) {
            Serial.println("Group cipher invalid");
        }
        else if(info.prov_fail_reason == WIFI_REASON_PAIRWISE_CIPHER_INVALID) {
            Serial.println("Pairwise cipher invalid");
        }
    }
}

void PEClient::initWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(_ssid, _password);
    ESP_LOGI("PEClient", "Connecting to the WiFi network");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print('.');
        delay(1000);
        ESP_LOGI("PEClient", "WiFi status: %d", WiFi.status());
        if(WiFi.status() == WL_DISCONNECTED || WiFi.status() == WL_CONNECTION_LOST || WiFi.status() == WL_CONNECT_FAILED) {
            ESP_LOGE("PEClient", "WiFi disconnected");
            ESP.restart();
        }
    }
    ESP_LOGI("PEClient", "Connected to the WiFi network");
    ESP_LOGI("PEClient", "IP address: %s", WiFi.localIP().toString().c_str());
}

void PEClient::reconnect()
{
    while (!_client.connected())
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            initWiFi();
        }
        ESP_LOGI("PEClient", "Attempting MQTT connection...");
        if (_client.connect(_clientId, _username, _passwordMqtt))
        {
            ESP_LOGI("PEClient", "connected");
            String topic = "v1/devices/";
            topic += _clientId;
            topic += "/attributes/set";
            _client.subscribe(topic.c_str());
        }
        else
        {
            ESP_LOGE("PEClient", "failed, rc=%d try again in 5 seconds", _client.state());
            delay(5000);
        }
    }
}

void PEClient::callback(char *topic, byte *message, unsigned int length)
{
    String messageTemp;
    for (int i = 0; i < length; i++)
    {
        messageTemp += (char)message[i];
    }

    ESP_LOGD("PEClient", "Message arrived on topic: %s. Message: %s", topic, messageTemp.c_str());

    // Parse the JSON message
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, messageTemp);
    if (error)
    {
        ESP_LOGE("PEClient", "deserializeJson() failed: %s", error.c_str());
        return;
    }

    JsonObject obj = doc.as<JsonObject>();
    for (JsonPair kv : obj)
    {
        String key = kv.key().c_str();
        String value = kv.value().as<String>();

        if (_instance->_callbacks.find(key) != _instance->_callbacks.end())
        {
            _instance->_callbacks[key](value);
        }
    }
}

void PEClient::sendMetric(uint64_t timestamp, const char *key, double value)
{
    if (!_client.connected())
    {
        return;
    }
    JsonDocument doc;
    doc["ts"] = timestamp;

    JsonObject metrics = doc["metrics"].to<JsonObject>();
    metrics[key] = value;

    char buffer[256];
    serializeJson(doc, buffer);

    _client.publish(_sendMetricTopic.c_str(), buffer);
    ESP_LOGI("PEClient", "Send metric: %s", buffer);
}

void PEClient::sendMetric(const char *key, double value)
{
    if (!_client.connected())
    {
        return;
    }
    JsonDocument doc;

    JsonObject metrics = doc["metrics"].to<JsonObject>();
    metrics[key] = value;

    char buffer[256];
    serializeJson(doc, buffer);

    _client.publish(_sendMetricTopic.c_str(), buffer);
}

void PEClient::sendAttribute(const char *key, double value)
{
    if (!_client.connected())
    {
        return;
    }
    JsonDocument doc;

    JsonObject attributes = doc["attributes"].to<JsonObject>();
    attributes[key] = value;

    char buffer[256];
    serializeJson(doc, buffer);

    _client.publish(_sendAttributeTopic.c_str(), buffer);
}

void PEClient::sendAttribute(const char *key, const char *value)
{
    if (!_client.connected())
    {
        return;
    }
    JsonDocument doc;

    JsonObject attributes = doc["attributes"].to<JsonObject>();
    attributes[key] = value;

    char buffer[256];
    serializeJson(doc, buffer);

    _client.publish(_sendAttributeTopic.c_str(), buffer);
}

void PEClient::on(const char *key, void (*callback)(String))
{
    _callbacks[key] = std::bind(callback, std::placeholders::_1);
}
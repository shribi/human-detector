#include <Arduino.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>

#include "websocket.h"

#include "logger.h"
#include "base.h"

static void(*wsMsgHandler)(WsMessage msg) = nullptr;

websockets::WebsocketsClient& getWebSocketClient() {
    static websockets::WebsocketsClient ws;
    return ws;
}

bool isWebSocketConnected() {
    return getWebSocketClient().available();
}

void parseWsPayload(const String& payload, WsMessage& msg) {
    // json to WsMessage
    JsonDocument doc;
    deserializeJson(doc, payload);
    msg.cmd = static_cast<eWsEventType>(doc["cmd"].as<int>());
    msg.data = doc["data"].as<String>();
}

void constructWsPayload(const WsMessage& msg, String& payload) {
    // WsMessage to json
    JsonDocument doc;
    doc["cmd"] = static_cast<int>(msg.cmd);
    doc["data"] = msg.data;
    serializeJson(doc, payload);
}

void sendWebSocketMessage(const String& message) {
    getWebSocketClient().send(message);
}

void sendWebSocketBinaryMessage(const int16_t* message, size_t length) {
    auto& client = getWebSocketClient();
    if (!client.available()) {
        return;
    }
    client.sendBinary((const char*)message, length);
}

void registerWebSocketMessageHandler(void(*handler)(WsMessage msg)) {
    wsMsgHandler = handler;
}

// Listener for incoming WebSocket messages
void handleWebSocketMessage(websockets::WebsocketsMessage msg) {
    WsMessage wsMsg;
    parseWsPayload(msg.data(), wsMsg);
    if (wsMsgHandler) {
        wsMsgHandler(wsMsg);
    }
}

void startWebSocketClient(const char* host, uint16_t port) {
    String url = String("ws://") + host + ":" + port;
    log("Connecting to WebSocket Server: " + url);
    if (!getWebSocketClient().connect(url)) {
        log("WebSocket Connection Failed");
        delay(1000); // Wait before retrying
        startWebSocketClient(host, port); // Retry connection
    } else {
        log("WebSocket Connected");
    }
    getWebSocketClient().onMessage(handleWebSocketMessage);

    xTaskCreatePinnedToCore(
        [](void*){ 
            while (true) {
                getWebSocketClient().poll();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        },
        "WebSocketReconnectTask",
        4096,
        nullptr,
        1,
        nullptr,
        0);
}
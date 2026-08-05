#pragma once
#include <ArduinoWebsockets.h>
#include "base.h"

websockets::WebsocketsClient& getWebSocketClient();
void constructWsPayload(const WsMessage& msg, String& payload);
void parseWsPayload(const String& payload, WsMessage& msg);
void handleWebSocketMessage(websockets::WebsocketsMessage msg);
bool isWebSocketConnected();
void registerWebSocketMessageHandler(void(*handler)(WsMessage msg));
void sendWebSocketMessage(const String& message);
void startWebSocketClient(const char* host, uint16_t port);
void sendWebSocketBinaryMessage(const int16_t* message, size_t length);
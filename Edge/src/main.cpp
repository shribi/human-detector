#include <Arduino.h>
#include <WebServer.h>

#include "constants.h"
#include "logger.h"
#include "base.h"
#include "helpers.h"
#include "provision.h"
#include "websocket.h"
#include "devices.h"
#include <base64.h>

using namespace websockets;

WebServer server(80);
//==================================================
// Forward Declarations
//==================================================
bool credentialsExist();
void log(const String& message);
void factoryReset();

// Station Mode Methods
void startStationMode();

// Provisioning Mode Methods
void startProvisioningMode();
void startAccessPoint();
void startHttpServer();
void httpHandlerRoot();
void httpHandlerSubmit();
void httpHandlerLogs();

//==================================================
// Setup & Loop
//==================================================

void setup(){
    Serial.begin(115200);
    createLogTask();
    startAccessPoint();

    // Give some time for the AP to start
    delay(1000); 
    if (credentialsExist()) {
        log("Credentials exist in NVS");
        startStationMode();
    } else {
        log("Credentials do not exist in NVS");
        startProvisioningMode();
    }
}

void loop(){
    delay(5000);
}

//==================================================
// Members Definitions
//==================================================

void startAccessPoint() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("HumanDetector_AP", "123456789");
    log("Access Point IP: " + WiFi.softAPIP().toString());
}

void startHttpServer() {
    server.on("/", HTTP_GET, httpHandlerRoot);
    server.on("/submit", HTTP_POST, httpHandlerSubmit);
    server.on("/logs", HTTP_GET, httpHandlerLogs);
    server.begin();
    log("HTTP Server Started");

    xTaskCreatePinnedToCore(
        [](void*){ 
            while (true) {
                server.handleClient();
                vTaskDelay(pdMS_TO_TICKS(10)); // Delay to prevent task from consuming too much CPU
            }
        },
        "HttpServerTask",
        4096,
        nullptr,
        1,
        nullptr,
        0);
}

void httpHandlerLogs() {
    log("Handling HTTP Logs Request");
    String html = "<!DOCTYPE html><html><head><title>Device Logs</title></head><body>";
    html += "<h1>Device Logs</h1>";
    html += "<ul>";

    // start from headIndex and roate after the end, till headIndex - 1
    LogMessage logs[LOG_SIZE];
    size_t logCount = 0;
    getLogs(logs, logCount);
    for (size_t i = 0; i < logCount; ++i) {
        html += "<li>[" + String(logs[i].timestamp) + "] " + String(logs[i].message) + "</li>";
    }

    html += "</ul>";
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void httpHandlerRoot() {
    log("Handling HTTP Root Request");
    String html = "<!DOCTYPE html><html><head><title>Device Settings</title></head><body>";
    if (WiFi.isConnected()) {
        html += "<h1>Device Settings</h1>";
        html += "<p>Connected to WiFi: " + WiFi.SSID() + "</p>";
        html += "<p>IP Address: " + WiFi.localIP().toString() + "</p>";
        html += "<form action='/submit' method='POST'>";
        html += "<input type='submit' name='Factory Reset' value='Reset'>";
        html += "</form>";
    } else {
        html += "<h1>WiFi Configuration</h1>";
        html += "<form action='/submit' method='POST'>";
        html += "<label for='ssid'>Select WiFi Network:</label><br>";
        html += "<select name='ssid' id='ssid'>";
        const int networkCount = WiFi.scanNetworks();
        for (int i = 0; i < networkCount; ++i) {
            String network = WiFi.SSID(i);
            html += "<option value='" + network + "'>" + network + "</option>";
        }
        html += "</select><br><br>";
        html += "<label for='password'>Password:</label><br>";
        html += "<input type='password' name='password' id='password'><br><br>";
        html += "<input type='text' name='reservoir_ip' id='reservoir_ip' value='" + String(WS_HOST) + "' placeholder='Reservoir IP Address'><br><br>";
        html += "<input type='submit' name='submit' value='Connect'>";
        html += "</form></body></html>";
    }
    server.send(200, "text/html", html);
}

void httpHandlerSubmit() {
    log("Handling HTTP Submit Request");
    // Handle based on the form submission, submit value
    if (server.arg("submit") == "Reset") {
        log("Factory Reset Requested");
        provisionQueueSend(eEventType::EVT_DEV_RESET, {}, true);
        server.send(200, "text/plain", "Factory Reset Requested. Device will reset.");
        return;
    } 
    else if (server.arg("submit") == "Connect") {
        Config config;
        strncpy(config.ssid, server.arg("ssid").c_str(), sizeof(config.ssid) - 1);
        config.ssid[sizeof(config.ssid) - 1] = '\0';
        strncpy(config.password, server.arg("password").c_str(), sizeof(config.password) - 1);
        config.password[sizeof(config.password) - 1] = '\0';
        strncpy(config.reservoir_ip, server.arg("reservoir_ip").c_str(), sizeof(config.reservoir_ip) - 1);
        config.reservoir_ip[sizeof(config.reservoir_ip) - 1] = '\0';
        provisionQueueSend(eEventType::EVT_DEV_CONFIG, config, false);
        server.send(200, "text/plain", "Configuration Received. Attempting to connect...");
        return;
    } else {
        log("Unknown form submission");
        server.send(400, "text/plain", "Unknown form submission");
        return;
    }
}

void heartBeatTask(void *pvParameters) {
    while (true) {
        if (isWebSocketConnected()) {
            WsMessage msg;
            msg.cmd = eWsEventType::WsHeartbeat;
            msg.data = String("Heartbeat");
            String payload;
            constructWsPayload(msg, payload);
            sendWebSocketMessage(payload);
        }    
        else {
            log("WebSocket not connected. Attempting to reconnect...");
            stopRecordingI2S();
            getWebSocketClient().close();
            delay(1000); // Wait before retrying
            startWebSocketClient(WS_HOST, WS_PORT);
            log("WebSocket Client Restarted");
        }
        vTaskDelay(10 * 1000 / portTICK_PERIOD_MS); // Send heartbeat every 5 seconds
    }
}

void onRadarPresenceDetected() {
    WsMessage msg;
    msg.cmd = eWsEventType::WsCmdHumanDetected;
    msg.data = String("Human Detected");
    String payload;
    constructWsPayload(msg, payload);
    sendWebSocketMessage(payload);
}

void onAudioStream(int16_t* audioData, size_t bytesRead, size_t audioPacketCount) {
    if (bytesRead > 0 && isWebSocketConnected()) {
        sendWebSocketBinaryMessage(audioData, bytesRead);
    }
}

void handleWebSocketMessage(WsMessage msg) {
    switch (msg.cmd) {
        case eWsEventType::WsStopHumanDetection:
            log("Stop Human Detection Command Received");
            stopHumanDetection(); // Stop the radar task
            break;
        case eWsEventType::WsStartHumanDetection:
            log("Start Human Detection Command Received");
            startHumanDetection(); // Start the radar task
            break;
        case eWsEventType::WsCmdStartRecording:
            log("Start Recording Command Received");
            // Send Acknowledgment back to the server
            stopHumanDetection(); // Stop the radar task to avoid interference
            startRecordingI2S(); // Start the I2S recording task
            if (isWebSocketConnected())
            {
                WsMessage ackMsg;
                ackMsg.cmd = eWsEventType::WsCmdStartRecordingAck;
                ackMsg.data = String("Start Recording Acknowledged");
                String ackPayload;
                constructWsPayload(ackMsg, ackPayload);
                sendWebSocketMessage(ackPayload);
            }
            break;
        case eWsEventType::WsCmdEndRecording:
            log("End Recording Command Received");
            stopRecordingI2S(); // Stop the I2S recording task
            if (isWebSocketConnected())
            {
                // sendWebSocketBinaryMessage(audioCache, sizeof(audioCache));
                WsMessage ackMsg;
                ackMsg.cmd = eWsEventType::WsCmdEndRecordingAck;
                ackMsg.data = String("End Recording Acknowledged");
                String ackPayload;
                constructWsPayload(ackMsg, ackPayload);
                sendWebSocketMessage(ackPayload);
            }
            break;
        case eWsEventType::WsReboot:
            log("Reboot Command Received");
            ESP.restart();
            break;
        case eWsEventType::WsFactoryReset:
            log("Factory Reset Command Received");
            factoryReset();
            break;
        case eWsEventType::WsHeartbeat:
            log("Its Alive!");
            break;
        default:
            log("Unknown WebSocket Command Received");
            break;
    }
}

// --------- Provisioning Mode Methods ---------
void startProvisioningMode() {
    log("Starting Provisioning Mode");
    createProvisioningTask();
    startHttpServer();
}

// ----------- Station Mode Methods ------------
void startStationMode() {
    log("Starting Station Mode");
    startProvisioningMode(); // Start provisioning task to handle any future config changes
    connectToWiFi();
    
    // Create a WebSocket client to connect to the server, with the provided host and port
    registerWebSocketMessageHandler(handleWebSocketMessage);

    auto reservoirIPAddress = loadConfigFromNVS().reservoir_ip;
    if (strlen(reservoirIPAddress) <= 0) {
        log("Reservoir IP not set in NVS. Using default: " + String(WS_HOST));
        reservoirIPAddress = (char*)WS_HOST;
    }
    else {
        log("Reservoir IP set in NVS: " + String(reservoirIPAddress));
    }
    startWebSocketClient(reservoirIPAddress, WS_PORT);
    log("WebSocket Client Started");
    
    registerRadarCallback(onRadarPresenceDetected);
    registerAudioStreamCallback(onAudioStream);

    createRadarTask(); // Start the radar task to detect human presence
    createI2STask(); // Start the I2S task to handle audio streaming
    
    // Create a heartbeat task to send periodic heartbeats to the server
    xTaskCreate(heartBeatTask, "HeartBeatTask", 2048, NULL, 1, NULL);

    delay(1000); // Give some time for the radar task to start
}
#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <ld2410.h>
#include <ArduinoWebsockets.h>

using namespace websockets;

WebsocketsClient ws;

const char *WS_HOST = "192.168.1.33";   // Laptop IP
const int   WS_PORT = 12000;
const char *WS_PATH = "/audio";

//==================================================
// Configuration
//==================================================

// WiFi
const char* WIFI_SSID               = "Sathya Nilaya_AP";
const char* FALL_BACK_WIFI_SSID     = "Excitel_Sathya Nilaya";
const char* WIFI_PASSWORD           = "narayana";

// UART (LD2410C)
#define LD2410_RX_PIN 16
#define LD2410_TX_PIN 17

ld2410 radar;
HardwareSerial RadarSerial(2);

// I2S (INMP441)
#define I2S_WS      25
#define I2S_SCK     26
#define I2S_SD      22

int16_t audioBuffer[512];
#define I2S_PORT I2S_NUM_0

//==================================================
// Globals
//==================================================

bool humanDetected = false;

//==================================================
// Function Prototypes
//==================================================

void initWiFi();
void initRadar();
void initI2S();
bool connectWebSocket();

void radarTask(void *pvParameters);
void audioTask(void *pvParameters);

void processRadar();

// ==================================================
// Recording State Machine
// ==================================================
bool isRecording = false;

unsigned long recordingStartTime = 0;
unsigned long cooldownStartTime = 0;

enum RadarState
{
    IDLE,
    RECORDING,
    COOLDOWN
};

RadarState state = IDLE;

volatile bool startRecordingFlag = false;
volatile bool stopRecordingFlag = false;

//==================================================
// Setup
//==================================================

void setup()
{
    Serial.begin(115200);
    initWiFi();
    initRadar();
    initI2S();
    connectWebSocket();

    xTaskCreatePinnedToCore(
        radarTask,
        "Radar",
        4096,
        nullptr,
        2,
        nullptr,
        0);

    xTaskCreatePinnedToCore(
        audioTask,
        "Audio",
        8192,
        nullptr,
        1,
        nullptr,
        1);
}

//==================================================
// Loop
//==================================================

void loop()
{
    delay(1000);
}

//==================================================
// WiFi
//==================================================

bool connectWebSocket()
{
    String url = String("ws://") + WS_HOST + ":" + WS_PORT + WS_PATH;

    Serial.println(url);

    if (!ws.connect(url))
    {
        Serial.println("WebSocket Connection Failed");
        return false;
    }

    Serial.println("WebSocket Connected");

    return true;
}

void initWiFi()
{
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connecting");

    while (WiFi.status() != WL_CONNECTED)
    {
        WiFi.begin(FALL_BACK_WIFI_SSID, WIFI_PASSWORD);
        if (WiFi.status() == WL_CONNECTED)
        {
            break;
        }
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected : ");
    Serial.println(WiFi.localIP());
}

//==================================================
// Radar
//==================================================

void initRadar()
{
    RadarSerial.begin(
        256000,
        SERIAL_8N1,
        LD2410_RX_PIN,
        LD2410_TX_PIN);

    radar.begin(RadarSerial);
    Serial.println("Radar Ready");
}

void processRadar()
{
    radar.read();
    Serial.println(radar.isConnected());
    if (radar.isConnected())
    {
        if (radar.presenceDetected())
        {
            humanDetected = true;

            Serial.println("Human Detected");

            Serial.print("Distance : ");
            Serial.println(radar.detectionDistance());

            Serial.print("Moving Energy : ");
            Serial.println(radar.movingTargetEnergy());

            Serial.print("Stationary Energy : ");
            Serial.println(radar.stationaryTargetEnergy());
        }
        else
        {
            humanDetected = false;
        }
    }
}

//==================================================
// I2S
//==================================================

void initI2S()
{
    i2s_config_t i2s_config =
    {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),

        .sample_rate = 16000,

        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,

        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,

        .communication_format = I2S_COMM_FORMAT_STAND_I2S,

        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,

        .dma_buf_count = 8,

        .dma_buf_len = 512,

        .use_apll = false,

        .tx_desc_auto_clear = false,

        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config =
    {
        .bck_io_num = I2S_SCK,

        .ws_io_num = I2S_WS,

        .data_out_num = I2S_PIN_NO_CHANGE,

        .data_in_num = I2S_SD
    };

    i2s_driver_install(
        I2S_PORT,
        &i2s_config,
        0,
        NULL);

    i2s_set_pin(
        I2S_PORT,
        &pin_config);

    i2s_zero_dma_buffer(I2S_PORT);

    Serial.println("I2S Initialized");
}

//==================================================
// Tasks
//==================================================

void startRecording()
{
    Serial.println("Recording Started");

    startRecordingFlag = true;
    stopRecordingFlag = false;
}

void stopRecording()
{
    Serial.println("Recording Stopped");

    stopRecordingFlag = true;
}

void radarTask(void *pvParameters)
{    
    while (true)
    {
        if (WiFi.status() != WL_CONNECTED)
        {
            initWiFi();
        }
        radar.read();
        switch(state)
        {
            case IDLE:
                if(radar.presenceDetected())
                {
                    Serial.println("Human Detected");
                    recordingStartTime = millis();
                    state = RECORDING;
                    startRecording();
                }
                break;

            case RECORDING:
                if(millis() - recordingStartTime >= 20 * 1000)
                {
                    stopRecording();
                    cooldownStartTime = millis();
                    state = COOLDOWN;
                }
                break;

            case COOLDOWN:
                if(millis() - cooldownStartTime >= 5 * 60 * 1000)
                {
                    state = IDLE;
                }
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}


void sendStartMessage()
{
    if (ws.available())
    {
        Serial.println("Sending Start Message");
        ws.send("START_AUDIO");
    }
}

void streamAudioChunk()
{
    size_t bytesRead;
    i2s_read(
        I2S_PORT,
        audioBuffer,
        sizeof(audioBuffer),
        &bytesRead,
        portMAX_DELAY);

    if (ws.available())
    {
        Serial.print("Streaming Audio Chunk: ");
        Serial.println(bytesRead);
        ws.send((const char *)audioBuffer, bytesRead);
    }
}

void sendEndMessage()
{
    if (ws.available())
    {
        ws.send("END_AUDIO");
    }
}

void audioTask(void *pvParameters)
{
    while (true)
    {
        if (startRecordingFlag)
        {
            startRecordingFlag = false;

            sendStartMessage();

            while (!stopRecordingFlag)
            {
                streamAudioChunk();
            }

            stopRecordingFlag = false;

            sendEndMessage();
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
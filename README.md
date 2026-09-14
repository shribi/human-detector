# Smart Edge Intrusion Detection System

An AI-assisted edge surveillance system that detects human presence using a 24GHz mmWave radar, records high-quality audio on demand using an ESP32 and INMP441 microphone, and streams the audio to a Go backend for storage and analysis.

The system is designed for low-power, privacy-preserving perimeter monitoring without continuously recording audio.

---

# Architecture

```
                 +--------------------+
                 | 24GHz mmWave Radar |
                 +---------+----------+
                           |
                    Presence Detected
                           |
                           v
+--------------------------------------------------+
|                ESP32 Edge Device                 |
|--------------------------------------------------|
| • Radar Monitoring Task                          |
| • I2S Audio Capture (INMP441)                    |
| • Circular Audio Ring Buffer                     |
| • FreeRTOS Task Synchronization                  |
| • WebSocket Client                               |
+-----------------------+--------------------------+
                        |
                 Binary PCM Stream
                        |
                  WebSocket (LAN/WiFi)
                        |
                        v
+--------------------------------------------------+
|                Go Backend Server                 |
|--------------------------------------------------|
| • WebSocket Server                               |
| • Audio Packet Receiver                          |
| • WAV File Generator                             |
| • Telegram Notification                          |
| • AI Processing Pipeline (Optional)              |
+--------------------------------------------------+
```

---

# Features

- Human presence detection using 24GHz FMCW mmWave radar
- Event-driven audio recording
- Continuous circular audio buffer for pre-event capture
- 16 kHz / 16-bit PCM audio acquisition
- Binary WebSocket streaming
- Automatic WAV generation
- Telegram Bot integration
- Modular AI processing pipeline
- Low latency communication
- Optimized for ESP32 using FreeRTOS

---

# Hardware

## Edge Device

- ESP32
- INMP441 Digital I2S MEMS Microphone
- Waveshare 24GHz Human Presence Radar
- WiFi Network

---

# Software Stack

## Edge

- C++
- ESP-IDF / Arduino Framework
- FreeRTOS
- I2S Driver
- WebSockets

## Backend

- Go
- Gorilla WebSocket
- WAV Encoder
- Telegram Bot API

---

# System Workflow

1. ESP32 continuously monitors the mmWave radar.
2. Audio samples are continuously written into a circular ring buffer.
3. Human presence triggers an event.
4. The latest audio (including pre-event samples) is retained.
5. Audio is streamed to the Go server over WebSocket.
6. Go reconstructs PCM into a valid WAV file.
7. The recording is stored.
8. AI analysis (optional) can process the recording.
9. Telegram notifications are sent to the user.

---

# Audio Specifications

| Property | Value |
|----------|------:|
| Sample Rate | 16000 Hz |
| Sample Format | PCM Signed 16-bit |
| Channels | Mono |
| Encoding | Little Endian |
| Transport | Binary WebSocket |

---

# Backend

The Go backend performs the following:

- Accepts WebSocket connections
- Receives binary PCM audio
- Generates valid WAV files
- Stores recordings
- Sends Telegram alerts
- Provides interfaces for AI analysis

---

# Performance

- 16 kHz audio capture
- Low latency streaming
- Binary packet transport
- DMA-based I2S acquisition
- Event-driven recording
- Optimized memory usage

---

# Future Improvements

- Improved Edge Logging
- Bird/Animal/Human classification
- Audio compression (Opus)
- Cloud synchronization
- Mobile application
- OTA firmware updates

---

# Skills Demonstrated

- Embedded Systems
- ESP32 Firmware Development
- FreeRTOS
- I2S Audio
- WebSocket Communication
- Go Backend Development
- Binary Protocol Design
- Ring Buffer Implementation
- Concurrent Programming
- Network Programming
- Audio Processing
- System Architecture

---

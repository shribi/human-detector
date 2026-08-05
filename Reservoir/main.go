package main

import (
	"context"
	"encoding/binary"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net/http"
	"net/url"
	"os"
	"time"

	"github.com/google/uuid"
	"github.com/gorilla/websocket"

	"github.com/aws/aws-sdk-go-v2/aws"
	"github.com/aws/aws-sdk-go-v2/config"
	"github.com/aws/aws-sdk-go-v2/credentials"
	"github.com/aws/aws-sdk-go-v2/service/s3"
)

const (
	sampleRate    = 16000
	bitsPerSample = 16
	channels      = 1
)

const (
	botToken = "8946625392:AAHWUZuHPVPp56CenOfPcAKtIiGUreylfsw"
	keyID = "005106d340fd53d0000000002"
	applicationKey = "K005tqpD0EdYYj3GeteuxAXjSYw8M1g"
	bucket = "terrace-recs"
)

var subscribers = []string{
	"841893119",
	// "981786229",
	// "1216755897",
}

var upgrader = websocket.Upgrader{
	CheckOrigin: func(r *http.Request) bool {
		return true
	},
}

type Recorder struct {
	file      *os.File
	dataBytes uint32
	recording bool
}

type WsMessage struct {
	Cmd eWsCmd `json:"cmd"`
	Data string `json:"data"`
}

// 1. Define the custom type
type eWsCmd int

// 2. Declare the enum values using iota
const (
    WsCmdUnknown   eWsCmd = iota
	WsCmdHumanDetected
	WsCmdAudioStream
	WsCmdStartRecording
	WsCmdStartRecordingAck
	WsCmdEndRecording
	WsCmdEndRecordingAck
	WsHeartbeat
	WsReboot
	WsFactoryReset
	WsCmdStopHumanDetection
	WsCmdStartHumanDetection
)

var (
	s3Client *s3.Client = nil

	// 1 minute ago to allow immediate detection on first run
	lastHumanDetectionTime time.Time = time.Now().Add(-1 * time.Minute)

	// 10 minutes ago to allow immediate recording on first detection
	lastRecordingTime time.Time = time.Now().Add(-8 * time.Minute)
	humanDetectionActive = false
)

func SignedURL(
	key string,
) (string, error) {

	presign := s3.NewPresignClient(s3Client)

	req, err := presign.PresignGetObject(
		context.Background(),

		&s3.GetObjectInput{
			Bucket: aws.String(bucket),
			Key:    aws.String(key),
		},

		// Set the expiration time for the signed URL (e.g., 7 days)
		s3.WithPresignExpires(24*7*time.Hour),
	)

	if err != nil {
		return "", err
	}

	return req.URL, nil
}

func UploadFile(localPath string) (string, error) {
	file, err := os.Open(localPath)
	if err != nil {
		return "", err
	}
	defer file.Close()

	now := time.Now()
    remoteKey := fmt.Sprintf(
        "%04d/%02d/%02d/%s.wav",
        now.Year(),
        now.Month(),
        now.Day(),
        uuid.New().String(),
    )

	_, err = s3Client.PutObject(
		context.Background(),
		&s3.PutObjectInput{
			Bucket: aws.String(bucket),
			Key:    aws.String(remoteKey),
			Body:   file,
		},
	)
	if err != nil {
		return "", err
	}

	url , err := SignedURL(remoteKey)
	if err != nil {
		return "", err
	}
	return url, nil
}

func SendTelegram(msg string, chatID string) error {
	api := fmt.Sprintf(
		"https://api.telegram.org/bot%s/sendMessage",
		botToken,
	)

	values := url.Values{}
	values.Set("chat_id", chatID)
	values.Set("text", msg)

	resp, err := http.PostForm(api, values)
	if err != nil {
		return err
	}
	defer resp.Body.Close()

	body, _ := io.ReadAll(resp.Body)
	fmt.Println(string(body))
	return nil
}

func writeWavHeader(f *os.File) error {
	header := make([]byte, 44)
	copy(header[0:], "RIFF")
	copy(header[8:], "WAVE")
	copy(header[12:], "fmt ")
	binary.LittleEndian.PutUint32(header[16:], 16)
	binary.LittleEndian.PutUint16(header[20:], 1)
	binary.LittleEndian.PutUint16(header[22:], channels)
	binary.LittleEndian.PutUint32(header[24:], sampleRate)
	byteRate := sampleRate * channels * bitsPerSample / 8
	binary.LittleEndian.PutUint32(header[28:], uint32(byteRate))
	blockAlign := channels * bitsPerSample / 8
	binary.LittleEndian.PutUint16(header[32:], uint16(blockAlign))
	binary.LittleEndian.PutUint16(header[34:], bitsPerSample)
	copy(header[36:], "data")
	_, err := f.Write(header)
	return err
}

func finalizeWav(r *Recorder) {
	r.file.Seek(4, 0)
	binary.Write(r.file, binary.LittleEndian, uint32(36+r.dataBytes))
	r.file.Seek(40, 0)
	binary.Write(r.file, binary.LittleEndian, r.dataBytes)
	r.file.Close()

	log.Println("Recording finalized:", r.file.Name())
	url , err := UploadFile(r.file.Name())
	if err != nil {
		log.Println("Error uploading file:", err)
		return
	}	

	log.Println("Notifying subscribers about new recording:", url)
	for _, chatID := range subscribers {
		SendTelegram("ALERT! Aliens In the Terrace....\n Letss gooo kick them \n New recording available: " + url, chatID)
	}
}


func NewClient() (*s3.Client, error) {
	cfg, err := config.LoadDefaultConfig(
		context.Background(),
		config.WithRegion("us-east-005"),
		config.WithCredentialsProvider(
			credentials.NewStaticCredentialsProvider(
				keyID,
				applicationKey,
				"",
			),
		),
	)

	if err != nil {
		return nil, err	
	}

	client := s3.NewFromConfig(
		cfg,

		func(o *s3.Options) {
			o.BaseEndpoint = aws.String(
				"https://s3.us-east-005.backblazeb2.com",
			)
		},
	)

	return client, nil
}

func extractWsMessage(data []byte) (WsMessage, error) {
	var msg WsMessage
	err := json.Unmarshal(data, &msg)
	if err != nil {
		return WsMessage{}, err
	}
	return msg, nil
}

func sendStartRecordingCmd(conn *websocket.Conn) {
	startRecordingMsg := WsMessage{
		Cmd: WsCmdStartRecording,
		Data: "Start recording",
	}
	msgBytes, _ := json.Marshal(startRecordingMsg)
	conn.WriteMessage(websocket.TextMessage, msgBytes)
}

func sendEndRecordingCmd(conn *websocket.Conn) {
	endRecordingMsg := WsMessage{
		Cmd: WsCmdEndRecording,
		Data: "End recording",
	}
	msgBytes, _ := json.Marshal(endRecordingMsg)
	conn.WriteMessage(websocket.TextMessage, msgBytes)
}

func startTimerRoutine(rec *Recorder, conn *websocket.Conn) {
	if rec.recording {
		go func() {
			timer := time.NewTimer(16 * time.Second)
			defer timer.Stop()

			<-timer.C
			// send a message to ws client to stop recording
			sendEndRecordingCmd(conn)
		}()
	}
}


func sendStopHumanDetectionCmd(conn *websocket.Conn) {
	stopHumanDetectionMsg := WsMessage{
		Cmd: WsCmdStopHumanDetection,
		Data: "Stop human detection",
	}
	msgBytes, _ := json.Marshal(stopHumanDetectionMsg)
	conn.WriteMessage(websocket.TextMessage, msgBytes)
}

func sendStartHumanDetectionCmd(conn *websocket.Conn) {
	if time.Since(lastHumanDetectionTime) >= 2*60*time.Second {
		log.Println("Sending start human detection command to ws client")
		startHumanDetectionMsg := WsMessage{
			Cmd: WsCmdStartHumanDetection,
			Data: "Start human detection",
		}
		msgBytes, _ := json.Marshal(startHumanDetectionMsg)
		conn.WriteMessage(websocket.TextMessage, msgBytes)
		lastHumanDetectionTime = time.Now()
		humanDetectionActive = true
	} else {
		if humanDetectionActive {
			sendStopHumanDetectionCmd(conn)
			humanDetectionActive = false
		}
	}
}


func wsHandler(w http.ResponseWriter, req *http.Request) {
	conn, err := upgrader.Upgrade(w, req, nil)
	if err != nil {
		log.Println(err)
		return
	}
	defer conn.Close()

	logFile, err := os.OpenFile("reservoir.log", os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		log.Println("Error opening log file:", err)
		return
	}
	defer logFile.Close()
	log.SetOutput(logFile)

	rec := Recorder{}
	s3Client, err = NewClient()
	if err != nil {
		log.Println("Error creating S3 client:", err)
		return
	}

	for {
		msgType, data, err := conn.ReadMessage()
		if err != nil {
			break
		}

		if msgType == websocket.TextMessage {			
			msg, err := extractWsMessage(data)
			if err != nil {
				log.Println("Error extracting WS message:", err)
				continue
			}

			switch msg.Cmd {
				case WsCmdHumanDetected:
					// Check last human detection time. 
					now := time.Now()
					lastHumanDetectionTime = now
					log.Println("Ws Message: ", string(msg.Data))

					// Send a message to ws client
					if !rec.recording && time.Since(lastRecordingTime) >= 10*60*time.Second {
						log.Println("Sending start recording command to ws client")
						sendStartRecordingCmd(conn)
					}

				case WsCmdStartRecordingAck:
					sendStopHumanDetectionCmd(conn)
					name := fmt.Sprintf("%d.wav", time.Now().Unix())			
					// Cleanup the previous rec
					if rec.recording {
						finalizeWav(&rec)
						log.Println("Previous recording finalized:", rec.file.Name())
					}

					path := "./recordings"
					if _, err := os.Stat(path); os.IsNotExist(err) {
						os.Mkdir(path, 0755)
					}

					name = path + "/" + name
					rec.file, _ = os.Create(name)
					writeWavHeader(rec.file)
					rec.dataBytes = 0
					rec.recording = true
					log.Println("Recording started:", name)					
					startTimerRoutine(&rec, conn)

				case WsCmdEndRecordingAck:
					if rec.recording {
						finalizeWav(&rec)
						rec.recording = false
						log.Println("Recording complete")
						lastRecordingTime = time.Now()
					}

				case WsHeartbeat:
					heartbeatMsg := WsMessage{
						Cmd: WsHeartbeat,
						Data: "Heartbeat",
					}
					heartbeatMsgBytes, _ := json.Marshal(heartbeatMsg)
					conn.WriteMessage(websocket.TextMessage, heartbeatMsgBytes)

					// Arming the human detection after 2 minutes of last detection
					sendStartHumanDetectionCmd(conn)

				default:
					log.Println("Unknown command received:", msg.Cmd)
			}
		} else if msgType == websocket.BinaryMessage {
			if !rec.recording {
				log.Println("Audio stream received, but not recording")
				continue
			}
			rawBytes := data
			bytesWritten, err := rec.file.Write(rawBytes)
			if err != nil {
				log.Println("Error writing to file:", err)
				continue
			}
			rec.dataBytes += uint32(bytesWritten)
		} else {
			log.Println("Unknown message type:", msgType)
		}
	}
}

func main() {
	http.HandleFunc("/", wsHandler)
	log.Println("Listening on :12000")
	log.Fatal(http.ListenAndServe(":12000", nil))
}

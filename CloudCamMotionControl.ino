#include <AsyncTCP.h>
#include <PubSubClient.h>
#include <WiFi.h>


const char* const TAG = "CAM_CONTROL";

// LED pins.
const uint8_t RED_LED_PIN = GPIO_NUM_3;
const uint8_t GREEN_LED_PIN = GPIO_NUM_4;
const uint8_t BLUE_LED_PIN = GPIO_NUM_5;


/**************************************************************************************/
/*                                  Common constants                                  */

// Common WiFi settings.
const char* const WIFI_SSID = "<WIFI_SSID>";
const char* const WIFI_PWD = "<WIFI_PASSWORD>";

// Common MQTT settings.
const uint16_t MQTT_PORT = 1883;
const char* const MQTT_SERVER = "<MQTT_SERVER_IP>";
const char* const MQTT_CLIENT_ID = "<MQTT_CLIENT_ID>";
const char* const MQTT_USER_NAME = "<MQTT_USER_NAME>";
const char* const MQTT_PASSWORD = "<MQTT_PASSWORD>";
const char* const MQTT_MOTION_ON_MESSAGE = "ON";
const char* const MQTT_MOTION_OFF_MESSAGE = "OFF";

// Common camera settings.
const uint16_t CAMERA_EVENT_PORT = 3201;
const char* const CAMERA_MOTION_EVENT = "EVENT: MOTION detect";

// Individual camera settings.
const char* const INDOOR_CAMERA_IP = "<FIRST_CAMERA_IP>";
const char* const INDOOR_CAMERA_MOTION_TOPIC = "indoorcam/motion";
const uint8_t INDOOR_CAMERA_MAX_MOTION_EVENTS = 1;
const uint32_t INDOOR_CAMERA_MOTION_COUNTER_RESET_TIMEOUT = 5000;
const uint32_t INDOOR_CAMERA_MOTION_RESET_TIMEOUT = 10000;
const uint8_t INDOOR_CAMERA_MOTION_DETECTED_LED = BLUE_LED_PIN;

const char* const OUTDOOR_CAMERA_IP = "<SECOND_CAMERA_IP>";
const char* const outDOOR_CAMERA_MOTION_TOPIC = "doorcam/motion";
const uint8_t OUTDOOR_CAMERA_MAX_MOTION_EVENTS = 2;
const uint32_t OUTDOOR_CAMERA_MOTION_COUNTER_RESET_TIMEOUT = 10000;
const uint32_t OUTDOOR_CAMERA_MOTION_RESET_TIMEOUT = 15000;
const uint8_t OUTDOOR_CAMERA_MOTION_DETECTED_LED = GREEN_LED_PIN;

/**************************************************************************************/


/**************************************************************************************/
/*                                     Data types                                     */

// The record describes remote IP camera.
struct CAMERA_DATA
{
    // Camera IP address.
    const char* Address;
    // Camera motion detection MQTT topic.
    const char* Topic;
    // Number of motion detected message must be received before MQTT motion
    // message will be send. Set to 1 to send MQTT message on first motion event.
    const uint8_t MaxMotionEvents;
    // Timeout after which motion event counter should be reset. Ignored if
    // MaxMotionEvents == 1
    const uint32_t MotionCounterResetTimout;
    // Timeout after which MQTT motion reset will be send.
    const uint32_t MotionResetTimeout;
    // Motion indication PIN. 0 - disabled.
    const uint8_t MotionIndicationPin;
    // Motion detection status. True if camera ready for motion
    // detection.
    bool Ready;
    // Current camera event message text.
    String EventText;
    // Motion detection event counter.
    uint8_t MotionMessageCounter;
    // True if motion detected.
    bool MotionDetected;
    // Millis when last time motion detected.
    uint32_t MotionMillis;
    // Camera TCP client instance.
    AsyncClient* Client;
};

/**************************************************************************************/


/**************************************************************************************/
/*                                  Global variables                                  */

// Cameras array.
CAMERA_DATA Cameras[] = {
    {
        INDOOR_CAMERA_IP,
        INDOOR_CAMERA_MOTION_TOPIC,
        INDOOR_CAMERA_MAX_MOTION_EVENTS,
        INDOOR_CAMERA_MOTION_COUNTER_RESET_TIMEOUT,
        INDOOR_CAMERA_MOTION_RESET_TIMEOUT,
        INDOOR_CAMERA_MOTION_DETECTED_LED,
        false, "", 0, false, 0, new AsyncClient()
    },
    {
        OUTDOOR_CAMERA_IP,
        outDOOR_CAMERA_MOTION_TOPIC,
        OUTDOOR_CAMERA_MAX_MOTION_EVENTS,
        OUTDOOR_CAMERA_MOTION_COUNTER_RESET_TIMEOUT,
        OUTDOOR_CAMERA_MOTION_RESET_TIMEOUT,
        OUTDOOR_CAMERA_MOTION_DETECTED_LED,
        false, "", 0, false, 0, new AsyncClient()
    }
};

const size_t CamsCount = sizeof(Cameras) / sizeof(Cameras[0]);

// WiFiClient for MQTT connection.
WiFiClient _WiFiClient;
// MQTT client.
PubSubClient _MqttClient(_WiFiClient);

/**************************************************************************************/


/**************************************************************************************/
/*                                   MQTT  routines                                   */

// Checks MQTT connection status and sends specified MQTT message with
// specified MQTT topic.
void SendMqttMessage(const char* const Topic, const char* const Message)
{
    ESP_LOGI(TAG, "Check WiFi connection status");
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP_LOGI(TAG, "WiFi not connected");
        return;
    }
    
    ESP_LOGI(TAG, "Check MQTT connection status");
    if (!_MqttClient.connected())
    {
        ESP_LOGI(TAG, "MQTT not connected, try to connect");
        _MqttClient.connect(MQTT_CLIENT_ID, MQTT_USER_NAME, MQTT_PASSWORD);
    }
    if (!_MqttClient.connected())
    {
        ESP_LOGE(TAG, "MQTT connect failed");
        return;
    }

    ESP_LOGI(TAG, "MQTT connected. Send MQTT message (%s : %s)", Topic, Message);
    if (!_MqttClient.publish(Topic, Message))
    {
        ESP_LOGE(TAG, "MQTT message send failed");
        return;
    }
    
    ESP_LOGI(TAG, "MQTT message sent");
}

void SendMotionOnMessage(const CAMERA_DATA* const CamData)
{
    if (CamData->MotionIndicationPin != 0)
        digitalWrite(CamData->MotionIndicationPin, HIGH);
    SendMqttMessage(CamData->Topic, MQTT_MOTION_ON_MESSAGE);
}

void SendMotionOffMessage(const CAMERA_DATA* const CamData)
{
    if (CamData->MotionIndicationPin != 0)
        digitalWrite(CamData->MotionIndicationPin, LOW);
    SendMqttMessage(CamData->Topic, MQTT_MOTION_OFF_MESSAGE);
}

/**************************************************************************************/


/**************************************************************************************/
/*                             TCP clients event handlers                             */

void ResetCameraData(CAMERA_DATA* const CamData)
{
    CamData->Ready = false;
    CamData->EventText = "";
    CamData->MotionMessageCounter = 0;
    CamData->MotionDetected = false;
    CamData->MotionMillis = 0;
}

void TcpConnect(CAMERA_DATA* const CamData)
{
    ResetCameraData(CamData);
    
    if (WiFi.status() != WL_CONNECTED)
    {
        ESP_LOGI(TAG, "WiFi not connected");
        return;
    }

    if (CamData->Client->connect(CamData->Address, CAMERA_EVENT_PORT))
    {
        ESP_LOGI(TAG, "Connection to %s started", CamData->Address);
        return;
    }
    
    ESP_LOGE(TAG, "Start connection to %s failed", CamData->Address);
}

// The event called when TCP connection established.
void ClientConnect(void* Args, AsyncClient* Client)
{
    CAMERA_DATA* CamData = (CAMERA_DATA*)Args;
    ESP_LOGI(TAG, "Connected to camera: %s", CamData->Address);
}

// The event called when TCP connection error appeared.
void ClientError(void* Args, AsyncClient* Client, int8_t Error)
{
    CAMERA_DATA* CamData = (CAMERA_DATA*)Args;
    ESP_LOGE(TAG, "Connect to camera %s failed: %d", CamData->Address, Error);

    Client->close(true);
    TcpConnect(CamData);
}

void ClientTimeout(void* Args, AsyncClient* Client, uint32_t Time)
{
    CAMERA_DATA* CamData = (CAMERA_DATA*)Args;
    ESP_LOGI(TAG, "Connect to camera %s timeout %d", CamData->Address, Time);
    
    Client->close(true);
    TcpConnect(CamData);
}

// The event called when TCP connection closed.
void ClientDisconnect(void* Args, AsyncClient* Client)
{
    CAMERA_DATA* CamData = (CAMERA_DATA*)Args;
    ESP_LOGI(TAG, "Disconnected from camera: %s", CamData->Address);

    Client->close(true);
    TcpConnect(CamData);
}

// The event called when data from camera received.
void ClientData(void* Args, AsyncClient* Client, void* Data, size_t Len)
{
    if (Len == 0)
        return;
    
    CAMERA_DATA* CamData = (CAMERA_DATA*)Args;
    if (!CamData->Ready)
    {
        if (Len > 17)
            return;
        CamData->Ready = true;
    }

    size_t i = 0;
    char* StrData = (char*)Data;
    while (i < Len)
    {
        char c = StrData[i];
        i++;

        if (StrData[i] != 0x0A)
        {
            CamData->EventText += c;
            continue;
        }
        
        if (CamData->EventText.indexOf(CAMERA_MOTION_EVENT) > -1)
        {
            CamData->MotionMillis = millis();
            if (!CamData->MotionDetected)
            CamData->MotionMessageCounter++;
            ESP_LOGI(TAG, "Motion event received from %s (%d)", CamData->Address,
                CamData->MotionMessageCounter);
        }
        CamData->EventText = "";
    }
}

/**************************************************************************************/


/**************************************************************************************/
/*                                WiFi  event handlers                                */

// The event called when connection to WiFI network has been established.
void WiFiConnected(WiFiEvent_t Event, WiFiEventInfo_t EventInfo)
{
    ESP_LOGI(TAG, "WiFi network connected");
}

// The event called when IP address assigned.
void WiFiIpAssigned(WiFiEvent_t Event, WiFiEventInfo_t EventInfo)
{
    ESP_LOGI(TAG, "IP address assigned: %s", WiFi.localIP().toString());
    
    for (size_t i = 0; i < CamsCount; i++)
        TcpConnect(&Cameras[i]);
}

// The event called when WiFi network disconnected.
void WiFiDisconnected(WiFiEvent_t Event, WiFiEventInfo_t EventInfo)
{
    ESP_LOGI(TAG, "WiFi network disconnected: %d",
        EventInfo.wifi_sta_disconnected.reason);
}

/**************************************************************************************/


/**************************************************************************************/
/*                                  Arduino routines                                  */

void setup()
{
    // As always, first initialize debug UART.
    Serial.begin(115200);
    // Wait UART startup (actually we need this delay for UART connection).
    delay(1000);

    // Init LED pins.
    pinMode(RED_LED_PIN, OUTPUT);
    pinMode(GREEN_LED_PIN, OUTPUT);
    pinMode(BLUE_LED_PIN, OUTPUT);
    
    // Turn all LEDs off.
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(BLUE_LED_PIN, LOW);
    
    // Add WiFi event handlers.
    WiFi.onEvent(WiFiConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_CONNECTED);
    WiFi.onEvent(WiFiIpAssigned, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent(WiFiDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    // Add TCP clients event handlers.
    for (size_t i = 0; i < CamsCount; i++)
    {
        Cameras[i].Client->onConnect(ClientConnect, (void*)&Cameras[i]);
        Cameras[i].Client->onError(ClientError, (void*)&Cameras[i]);
        Cameras[i].Client->onTimeout(ClientTimeout, (void*)&Cameras[i]);
        Cameras[i].Client->onData(ClientData, (void*)&Cameras[i]);

        // Allocate memory for event text.
        Cameras[i].EventText.reserve(255);
    }

    // Configure MQTT server.
    _MqttClient.setServer(MQTT_SERVER, MQTT_PORT);

    // Enable WiFi auto-reconnect.
    WiFi.setAutoReconnect(true);
    // Start WiFi connection.
    WiFi.begin(WIFI_SSID, WIFI_PWD);
}

void loop()
{
    for (size_t i = 0; i < CamsCount; i++)
    {
        CAMERA_DATA* CamData = &Cameras[i];
        
        if (CamData->MotionDetected)
        {
            if (millis() - CamData->MotionMillis >= CamData->MotionResetTimeout)
            {
                ESP_LOGI(TAG, "Motion reset on camera %s", CamData->Address);
                SendMotionOffMessage(CamData);
                CamData->MotionMessageCounter = 0;
                CamData->MotionDetected = false;
                CamData->MotionMillis = 0;
            }
            continue;
        }

        if (CamData->MotionMessageCounter >= CamData->MaxMotionEvents)
        {
            ESP_LOGI(TAG, "Motion detected on camera %s", CamData->Address);
            CamData->MotionDetected = true;
            SendMotionOnMessage(CamData);
            continue;
        }

        if (CamData->MotionMessageCounter > 0)
        {
            if (millis() - CamData->MotionMillis >= CamData->MotionCounterResetTimout)
            {
                ESP_LOGI(TAG, "Motion counter reset for %s", CamData->Address);
                CamData->MotionMessageCounter = 0;
                CamData->MotionMillis = 0;
            }
        }
    }
}

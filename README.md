# Cloud Cameras Motion Control
 
ESP32 firmware to handle YCC365 cloud cameras motion detection notification

## Description

This firmware allows to handle YCC365 (GK7102) cloud cameras with [hack installed](https://github.com/ant-thomas/zsgx1hacks) onboard motion detection feature and translate them to MQTT messages for [HomeBridge](https://homebridge.io) [Camera.UI](https://github.com/seydx/homebridge-camera-ui) plugin.

## Why do you need this firmware

By default when you add such camera to the HomeBridge Camera.UI you enables software video analizing and motion detection in Camera.UI. It works great but, however, it detect light changing as motion and may cause recording empty videos and send notification when there is no actual motion.

Fortunately such cameras usually have onboard motion detection. The only problem was to find how to get such notitifcations from camera. And there is a solution.

The camera sends simple text events to TCP port 3201 (it looks like simple logging port). So I wrote this simple firmware (for me its better to have separate ESP32 board that handles such things) that connects to cameras on TCP port 3201, receives all events and translate them to Camera UI MQTT messages.

## Configuration

**WiFi Settings**

_WIFI_SSID_ - Set to your WiFi network SSID.<br/>
_WIFI_PWD_ - Your WiFi network password.

**MQTT server settings**

_MQTT_PORT_ - Camera UI MQTT server port.<br/>
_MQTT_SERVER_ - Camera UI (actually HomeBridge) IP address.<br/>
_MQTT_CLIENT_ID_ - MQTT server client ID, can be any text value.<br/>
_MQTT_USER_NAME_ - Camera UI MQTT server user name.<br/>
_MQTT_PASSWORD_ - Camera UI MQTT server password.<br/>
_MQTT_MOTION_ON_MESSAGE_ - MQTT message send to Camera UI when motion detected.<br/>
_MQTT_MOTION_OFF_MESSAGE_ - MQTT message send to Camera UI to reset motion detection.<br/>

**Camera settings**

You can get more than one camera controlled. Each camera must have the following settings:<br/>
_CAMERA_IP_ - Camera's IP address.<br/>
_CAMERA_MOTION_TOPIC_ - Camera UI MQTT motion detection topic.<br/>
_CAMERA_MAX_MOTION_EVENTS_ - How many motion events should be received from camera before MQTT message will be send to Camera UI.<br/>
_CAMERA_MOTION_COUNTER_RESET_TIMEOUT_ - Timeout after which motion events counter should be reset.<br/>
_CAMERA_MOTION_RESET_TIMEOUT_ - Motion detection reset timeout. When this timeout expires Motion Reset MQTT message will be send to Camera UI.<br/>
_CAMERA_MOTION_DETECTED_LED_ - The motion detection LED pin. Set to 0 if no motion detected indication LED used.<br/>

Should you have any question please do not hesiatate to contact me at mike@btframework.com

## Support

Would you like to support me? That would be great:<br/>
**USD**: https://buymeacoffee.com/dronetales<br/>
**BTC**: bitcoin:1A1WM3CJzdyEB1P9SzTbkzx38duJD6kau<br/>
**BCH**: bitcoincash:qre7s8cnkwx24xpzvvfmqzx6ex0ysmq5vuah42q6yz<br/>
**ETH**: 0xf780b3B7DbE2FC74b5F156cBBE51F67eDeAd8F9a<br/>
 

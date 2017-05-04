# Sonoff Touch Firmware with Alexa support
This is a little Arduino sketch as a firmware replacement for the [Sonoff Touch light switch](https://www.itead.cc/sonoff-touch.html) with Alexa support.

All the hard work was done by [this](http://www.makermusings.com/2015/07/13/amazon-echo-and-home-automation/) guy, so thanks a lot.
If you want more information on how it works read his blog post ;-)

With this firmware you can say something like this: "Alexa, turn on the kitchen light".

Basic setup:
```

// IP settings if you want static ip.
IPAddress ip(192, 168, 178, 50);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);

#define WIFI_SSID "####" // SSID of your wifi
#define WIFI_PASSWORD "####" // Your wifi password

String device_name = "###"; // The device you call alexa for (e.g 'kitchen' -> say "Alexa, turn on the kitchen light")
```

I flashed the firmware with the Arduino IDE with the following settings:

*'Generic ESP8266 board' and Flash: (1M, 64K SPIFFS)*

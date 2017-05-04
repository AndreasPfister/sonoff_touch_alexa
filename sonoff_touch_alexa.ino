#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>

//Default IP setting is DHCP
//if you want a static ip, uncomment and edit.
#define STATIC_IP
#ifdef STATIC_IP
IPAddress ip(192, 168, 178, 50);
IPAddress gateway(192, 168, 178, 1);
IPAddress subnet(255, 255, 255, 0);
#endif

#define LED_PIN 13
#define RELAY_PIN 12
#define BUTTON_PIN 0
#define WIFI_SSID "####" //SSID of your wifi
#define WIFI_PASSWORD "####" //Your wifi password
#define PORT_MULTI 1900 //Port to listen on

WiFiUDP udp;
ESP8266WebServer http(80);
IPAddress ipMulti(239, 255, 255, 250);

boolean udpConnected = false;
boolean wifiConnected = false;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet

String serial;
String persistent_uuid;
String device_name = "Schlafzimmer";

volatile int desiredRelayState = 0;
volatile int relayState = 0;

void setup() {
  Serial.begin(115200);

  // Setup PINS
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  digitalWrite(RELAY_PIN, LOW);

  prepareIds();

  wifiConnected = connectWifi();

  if(wifiConnected){
    udpConnected = connectUDP();
    digitalWrite(LED_PIN, LOW);
    if (udpConnected){
      startHttpServer();
    }
  } else {
    digitalWrite(LED_PIN, HIGH);
  }

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), buttonChangeCallback, CHANGE);
}

void loop() {

  http.handleClient();
  delay(1);

  if (relayState != desiredRelayState) {
      Serial.print("Changing state to ");
      Serial.println(desiredRelayState);

      digitalWrite(RELAY_PIN, desiredRelayState);
      relayState = desiredRelayState;
  }

  // if there's data available, read a packet
  // check if the WiFi and UDP connections were successful
  if(wifiConnected){
    if(udpConnected){
      int packetSize = udp.parsePacket();

      if(packetSize) {
        Serial.println("");
        Serial.print("Received packet of size ");
        Serial.println(packetSize);
        Serial.print("From ");
        IPAddress remote = udp.remoteIP();

        for (int i =0; i < 4; i++) {
          Serial.print(remote[i], DEC);
          if (i < 3) {
            Serial.print(".");
          }
        }

        Serial.print(", port ");
        Serial.println(udp.remotePort());

        int len = udp.read(packetBuffer, 255);

        if (len > 0) {
            packetBuffer[len] = 0;
        }

        String request = packetBuffer;

        if(request.indexOf('M-SEARCH') > 0) {
            if(request.indexOf("urn:Belkin:device:**") > 0) {
                Serial.println("Responding to search request ...");
                respondToSearch();
            }
        }
      }

      delay(10);
    }
  } else {
      // TODO
      // Turn on/off to indicate cannot connect ..
  }
}

void prepareIds() {
  uint32_t chipId = ESP.getChipId();
  char uuid[64];
  sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
        (uint16_t) ((chipId >> 16) & 0xff),
        (uint16_t) ((chipId >>  8) & 0xff),
        (uint16_t)   chipId        & 0xff);

  serial = String(uuid);
  persistent_uuid = "Socket-1_0-" + serial;
}

void respondToSearch() {
    Serial.println("");
    Serial.print("Sending response to ");
    Serial.println(udp.remoteIP());
    Serial.print("Port : ");
    Serial.println(udp.remotePort());

    IPAddress localIP = WiFi.localIP();
    char s[16];
    sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

    String response =
         "HTTP/1.1 200 OK\r\n"
         "CACHE-CONTROL: max-age=86400\r\n"
         "DATE: Fri, 15 Apr 2016 04:56:29 GMT\r\n"
         "EXT:\r\n"
         "LOCATION: http://" + String(s) + ":80/setup.xml\r\n"
         "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
         "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
         "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
         "ST: urn:Belkin:device:**\r\n"
         "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
         "X-User-Agent: redsonic\r\n\r\n";

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(response.c_str());
    udp.endPacket();

    Serial.println("Response sent !");
}

void startHttpServer() {
    http.on("/index.html", HTTP_GET, [](){
      Serial.println("Got Request index.html ...\n");
      http.send(200, "text/plain", "Hello World!");
    });

    http.on("/upnp/control/basicevent1", HTTP_POST, []() {
      Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");

      String request = http.arg(0);
      Serial.print("request:");
      Serial.println(request);

      if(request.indexOf("<BinaryState>1</BinaryState>") > 0) {
          Serial.println("Got Turn on request");
          desiredRelayState = 1;
      }

      if(request.indexOf("<BinaryState>0</BinaryState>") > 0) {
          Serial.println("Got Turn off request");
          desiredRelayState = 0;
      }

      http.send(200, "text/plain", "");
    });

    http.on("/eventservice.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to eventservice.xml ... ########\n");
      String eventservice_xml = "<?scpd xmlns=\"urn:Belkin:service-1-0\"?>"
            "<actionList>"
              "<action>"
                "<name>SetBinaryState</name>"
                "<argumentList>"
                  "<argument>"
                    "<retval/>"
                    "<name>BinaryState</name>"
                    "<relatedStateVariable>BinaryState</relatedStateVariable>"
                    "<direction>in</direction>"
                  "</argument>"
                "</argumentList>"
                 "<serviceStateTable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>BinaryState</name>"
                    "<dataType>Boolean</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                  "<stateVariable sendEvents=\"yes\">"
                    "<name>level</name>"
                    "<dataType>string</dataType>"
                    "<defaultValue>0</defaultValue>"
                  "</stateVariable>"
                "</serviceStateTable>"
              "</action>"
            "</scpd>\r\n"
            "\r\n";

      http.send(200, "text/plain", eventservice_xml.c_str());
    });

    http.on("/setup.xml", HTTP_GET, [](){
      Serial.println(" ########## Responding to setup.xml ... ########\n");

      IPAddress localIP = WiFi.localIP();
      char s[16];
      sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

      String setup_xml = "<?xml version=\"1.0\"?>"
            "<root>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ device_name +"</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Emulated Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<UDN>uuid:"+ persistent_uuid +"</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>"
                "<serviceList>"
                  "<service>"
                      "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                      "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
              "</serviceList>"
              "</device>"
            "</root>\r\n"
            "\r\n";

        http.send(200, "text/xml", setup_xml.c_str());

        Serial.print("Sending :");
        Serial.println(setup_xml);
    });

    http.begin();
    Serial.println("HTTP Server started ..");
}


boolean connectWifi(){
  boolean state = true;
  int i = 0;

  WiFi.mode(WIFI_STA);
#ifdef STATIC_IP
  WiFi.config(ip, gateway, subnet);
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 10){
      state = false;
      break;
    }
    i++;
  }

  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(WIFI_SSID);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
  }

  return state;
}

boolean connectUDP(){
  boolean state = false;

  Serial.println("");
  Serial.println("Connecting to UDP");

  if(udp.beginMulticast(WiFi.localIP(), ipMulti, PORT_MULTI)) {
    Serial.println("Connection successful");
    state = true;
  }
  else{
    Serial.println("Connection failed");
  }

  return state;
}

void buttonPressed() {
  desiredRelayState = !desiredRelayState; //Toggle relay state.
}

void buttonChangeCallback() {
  if (digitalRead(0) == 1) {
    buttonPressed();
  }
  else {
    //Just been pressed - do nothing until released.
  }
}


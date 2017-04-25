#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <BlynkSimpleEsp8266.h>
#include <SPI.h>
#include <bitBangedSPI.h>
#include <MAX7219_Dot_Matrix.h>

#define OTA_UPDATES
#define OTA_HOSTNAME    "POWER-MONITOR"

#define emonTxV3                 // Wemos D1 mini tolerates up to 3.3V
#include "EmonLib.h"             // Include Emon Library
EnergyMonitor emon1;             // Create an instance

// 4 chips (display module), hardware SPI with load on D2
MAX7219_Dot_Matrix display (4, 2);  // Chips / LOAD 

char message [] = "Booting...";

#define BLYNK_PRINT Serial
char auth[] = "__replace_your_own__";

const char* ssid = "__replace_your_own__";
const char* password = "__replace_your_own__";

unsigned long lastMoved = 0;
unsigned long MOVE_INTERVAL = 20;  // mS
int  messageOffset;

int adc = 0;

const long price = 0.6*24*30; // price kwh * 24 hours * 30 days

void updateDisplay ()
  {
  display.sendSmooth (message, messageOffset);
  
  // next time show one pixel onwards
  if (messageOffset++ >= (int) (strlen (message) * 8))
    messageOffset = - chips * 8;
  }  // end of updateDisplay

int calcPower() {

  double Irms = emon1.calcIrms(1480);  // Calculate Irms only

  Serial.print(analogRead(A0));
  Serial.print(" ");
  Serial.print(Irms*236.0);            // Apparent power
  Serial.print(" ");
  Serial.println(Irms);                // Irms
  Serial.println();

  char buf [20];
  sprintf (buf, "%.0fWh", Irms*236 );  // I measured 236V on my mains
  Serial.println(buf);
  display.sendString (buf);
  return Irms*236;
}

BlynkTimer timer;

// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void readPower()
{
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V1, calcPower());
}

BLYNK_READ(V2)
{
  // This command writes only when we ask data from it
  Blynk.virtualWrite(V2, calcPower());
}

BLYNK_WRITE(V0)
{
  double pinValue = param.asDouble(); // assigning incoming value from pin V0 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("Calibrate value is: ");
  Serial.println(pinValue);
  emon1.current(A0, pinValue);
}

void setup() {
  emon1.current(A0, 23.4);             // Current: input pin, calibration (2000/burden resistance)
  display.begin ();
  display.setIntensity (1);
  display.sendString("..");
  Serial.begin(115200);
  Serial.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  // ArduinoOTA.setHostname("myesp8266");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  display.sendString("OK");
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  Blynk.begin(auth, ssid, password);

  // Setup a function to be called every second
  timer.setInterval(1000L, readPower);
}
  
void loop() {
  ArduinoOTA.handle();
  Blynk.run();
  timer.run(); // Initiates BlynkTimer
//calcPower();
}

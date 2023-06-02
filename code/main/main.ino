#include <Adafruit_TLA202x.h>
#include "src/analogWrite.h"
#include <ESPAsyncWebServer.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>

#define SERIES_RESISTOR 10

Adafruit_TLA202x ADC;

struct interruption_flags {
    bool button = false;
    bool button_release = true;
    bool heating = false;
} interruption_flags;

float temperature=25;

//WEBSERVER
AsyncWebServer server(80);


// ---------------------- main functions ---------------------------

void setup() {
    Serial.begin(115200);
    pinMode(19,INPUT_PULLUP);
    pinMode(23, OUTPUT);
    analogWriteFrequency(20000);

    if (!ADC.begin(0x48))
      Serial.println("Failed to find TLA202x chip");

    ADC.setChannel((tla202x_channel_t) 0);

    serverBegin();
}

void loop() {
    interruption_control();     
}

// ---------------------- complementary functions -------------------------

void interruption_control () {
    check_interruptions();
    execute_interruptions();
}

void check_interruptions () {
    if (digitalRead(19) == LOW) {
        if (interruption_flags.button_release) //if it's the first time we click on the button
        {
            interruption_flags.button = true;
            interruption_flags.button_release = false;
            delay(100);
        }
    } else  { // if the button is not pressed we release the flag
        interruption_flags.button_release = true;
    }
}

void execute_interruptions () {
    if (interruption_flags.button) {
        button_interruption();
    }

    if (interruption_flags.heating) {
        update_temperature();
        if (temperature < 63)
            analogWrite(23, 20);
        else
            analogWrite(23, 0);
    } else {
        analogWrite(23, 0);
    }
}

void button_interruption () {
    interruption_flags.heating = !interruption_flags.heating;
    if (interruption_flags.heating) {
        Serial.println("heating on");
    } else {
        Serial.println("heating off");
    }
    interruption_flags.button = false;
}

float calculate_resistance ()
{
    float voltage = ADC.readVoltage();
    float resistance =  1000 * ( SERIES_RESISTOR * voltage) / (3.3 /*Vcc*/ - voltage);
    return resistance;
}

void update_temperature () {
    float resistance = calculate_resistance();
    temperature = temperature_model(resistance);
}

float temperature_model(float measured_resistance)
{

  //https://www.thinksrs.com/downloads/programs/Therm%20Calc/NTCCalibrator/NTCcalculator.htm
  float a = 2.403870609e-3;
  float b = 0.5503277505e-4;
  float c = 6.137700401e-07;

  float LogR = log(measured_resistance);

  float result = (1.0 / (a + b*LogR + c*LogR*LogR*LogR))-273.15;
  
  return result;

}

// server functions

void serverBegin() {
    connect_wifi(40000);
    MDNS.addService("http", "tcp", 80);
}

void connect_wifi(int time_trying) {

  Serial.println("\n");
  char* ssid = "V2023";
  char* password = ".cri2021";
  char* mDNS = "reflow";

  //Initialize the mDNS
  if (!MDNS.begin(mDNS)) {
    Serial.println("[ERROR] Error setting up MDNS responder!");
  }

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(ssid, password);
  delay(1000);
  
  Serial.print("[INFO] Connecting to " + String(ssid) + ":" + String(password) + "...");

  int dot_counter=0;
  int timeout = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - timeout < time_trying) {

    if (dot_counter > 50)
    {
      Serial.println(".");
      dot_counter=0;
    } else {
      Serial.print(".");
      dot_counter++;
    }
    
    delay(100);
  } 

  if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n");  
      Serial.println("[INFO] Connected !");
      Serial.print("[INFO] IP: ");
      Serial.println(WiFi.localIP());
      // Print gateway
      Serial.print("[INFO] Gateway: ");
      Serial.println(WiFi.gatewayIP());
      // Print subnet
      Serial.print("[INFO] Subnet: ");
      Serial.println(WiFi.subnetMask());

      server.on("/res", SendResistance);    
    

      
      server.onNotFound(handleNotFound);  
      server.begin();  
      Serial.println("[INFO] Server online at http://" + WiFi.localIP().toString() +"/ or http://" + mDNS + ".local/");
  } else {
      Serial.println("[ERROR] \nConnection failed! Offline mode On.");
      // wifi off
      WiFi.mode(WIFI_OFF);
    }

}

void SendResistance (AsyncWebServerRequest *request)
{
  analogWrite(23,0);
  delay(500);
  float resistance = calculate_resistance();
  request->send(200, "text/plain", String(resistance));
  Serial.println("[INFO] Resistance requested and sent.");
  if (interruption_flags.heating) {
        analogWrite(23, 20);
    }
}

void handleNotFound(AsyncWebServerRequest *request) {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nParameters: ";
  int paramsNr = request->params();
  // add the AsyncWebServer arguments to the message
  message += paramsNr;
  message += "\n";

   for(int i=0; i<paramsNr; i++) {
    AsyncWebParameter* p = request->getParam(i);
    message += " " + p->name() + ": " + p->value() + "\n";
  }
 
  request->send(404, "text/plain", message);
}
#include <Adafruit_TLA202x.h>

Adafruit_TLA202x ADC;

struct interruption_flags {
    bool button = false;
} interruption_flags;

// ---------------------- main functions ---------------------------

void setup() {
    Serial.begin(115200);
    pinMode(19,INPUT_PULLUP);

    if (!ADC.begin(0x48))
      Serial.println("Failed to find TLA202x chip");

    ADC.setChannel((tla202x_channel_t) 0);
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
        interruption_flags.button = true;
    }
}

void execute_interruptions () {
    if (interruption_flags.button) {
        button_interruption();
    }
}

void button_interruption () {
    float voltage = ADC.readVoltage();
    float resistance = 1000 * ( SERIES_RESISTOR * voltage) / (5 /*Vcc*/ - voltage);
    float temperature = temperature_model(resistance);

    Serial.println("Sensor reading: " + temperature);
    interruption_flags.button = false;
}

float temperature_model(float measured_resistance)
{

  //https://www.thinksrs.com/downloads/programs/Therm%20Calc/NTCCalibrator/NTCcalculator.htm
  float a = 1.130496169e-3;
  float b = 2.343275854e-4;
  float c = 0.8385685197e-07;

  float LogR = log(measured_resistance);

  float result = (1.0 / (a + b*LogR + c*LogR*LogR*LogR))-273.15;
  
  return result;
  
}
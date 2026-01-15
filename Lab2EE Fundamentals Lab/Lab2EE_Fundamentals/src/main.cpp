#include <Arduino.h>

const int PIN_VOUT1 = D0;
const int PIN_VOUT2 = D1;

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("Lab2: Reading VOUT1(D0) and VOUT2(D1)");
}

void loop() {
  int adc1 = analogRead(PIN_VOUT1);
  int adc2 = analogRead(PIN_VOUT2);

  float v1 = (adc1 / 4095.0) * 3.3;
  float v2 = (adc2 / 4095.0) * 3.3;

  Serial.print("VOUT1: ");
  Serial.print(v1, 3);
  Serial.print(" V (ADC=");
  Serial.print(adc1);
  Serial.print(") | VOUT2: ");
  Serial.print(v2, 3);
  Serial.print(" V (ADC=");
  Serial.print(adc2);
  Serial.println(")");

  delay(500);
}

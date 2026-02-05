#include <Arduino.h>

#define TRIG_PIN 3  // D2
#define ECHO_PIN 4  // D3

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  delay(1000);
  Serial.println("Ultrasonic sensor test");
}

void loop() {
  // 发送触发脉冲
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // 读取回波时间
  long duration = pulseIn(ECHO_PIN, HIGH);
  
  // 计算距离 (cm)
  float distance = duration * 0.034 / 2;

  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");

  delay(500);
}
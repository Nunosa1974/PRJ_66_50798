#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  delay(2000);

  Serial.println("Recetor...");

  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETETADO");
    while (1);
  }

  Serial.println("✅ NRF OK");

  // CONFIG (igual ao transmissor)
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(false);   // 🔥 importante aqui também

  radio.startListening();
  radio.openReadingPipe(0, address);

  Serial.println("À espera...");
}

void loop() {
  if (radio.available()) {
    char text[32] = "";

    radio.read(&text, sizeof(text));

    Serial.print("📩 Recebido: ");
    Serial.println(text);

    digitalWrite(LED_PIN, HIGH);
    delay(100);
    digitalWrite(LED_PIN, LOW);
  }
}
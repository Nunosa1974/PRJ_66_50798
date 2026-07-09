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

  Serial.println("Transmissor...");

  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETETADO");
    while (1);
  }

  Serial.println("✅ NRF OK");

  // CONFIG
  radio.setChannel(76);
  radio.setDataRate(RF24_1MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(false);

  radio.stopListening();
  radio.openWritingPipe(address);

  randomSeed(analogRead(0)); // inicializar random
}

void loop() {

  const char* text;  // 👈 DECLARADO FORA

  int val = random(1, 6);

  switch(val){
    case 1: text = "Ola"; break;
    case 2: text = "Batata"; break;
    case 3: text = "Bonito"; break;
    case 4: text = "Feliz"; break;
    case 5: text = "Marisco"; break;
  }

  radio.write(text, strlen(text) + 1);

  Serial.print("📤 Enviado: ");
  Serial.println(text);

  digitalWrite(LED_PIN, HIGH);
  delay(100);
  digitalWrite(LED_PIN, LOW);

  delay(10);
}
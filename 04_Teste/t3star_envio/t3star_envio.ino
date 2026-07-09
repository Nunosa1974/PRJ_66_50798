#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

void setup() {
  Serial.begin(115200);
  delay(2000);

  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETETADO");
    while (1);
  }

  Serial.println("✅ NRF PRONTO");

  radio.openWritingPipe(address);
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening();
}

void loop() {
  const char text[] = "Teste";

  bool ok = radio.write(&text, sizeof(text));

  if (ok) {
    Serial.println("✅ ENVIOU COM SUCESSO");
  } else {
    Serial.println("❌ FALHA NO ENVIO");
  }

  delay(1000);
}
#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

void setup() {
  Serial.begin(115200);

  radio.begin();
  radio.openWritingPipe(address);
  radio.openReadingPipe(1, address);

  radio.setPALevel(RF24_PA_HIGH); // mais potência para teste

  radio.stopListening();

  Serial.println("Transmissor pronto...");
}

void loop() {
  char msg[] = "ping";

  // enviar
  radio.stopListening();
  bool ok = radio.write(&msg, sizeof(msg));

  if (!ok) {
    Serial.println("Erro ao enviar");
  }

  // esperar resposta
  radio.startListening();

  unsigned long start = millis();
  while (!radio.available()) {
    if (millis() - start > 200) {
      Serial.println("Timeout");
      delay(1000);
      return;
    }
  }

  char response[10] = "";
  radio.read(&response, sizeof(response));

  Serial.print("Recebi: ");
  Serial.println(response);

  delay(1000);
}
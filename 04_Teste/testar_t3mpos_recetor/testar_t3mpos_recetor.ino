#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

void setup() {
  Serial.begin(115200);

  radio.begin();
  radio.openReadingPipe(1, address);
  radio.openWritingPipe(address);

  radio.setPALevel(RF24_PA_HIGH);

  radio.startListening();

  Serial.println("Recetor pronto...");
}

void loop() {
  if (radio.available()) {
    char msg[10] = "";
    radio.read(&msg, sizeof(msg));

    Serial.print("Recebi: ");
    Serial.println(msg);

    // responder (eco)
    radio.stopListening();
    bool ok = radio.write(&msg, sizeof(msg));
    radio.startListening();

    if (!ok) {
      Serial.println("Erro ao responder");
    }
  }
}
#include <SPI.h>
#include <RF24.h>

// ===== PINS =====
#define CE_PIN    7
#define CSN_PIN   10
#define SCK_PIN   4
#define MOSI_PIN  6
#define MISO_PIN  5

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "ABCDE";

unsigned long sendTime;
int packetCount = 0;
unsigned long startTime;

void setup() {
  Serial.begin(115200);
  delay(2000);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);

  if (!radio.begin()) {
    Serial.println("ERRO: NRF24 não encontrado!");
    while (1);
  }

  Serial.println("TRANSMISSOR OK - Iniciando envio...");

  // Configurações
  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(120);
  radio.setAutoAck(false);

  // Pipe de envio
  radio.openWritingPipe(address);
  
  // Modo transmissão
  radio.stopListening();
  
  startTime = micros();
}

void loop() {
  // Guardar timestamp local antes de enviar
  sendTime = micros();
  
  // Enviar timestamp
  bool success = radio.write(&sendTime, sizeof(sendTime));
  
  packetCount++;
  
  // Mostrar estatísticas a cada 100 pacotes
  if (packetCount % 100 == 0) {
    unsigned long elapsed = (micros() - startTime) / 1000; // em ms
    Serial.print("TX: ");
    Serial.print(packetCount);
    Serial.print(" pacotes enviados em ");
    Serial.print(elapsed);
    Serial.print(" ms | Taxa: ");
    Serial.print((packetCount * 1000.0) / elapsed);
    Serial.println(" pps");
  }
  
  // Pequeno delay para não saturar o buffer
  delay(10);
}
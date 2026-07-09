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

unsigned long receivedTime;
unsigned long previousReceiveTime = 0;
unsigned long previousSentTime = 0;
unsigned long lastDisplayTime = 0;
int packetCount = 0;
unsigned long latencySum = 0;

void setup() {
  Serial.begin(115200);
  delay(2000);

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);

  if (!radio.begin()) {
    Serial.println("NRF FAIL");
    while (1);
  }

  Serial.println("RECEPTOR OK - Modo sem acumulação");

  radio.setPALevel(RF24_PA_MIN);
  radio.setDataRate(RF24_1MBPS);
  radio.setChannel(120);
  radio.setAutoAck(false);
  
  radio.openReadingPipe(1, address);
  radio.startListening();
}

void loop() {
  if (radio.available()) {
    unsigned long receiveMicros = micros();
    radio.read(&receivedTime, sizeof(receivedTime));
    
    packetCount++;
    
    // MÉTODO 1: Medir diferença entre INTERVALOS de chegada
    // Isso elimina completamente o problema de drift
    if (previousReceiveTime > 0 && previousSentTime > 0) {
      // Intervalo real entre chegadas dos pacotes
      unsigned long actualInterval = receiveMicros - previousReceiveTime;
      
      // Intervalo esperado (baseado nos timestamps enviados)
      unsigned long expectedInterval = receivedTime - previousSentTime;
      
      // A diferença entre intervalos é a variação de latência
      long latencyVariation = (long)actualInterval - (long)expectedInterval;
      
      // A latência atual = latência anterior + variação
      static long currentLatency = 0;
      currentLatency += latencyVariation;
      
      // Evitar valores negativos
      if (currentLatency < 0) currentLatency = 0;
      
      // Mostrar a cada 10 pacotes
      if (packetCount % 10 == 0) {
        Serial.print("Latencia: ");
        Serial.print(currentLatency);
        Serial.println(" us");
      }
    }
    
    // Guardar para próximo cálculo
    previousReceiveTime = receiveMicros;
    previousSentTime = receivedTime;
    
    // MÉTODO ALTERNATIVO: Mostrar também a diferença bruta (apenas para debug)
    if (packetCount <= 5 || packetCount % 50 == 0) {
      long rawDiff = (long)receiveMicros - (long)receivedTime;
      Serial.print("  [debug] diff bruta: ");
      Serial.print(rawDiff);
      Serial.println(" us");
    }
  }
}
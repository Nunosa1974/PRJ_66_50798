#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8
#define MAX_GLOVES 6

RF24 radio(CE_PIN, CSN_PIN);

// Endereços para cada luva
const byte addresses[][6] = {
  "00002",  // Pipe 0 - Luva 0
  "10002",  // Pipe 1 - Luva 1
  "20002",  // Pipe 2 - Luva 2
  "30002",  // Pipe 3 - Luva 3
  "40002",  // Pipe 4 - Luva 4
  "50002"   // Pipe 5 - Luva 5
};

// Estrutura do pacote
#pragma pack(push, 1)
typedef struct {
  uint8_t header;
  uint8_t glove_id;
  int16_t accel[3];
  int16_t gyro[3];
  int16_t mag[3];
  int16_t fsr[4];
  uint8_t sequence;
  uint16_t timestamp;
  uint8_t checksum;
} Packet;
#pragma pack(pop)

Packet packet;

// ========================
// 🔐 CHECKSUM
// ========================
uint8_t calculateChecksum(Packet *p) {
  uint8_t *data = (uint8_t*)p;
  uint8_t sum = 0;
  for (int i = 0; i < sizeof(Packet) - 1; i++) {
    sum ^= data[i];
  }
  return sum;
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(2000);

  Serial.println("==========================================");
  Serial.println("RECEPTOR MULTI-LUVA");
  Serial.println("==========================================");
  Serial.println();

  // Verificar NRF24L01
  if (!radio.begin()) {
    Serial.println("❌ NRF24L01 NÃO DETECTADO!");
    while (1);
  }
  Serial.println("✅ NRF24L01 OK");

  // Configurações
  radio.setChannel(80);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(false);
  
  // Abrir pipes para todas as luvas
  for(int i = 0; i < MAX_GLOVES; i++) {
    radio.openReadingPipe(i, addresses[i]);
  }
  
  radio.startListening();

  Serial.println("📡 Configurações:");
  Serial.println("  - Canal: 80");
  Serial.println("  - Taxa: 2MBPS");
  Serial.println("  - Potência: LOW");
  Serial.println("  - AutoAck: false");
  Serial.print("  - Luvas configuradas: ");
  Serial.println(MAX_GLOVES);
  Serial.println();
  Serial.println("✅ Aguardando dados das luvas...");
  Serial.println();
}

// ========================
// 🔁 LOOP PRINCIPAL
// ========================
void loop() {
  // Verificar se chegou algum dado
  if (radio.available()) {
    uint8_t pipeNum;  // Qual luva enviou?
    
    // Ler o pacote
    radio.read(&packet, sizeof(Packet), &pipeNum);
    
    // Validar checksum
    uint8_t calc = calculateChecksum(&packet);
    if (calc != packet.checksum) {
      Serial.println("❌ Checksum inválido");
      return;
    }
    
    // Mostrar dados recebidos
    printPacket(packet, pipeNum);
    
    // LED pisca ao receber
    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(500);
    digitalWrite(LED_PIN, LOW);
  }
}

// ========================
// 📄 IMPRIMIR PACOTE
// ========================
void printPacket(Packet &pkt, uint8_t gloveId) {
  // Decodificar horas e minutos
  uint8_t hora = (pkt.timestamp >> 8) & 0xFF;
  uint8_t minuto = pkt.timestamp & 0xFF;
  
  // Mostrar qual luva e número de sequência
  Serial.print("📦 [Luva ");
  Serial.print(gloveId);
  Serial.print("] [#");
  Serial.print(pkt.sequence);
  Serial.print("] ⏰ ");
  Serial.print(hora);
  Serial.print(":");
  if (minuto < 10) Serial.print("0");
  Serial.print(minuto);
  
  // Mostrar ACC
  Serial.print(" | ACC [");
  Serial.print(pkt.accel[0]); 
  Serial.print(",");
  Serial.print(pkt.accel[1]); 
  Serial.print(",");
  Serial.print(pkt.accel[2]);
  Serial.print("]");
  
  // Mostrar GYRO
  Serial.print(" | GYR [");
  Serial.print(pkt.gyro[0]); 
  Serial.print(",");
  Serial.print(pkt.gyro[1]); 
  Serial.print(",");
  Serial.print(pkt.gyro[2]);
  Serial.print("]");
  
  // Mostrar MAG
  Serial.print(" | MAG [");
  Serial.print(pkt.mag[0]); 
  Serial.print(",");
  Serial.print(pkt.mag[1]); 
  Serial.print(",");
  Serial.print(pkt.mag[2]);
  Serial.print("]");
  
  // Mostrar FSR
  Serial.print(" | FSR [");
  Serial.print(pkt.fsr[0]); 
  Serial.print(",");
  Serial.print(pkt.fsr[1]); 
  Serial.print(",");
  Serial.print(pkt.fsr[2]); 
  Serial.print(",");
  Serial.print(pkt.fsr[3]);
  Serial.print("]");
  
  Serial.println();
}
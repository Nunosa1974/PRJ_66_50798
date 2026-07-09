#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define SCK_PIN 4
#define MOSI_PIN 6
#define MISO_PIN 5

// Endereço deve ser igual ao do transmissor
const byte address[6] = "00002";

// Estrutura do pacote (igual à do transmissor)
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

RF24 radio(CE_PIN, CSN_PIN);
Packet packet;

unsigned long lastReceiveTime = 0;
int packetsReceived = 0;
int packetsFailed = 0;

uint8_t calculateChecksum(Packet *p) {
  uint8_t *data = (uint8_t*)p;
  uint8_t sum = 0;
  for (int i = 0; i < sizeof(Packet) - 1; i++) sum ^= data[i];
  return sum;
}

bool verifyChecksum(Packet *p) {
  uint8_t calculated = calculateChecksum(p);
  return (calculated == p->checksum);
}

void printPacket() {
  Serial.println("========================================");
  Serial.print("Header: 0x"); Serial.println(packet.header, HEX);
  Serial.print("Glove ID: "); Serial.println(packet.glove_id);
  Serial.print("Sequence: "); Serial.println(packet.sequence);
  
  // Decodificar timestamp
  uint8_t hours = (packet.timestamp >> 8) & 0xFF;
  uint8_t minutes = packet.timestamp & 0xFF;
  Serial.print("Timestamp: ");
  Serial.print(hours); Serial.print(":"); 
  Serial.print(minutes); Serial.print(":");
  Serial.println(millis() / 1000);
  
  // Acelerômetro
  Serial.print("Accel (x,y,z): ");
  Serial.print(packet.accel[0]); Serial.print(", ");
  Serial.print(packet.accel[1]); Serial.print(", ");
  Serial.println(packet.accel[2]);
  
  // Giroscópio
  Serial.print("Gyro (x,y,z): ");
  Serial.print(packet.gyro[0]); Serial.print(", ");
  Serial.print(packet.gyro[1]); Serial.print(", ");
  Serial.println(packet.gyro[2]);
  
  // Magnetômetro
  Serial.print("Mag (x,y,z): ");
  Serial.print(packet.mag[0]); Serial.print(", ");
  Serial.print(packet.mag[1]); Serial.print(", ");
  Serial.println(packet.mag[2]);
  
  // Sensores FSR
  Serial.print("FSR (1-4): ");
  for(int i = 0; i < 4; i++) {
    Serial.print(packet.fsr[i]);
    if(i < 3) Serial.print(", ");
  }
  Serial.println();
  
  Serial.print("Checksum: 0x"); Serial.print(packet.checksum, HEX);
  Serial.print(" | Valid: ");
  Serial.println(verifyChecksum(&packet) ? "YES" : "NO");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Iniciando Recetor ESP32-C3...");
  
  // Inicializar SPI com os pinos corretos
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CSN_PIN);
  
  // Inicializar rádio
  if (!radio.begin()) {
    Serial.println("Erro: nRF24L01 não encontrado!");
    Serial.print("Verifique as conexões:");
    Serial.print(" CE="); Serial.print(CE_PIN);
    Serial.print(" CSN="); Serial.print(CSN_PIN);
    Serial.print(" SCK="); Serial.print(SCK_PIN);
    Serial.print(" MOSI="); Serial.print(MOSI_PIN);
    Serial.print(" MISO="); Serial.print(MISO_PIN);
    while (1) {
      delay(1000);
    }
  }
  
  Serial.println("nRF24L01 inicializado com sucesso!");
  
  // Configurar o rádio (deve corresponder às configurações do transmissor)
  radio.setChannel(80);           // Mesmo canal do transmissor
  radio.setDataRate(RF24_2MBPS);  // Mesma taxa
  radio.setPALevel(RF24_PA_LOW);  // Mesmo nível de potência
  radio.setAutoAck(false);        // Auto-ACK desabilitado (igual ao transmissor)
  
  // Abrir pipe para leitura
  radio.openReadingPipe(0, address);
  radio.startListening();
  
  Serial.println("Recetor configurado e aguardando dados...");
  Serial.println("========================================\n");
}

void loop() {
  if (radio.available()) {
    // Ler o pacote
    radio.read(&packet, sizeof(Packet));
    lastReceiveTime = millis();
    
    // Contar pacotes recebidos
    packetsReceived++;
    
    // Verificar se é um pacote válido (header 0xAA)
    if (packet.header == 0xAA) {
      Serial.print("[");
      Serial.print(millis() / 1000);
      Serial.print("s] Pacote #");
      Serial.print(packetsReceived);
      Serial.print(" (SEQ:");
      Serial.print(packet.sequence);
      Serial.print(") - ");
      
      if (verifyChecksum(&packet)) {
        Serial.println("CHECKSUM OK");
        printPacket();
      } else {
        Serial.println("CHECKSUM ERROR!");
        packetsFailed++;
      }
    } else {
      Serial.println("Header inválido!");
      packetsFailed++;
    }
    
    // Estatísticas a cada 10 pacotes
    if (packetsReceived % 10 == 0) {
      float successRate = ((packetsReceived - packetsFailed) * 100.0) / packetsReceived;
      Serial.println("\n--- ESTATÍSTICAS ---");
      Serial.print("Pacotes recebidos: "); Serial.println(packetsReceived);
      Serial.print("Pacotes com erro: "); Serial.println(packetsFailed);
      Serial.print("Taxa de sucesso: "); Serial.print(successRate); Serial.println("%");
      Serial.println("--------------------\n");
    }
  }
  
  // Verificar se perdeu conexão (timeout de 1 segundo)
  if (millis() - lastReceiveTime > 2000 && lastReceiveTime != 0) {
    Serial.println("⚠️ Timeout: Nenhum dado recebido por 2 segundos!");
    lastReceiveTime = millis(); // Reset para não spammar
  }
}
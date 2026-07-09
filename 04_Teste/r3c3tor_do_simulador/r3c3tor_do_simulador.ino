#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

// ========================
// 📦 PACKET STRUCT (igual ao transmissor)
// ========================
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

uint8_t lastSequence = 0;
bool firstPacket = true;
unsigned long lastReceiveTime = 0;
int packetsReceived = 0;
int packetsLost = 0;

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

  Serial.println("=== RECETOR NRF24L01 ===");
  Serial.println();

  // Inicializar NRF24
  if (!radio.begin()) {
    Serial.println("❌ NRF24 NÃO DETECTADO!");
    Serial.println("Verifique as ligações:");
    Serial.println("  - VCC → 3.3V");
    Serial.println("  - GND → GND");
    Serial.println("  - CE  → Pino 7");
    Serial.println("  - CSN → Pino 10");
    Serial.println("  - SCK → Pino 13");
    Serial.println("  - MOSI → Pino 11");
    Serial.println("  - MISO → Pino 12");
    while (1) {
      delay(1000);
      Serial.print(".");
    }
  }

  Serial.println("✅ NRF24 inicializado com sucesso!");
  
  // ⚠️ CONFIGURAÇÕES - IDÊNTICAS AO TRANSMISSOR
  radio.setChannel(76);           // Mesmo canal
  radio.setDataRate(RF24_2MBPS);  // Mesma taxa
  radio.setPALevel(RF24_PA_LOW);  // Mesmo nível de potência
  
  // 🔥 IMPORTANTE: Desativar AutoACK (igual ao transmissor)
  radio.setAutoAck(false);        // ← AGORA IGUAL AO TX!
  
  // Configurar pipe de leitura
  radio.openReadingPipe(0, address);
  
  // Começar a escutar
  radio.startListening();
  
  // Pequeno delay para estabilizar
  delay(100);

  Serial.println("📡 Configuração completa:");
  Serial.println("  - Canal: 76");
  Serial.println("  - Taxa: 2 Mbps");
  Serial.println("  - Potência: LOW");
  Serial.println("  - AutoACK: DESLIGADO");
  Serial.println("  - Endereço: 00001");
  Serial.println();
  Serial.println("✅ Aguardando dados do transmissor...");
  Serial.println("----------------------------------------");
}

// ========================
// 🔁 LOOP PRINCIPAL
// ========================
void loop() {
  // Verificar se há pacote disponível
  if (radio.available()) {
    
    // Ler pacote
    radio.read(&packet, sizeof(Packet));
    
    // Verificar tamanho (opcional, para debug)
    if (radio.getPayloadSize() != sizeof(Packet)) {
      Serial.print("⚠️ Tamanho inesperado: ");
      Serial.print(radio.getPayloadSize());
      Serial.print(" bytes (esperado: ");
      Serial.print(sizeof(Packet));
      Serial.println(" bytes)");
    }
    
    // 🔍 Validar header
    if (packet.header != 0xAA) {
      Serial.print("⚠️ Header inválido: 0x");
      Serial.println(packet.header, HEX);
      return;
    }
    
    // 🔐 Validar checksum
    uint8_t calc = calculateChecksum(&packet);
    if (calc != packet.checksum) {
      Serial.print("❌ Checksum inválido! Esperado: 0x");
      Serial.print(packet.checksum, HEX);
      Serial.print(" Calculado: 0x");
      Serial.println(calc, HEX);
      return;
    }
    
    // 📊 Estatísticas de recepção
    packetsReceived++;
    unsigned long now = millis();
    
    // Detetar perdas de pacote
    if (!firstPacket) {
      uint8_t expected = (lastSequence + 1) & 0xFF; // & 0xFF para overflow
      uint8_t lost = packet.sequence - expected;
      
      if (packet.sequence != expected) {
        packetsLost += lost;
        Serial.print("⚠️ PERDA DE PACOTE! Esperado: ");
        Serial.print(expected);
        Serial.print(" | Recebido: ");
        Serial.print(packet.sequence);
        Serial.print(" | Perdidos: ");
        Serial.print(lost);
        Serial.print(" | Total perdidos: ");
        Serial.println(packetsLost);
      }
    }
    
    // Atualizar sequência
    firstPacket = false;
    lastSequence = packet.sequence;
    lastReceiveTime = now;
    
    // Mostrar dados do pacote
    printPacket();
    
    // Piscar LED rapidamente
    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(500);  // Aumentado para ser visível
    digitalWrite(LED_PIN, LOW);
  }
  
  // Verificar timeout (se não recebe dados há mais de 3 segundos)
  static unsigned long lastStatusTime = 0;
  if (millis() - lastStatusTime > 5000) {  // A cada 5 segundos
    if (millis() - lastReceiveTime > 3000) {
      Serial.println("⚠️ SEM DADOS - Verifique o transmissor");
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
      delay(100);
      digitalWrite(LED_PIN, HIGH);
      delay(100);
      digitalWrite(LED_PIN, LOW);
    }
    lastStatusTime = millis();
  }
}

// ========================
// 📊 PRINT PACKET
// ========================
void printPacket() {
  static int packetCount = 0;
  packetCount++;
  
  Serial.print("[");
  Serial.print(packetCount);
  Serial.print("] ");
  
  Serial.print("SEQ:");
  Serial.print(packet.sequence);
  
  // Decodificar timestamp
  uint8_t hora = (packet.timestamp >> 8) & 0xFF;
  uint8_t minuto = packet.timestamp & 0xFF;
  
  Serial.print(" | ⏰ ");
  if (hora < 10) Serial.print("0");
  Serial.print(hora);
  Serial.print(":");
  if (minuto < 10) Serial.print("0");
  Serial.print(minuto);
  
  Serial.print(" | 🎯 Acc:");
  Serial.print(packet.accel[0]); Serial.print(",");
  Serial.print(packet.accel[1]); Serial.print(",");
  Serial.print(packet.accel[2]);
  
  Serial.print(" | 🌀 Gir:");
  Serial.print(packet.gyro[0]); Serial.print(",");
  Serial.print(packet.gyro[1]); Serial.print(",");
  Serial.print(packet.gyro[2]);
  
  Serial.print(" | 🧭 Mag:");
  Serial.print(packet.mag[0]); Serial.print(",");
  Serial.print(packet.mag[1]); Serial.print(",");
  Serial.print(packet.mag[2]);
  
  Serial.print(" | 👆 FSR:");
  Serial.print(packet.fsr[0]); Serial.print(",");
  Serial.print(packet.fsr[1]); Serial.print(",");
  Serial.print(packet.fsr[2]); Serial.print(",");
  Serial.print(packet.fsr[3]);
  
  Serial.print(" | ✅ Checksum:0x");
  Serial.print(packet.checksum, HEX);
  
  Serial.println();
  
  // Estatísticas a cada 10 pacotes
  if (packetCount % 10 == 0) {
    Serial.println("----------------------------------------");
    Serial.print("📊 Estatísticas: ");
    Serial.print(packetsReceived);
    Serial.print(" recebidos, ");
    Serial.print(packetsLost);
    Serial.print(" perdidos (");
    if (packetsReceived + packetsLost > 0) {
      float lossRate = (float)packetsLost / (packetsReceived + packetsLost) * 100;
      Serial.print(lossRate, 1);
      Serial.print("% perda)");
    }
    Serial.println();
    Serial.println("----------------------------------------");
  }
}
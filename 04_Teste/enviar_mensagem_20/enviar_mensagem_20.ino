#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00002";

// ========================
// 📦 PACKET STRUCT (32 bytes)
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
  uint16_t timestamp;  // horas (8 bits) + minutos (8 bits)

  uint8_t checksum;
} Packet;
#pragma pack(pop)

Packet packet;

uint8_t sequence = 0;
unsigned long lastSend = 0;
const int interval = 10; // 10 pacotes por segundo (ajustável: 1000=1Hz, 100=10Hz, 50=20Hz, 20=50Hz)

// Variáveis para o tempo SIMULADO (automático, sem necessidade de receber hora do PC)
unsigned long lastTimeUpdate = 0;
int simulatedSeconds = 0;
int simulatedMinutes = 0;
int simulatedHours = 0;

// Variável para controlar o padrão rotativo dos sensores
int pattern_index = 0;

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
// ⏰ ATUALIZAR TEMPO SIMULADO
// ========================
void updateSimulatedTime() {
  unsigned long currentMillis = millis();
  
  // Atualizar a cada 1000ms (1 segundo)
  if (currentMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = currentMillis;
    simulatedSeconds++;
    
    // Gerenciar minutos e horas
    if (simulatedSeconds >= 60) {
      simulatedSeconds = 0;
      simulatedMinutes++;
      
      if (simulatedMinutes >= 60) {
        simulatedMinutes = 0;
        simulatedHours++;
        
        if (simulatedHours >= 24) {
          simulatedHours = 0;  // Reset após 24 horas
        }
      }
    }
  }
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  delay(2000);

  Serial.println("==========================================");
  Serial.println("TRANSMISSOR - Modo Teste com Padrões Rotativos");
  Serial.println("==========================================");
  Serial.println("✅ Tempo simulado: 00:00:00 (avança automaticamente)");
  Serial.println("✅ Sensores: Padrão rotativo [0, meio, max]");
  Serial.println("✅ FSR: Padrão rotativo [0, 1/3, 2/3, max]");
  Serial.println("✅ Frequência: 10Hz (10ms)");
  Serial.println();
  Serial.println("A enviar pacotes...");
  Serial.println();

  // Inicializar NRF24
  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETECTADO");
    Serial.println("Verifique as ligações:");
    Serial.println("  VCC → 3.3V");
    Serial.println("  GND → GND");
    Serial.println("  CE  → PIN 7");
    Serial.println("  CSN → PIN 10");
    while (1);
  }

  Serial.println("✅ NRF24L01 OK");

  // CONFIGURAÇÕES (iguais para transmissor e recetor)
  radio.setChannel(80);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(false);  // Desativado para maior velocidade

  radio.stopListening();
  radio.openWritingPipe(address);

  // Inicializar tempo simulado
  lastTimeUpdate = millis();
}

// ========================
// 🧱 BUILD PACKET
// ========================
void buildPacket() {
  packet.header = 0xAA;
  packet.glove_id = 2;

  // Valores para cada sensor [mínimo, meio, máximo]
  int val_acc[3] = {-2000, 0, 2000};
  int val_gyro[3] = {-500, 0, 500};
  int val_mag[3] = {-1000, 0, 1000};

  // Aplicar padrão rotativo baseado no pattern_index
  // Padrão 0: [mínimo, meio, máximo]
  // Padrão 1: [meio, máximo, mínimo]
  // Padrão 2: [máximo, mínimo, meio]
  for (int i = 0; i < 3; i++) {
    int posicao = (i + pattern_index) % 3;
    
    packet.accel[i] = val_acc[posicao];
    packet.gyro[i]  = val_gyro[posicao];
    packet.mag[i]   = val_mag[posicao];
  }

  // FSR com padrão rotativo (4 valores: 0, 1/3, 2/3, máximo)
  int fsr_vals[4] = {0, 341, 682, 1023};
  for (int i = 0; i < 4; i++) {
    int posicao_fsr = (i + pattern_index) % 4;
    packet.fsr[i] = fsr_vals[posicao_fsr];
  }

  packet.sequence = sequence++;
  
  // 🔥 USAR TEMPO SIMULADO (automático, sem necessidade de configuração)
  packet.timestamp = (simulatedHours << 8) | simulatedMinutes;

  packet.checksum = calculateChecksum(&packet);
  
  // Avançar para o próximo padrão na próxima mensagem
  pattern_index = (pattern_index + 1) % 3;
}

// ========================
// 📡 SEND PACKET
// ========================
void sendPacket() {
  bool ok = radio.write(&packet, sizeof(Packet));

  if (ok) {
    uint8_t hora = (packet.timestamp >> 8) & 0xFF;
    uint8_t minuto = packet.timestamp & 0xFF;
    
    // Mostrar informações formatadas
    Serial.print("📤 [");
    Serial.print(packet.sequence);
    Serial.print("] Padrão:");
    Serial.print(pattern_index == 0 ? 0 : (pattern_index == 1 ? 1 : 2));
    Serial.print(" | ⏰ ");
    Serial.print(hora);
    Serial.print(":");
    if (minuto < 10) Serial.print("0");
    Serial.print(minuto);
    Serial.print(":");
    if (simulatedSeconds < 10) Serial.print("0");
    Serial.print(simulatedSeconds);
    
    // Mostrar ACC
    Serial.print(" | ACC[");
    Serial.print(packet.accel[0]); 
    Serial.print(",");
    Serial.print(packet.accel[1]); 
    Serial.print(",");
    Serial.print(packet.accel[2]); 
    Serial.print("]");
    
    // Mostrar GYRO (opcional - pode comentar para mais velocidade)
    Serial.print(" | GYR[");
    Serial.print(packet.gyro[0]); 
    Serial.print(",");
    Serial.print(packet.gyro[1]); 
    Serial.print(",");
    Serial.print(packet.gyro[2]); 
    Serial.print("]");
    
    // Mostrar FSR
    Serial.print(" | FSR[");
    Serial.print(packet.fsr[0]); 
    Serial.print(",");
    Serial.print(packet.fsr[1]); 
    Serial.print(",");
    Serial.print(packet.fsr[2]); 
    Serial.print(",");
    Serial.print(packet.fsr[3]); 
    Serial.println("]");

    digitalWrite(LED_PIN, HIGH);
    delay(10);  // LED pisca rápido (10ms)
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.println("❌ Falha no envio");
  }
}

// ========================
// 🔁 LOOP PRINCIPAL
// ========================
void loop() {
  updateSimulatedTime();

  unsigned long now = millis();

  // Enviar pacote no intervalo definido
  if (now - lastSend >= interval) {
    lastSend = now;
    buildPacket();
    sendPacket();
  }
}
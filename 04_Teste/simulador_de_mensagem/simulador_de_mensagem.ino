#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

const byte address[6] = "00001";

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
const int interval = 10; // 1 segundo

// Variáveis para a hora atual
uint8_t currentHour = 0;
uint8_t currentMinute = 0;
uint8_t currentSecond = 0;
unsigned long lastTimeUpdate = 0;
bool timeReceived = false;

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
// 🕐 ATUALIZAR HORA (baseada em millis)
// ========================
void updateCurrentTime() {
  if (!timeReceived) return;
  
  unsigned long now = millis();
  unsigned long elapsedSeconds = (now - lastTimeUpdate) / 1000;
  
  if (elapsedSeconds > 0) {
    // Adicionar segundos decorridos
    int totalSeconds = currentHour * 3600 + currentMinute * 60 + currentSecond + elapsedSeconds;
    
    currentHour = (totalSeconds / 3600) % 24;
    currentMinute = (totalSeconds % 3600) / 60;
    currentSecond = totalSeconds % 60;
    
    // Atualizar o tempo base
    lastTimeUpdate = now;
  }
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  delay(2000);

  Serial.println("Transmissor - Hora atual automática");
  Serial.println("Envie a hora atual no formato: HH:MM");
  Serial.println("Exemplo: 15:30");
  Serial.println();

  // Inicializar NRF24
  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETECTADO");
    while (1);
  }

  Serial.println("✅ NRF OK");

  // CONFIG
  radio.setChannel(76);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(true);
  radio.setRetries(15, 15);

  radio.stopListening();
  radio.openWritingPipe(address);

  randomSeed(analogRead(0));
}

// ========================
// 🔁 LOOP (1 Hz)
// ========================
void loop() {
  // Verificar se recebeu hora do computador
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    // Verificar formato HH:MM
    if (input.length() == 5 && input[2] == ':') {
      currentHour = input.substring(0, 2).toInt();
      currentMinute = input.substring(3, 5).toInt();
      currentSecond = 0;
      
      if (currentHour >= 0 && currentHour <= 23 && currentMinute >= 0 && currentMinute <= 59) {
        timeReceived = true;
        lastTimeUpdate = millis();  // Marca o momento da definição da hora
        
        Serial.print("✅ Hora definida: ");
        Serial.print(currentHour);
        Serial.print(":");
        if (currentMinute < 10) Serial.print("0");
        Serial.print(currentMinute);
        Serial.print(":00");
        Serial.println(" (vai avançar automaticamente)");
      } else {
        Serial.println("❌ Hora inválida! Use HH:MM (00:00 a 23:59)");
      }
    } else {
      Serial.println("❌ Formato inválido! Use HH:MM (exemplo: 15:30)");
    }
  }

  // Atualizar a hora atual
  updateCurrentTime();

  unsigned long now = millis();

  if (now - lastSend >= interval) {
    lastSend = now;

    buildPacket();
    sendPacket();
  }
}

// ========================
// 🧱 BUILD PACKET
// ========================
void buildPacket() {
  packet.header = 0xAA;
  packet.glove_id = 1;

  // Simulação sensores
  for (int i = 0; i < 3; i++) {
    packet.accel[i] = random(-2000, 2000);
    packet.gyro[i]  = random(-500, 500);
    packet.mag[i]   = random(-1000, 1000);
  }

  for (int i = 0; i < 4; i++) {
    packet.fsr[i] = random(0, 1023);
  }

  packet.sequence = sequence++;
  
  // 🔥 USAR HORA ATUAL ATUALIZADA
  if (timeReceived) {
    packet.timestamp = (currentHour << 8) | currentMinute;
  } else {
    packet.timestamp = (0 << 8) | 0;  // 00:00 enquanto não define hora
  }

  packet.checksum = calculateChecksum(&packet);
}

// ========================
// 📡 SEND PACKET
// ========================
void sendPacket() {
  bool ok = radio.write(&packet, sizeof(Packet));

  if (ok) {
    uint8_t hora = (packet.timestamp >> 8) & 0xFF;
    uint8_t minuto = packet.timestamp & 0xFF;
    
    Serial.print("📤 Seq: ");
    Serial.print(packet.sequence);
    Serial.print(" | Hora: ");
    Serial.print(hora);
    Serial.print(":");
    if (minuto < 10) Serial.print("0");
    Serial.print(minuto);
    Serial.print(":");
    if (currentSecond < 10) Serial.print("0");
    Serial.print(currentSecond);
    Serial.println();

    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
  } else {
    Serial.println("❌ Falha envio");
  }
}
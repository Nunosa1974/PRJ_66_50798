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

// 10ms = 100Hz
const int interval = 10;

// ========================
// ⏰ TEMPO SIMULADO
// ========================
unsigned long lastTimeUpdate = 0;
int simulatedSeconds = 0;
int simulatedMinutes = 0;
int simulatedHours = 0;

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
// ⏰ ATUALIZAR TEMPO
// ========================
void updateSimulatedTime() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastTimeUpdate >= 1000) {
    lastTimeUpdate = currentMillis;

    simulatedSeconds++;

    if (simulatedSeconds >= 60) {
      simulatedSeconds = 0;
      simulatedMinutes++;

      if (simulatedMinutes >= 60) {
        simulatedMinutes = 0;
        simulatedHours++;

        if (simulatedHours >= 24) {
          simulatedHours = 0;
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
  Serial.println("TRANSMISSOR - Valores Aleatórios");
  Serial.println("==========================================");
  Serial.println("✅ Frequência: 100Hz");
  Serial.println("✅ Sensores: valores aleatórios");
  Serial.println("✅ Timestamp automático");
  Serial.println();

  // Inicializar NRF24
  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETECTADO");

    while (1);
  }

  Serial.println("✅ NRF24L01 OK");

  // ========================
  // 📡 CONFIGURAÇÃO NRF
  // ========================
  radio.setChannel(80);

  // 2 Mbps
  radio.setDataRate(RF24_2MBPS);

  // Potência baixa
  radio.setPALevel(RF24_PA_LOW);

  // ACK desligado para máxima velocidade
  radio.setAutoAck(false);

  radio.stopListening();
  radio.openWritingPipe(address);

  // Inicializar relógio
  lastTimeUpdate = millis();

  // Seed aleatória
  randomSeed(analogRead(0));

  Serial.println("🚀 A enviar pacotes...");
  Serial.println();
}

// ========================
// 🧱 BUILD PACKET
// ========================
void buildPacket() {

  packet.header = 0xAA;
  packet.glove_id = 1;

  // ========================
  // 🎲 ACCEL
  // ========================
  for (int i = 0; i < 3; i++) {
    packet.accel[i] = random(-2000, 2001);
  }

  // ========================
  // 🎲 GYRO
  // ========================
  for (int i = 0; i < 3; i++) {
    packet.gyro[i] = random(-500, 501);
  }

  // ========================
  // 🎲 MAG
  // ========================
  for (int i = 0; i < 3; i++) {
    packet.mag[i] = random(-1000, 1001);
  }

  // ========================
  // 🎲 FSR
  // ========================
  for (int i = 0; i < 4; i++) {
    packet.fsr[i] = random(0, 1024);
  }

  // Sequence number
  packet.sequence = sequence++;

  // Timestamp
  packet.timestamp = (simulatedHours << 8) | simulatedMinutes;

  // Checksum
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

    Serial.print(" | ⏰ ");
    Serial.print(hora);
    Serial.print(":");

    if (minuto < 10)
      Serial.print("0");

    Serial.print(minuto);

    Serial.print(":");

    if (simulatedSeconds < 10)
      Serial.print("0");

    Serial.print(simulatedSeconds);

    // Mostrar ACC
    Serial.print(" | ACC[");
    Serial.print(packet.accel[0]);
    Serial.print(",");
    Serial.print(packet.accel[1]);
    Serial.print(",");
    Serial.print(packet.accel[2]);
    Serial.print("]");

    // Mostrar GYRO
    Serial.print(" | GYR[");
    Serial.print(packet.gyro[0]);
    Serial.print(",");
    Serial.print(packet.gyro[1]);
    Serial.print(",");
    Serial.print(packet.gyro[2]);
    Serial.print("]");

    // Mostrar MAG
    Serial.print(" | MAG[");
    Serial.print(packet.mag[0]);
    Serial.print(",");
    Serial.print(packet.mag[1]);
    Serial.print(",");
    Serial.print(packet.mag[2]);
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

    // Piscar LED rapidamente
    digitalWrite(LED_PIN, HIGH);
    delay(1);
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

  // 100Hz
  if (now - lastSend >= interval) {

    lastSend = now;

    buildPacket();

    sendPacket();
  }
}
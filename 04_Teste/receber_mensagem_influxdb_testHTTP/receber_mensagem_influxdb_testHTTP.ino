#include <SPI.h>
#include <RF24.h>

#include <WiFi.h>
#include <HTTPClient.h>

// ========================
// 📡 NRF24L01 PINS
// ========================
#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// ========================
// 📶 WIFI
// ========================
const char* ssid = "NOS-454D";
const char* password = "SU7T2CL7";

// ========================
// 🗄️ INFLUXDB
// ========================
// TROCAR PELO IP DO TEU PC
const char* influxdbUrl =
"http://192.168.1.14:8086/api/v2/write?org=ISEL&bucket=Projeto_Boxe&precision=ms";

// TROCAR PELO TEU TOKEN
const char* influxToken = "b-OvLwEmVfrfLe_BlChdvR4CUhOhs4kHQnxAMfc83npZUdFsxkephYjVXUF1FCk2Ub4QYdjr3b0jN4sh0B0DFw==";

// ========================
// 📬 ENDEREÇO NRF24
// ========================
const byte address[6] = "00002";

// ========================
// 📦 PACKET
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

// ========================
// 📊 ESTATÍSTICAS
// ========================
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
// 📶 WIFI
// ========================
void connectWiFi() {

  WiFi.begin(ssid, password);

  Serial.print("📶 Ligando ao WiFi");

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("✅ WiFi conectado!");

  Serial.print("🌐 IP ESP32: ");
  Serial.println(WiFi.localIP());

  Serial.println();
}

// ========================
// 🗄️ ENVIAR PARA INFLUXDB
// ========================
void sendToInfluxDB() {

  // ========================
  // 📶 VERIFICAR WIFI
  // ========================
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("❌ WiFi desligado!");

    return;
  }

  HTTPClient http;

  // ========================
  // ⏱️ TIMEOUT MENOR
  // ========================
  http.setTimeout(1000);

  // ========================
  // 🌐 URL
  // ========================
  http.begin(influxdbUrl);

  // ========================
  // 📄 HEADERS HTTP
  // ========================
  http.addHeader("Authorization", String("Token ") + influxToken);

  http.addHeader("Content-Type", "text/plain");

  // ========================
  // 📝 INFLUX LINE PROTOCOL
  // ========================
  String data = "";

  // Measurement + tag
  data += "boxing";
  data += ",glove=" + String(packet.glove_id);

  // Fields
  data += " accelX=" + String(packet.accel[0]);
  data += ",accelY=" + String(packet.accel[1]);
  data += ",accelZ=" + String(packet.accel[2]);

  data += ",gyroX=" + String(packet.gyro[0]);
  data += ",gyroY=" + String(packet.gyro[1]);
  data += ",gyroZ=" + String(packet.gyro[2]);

  data += ",magX=" + String(packet.mag[0]);
  data += ",magY=" + String(packet.mag[1]);
  data += ",magZ=" + String(packet.mag[2]);

  data += ",fsr1=" + String(packet.fsr[0]);
  data += ",fsr2=" + String(packet.fsr[1]);
  data += ",fsr3=" + String(packet.fsr[2]);
  data += ",fsr4=" + String(packet.fsr[3]);

  data += ",sequence=" + String(packet.sequence);

  // ========================
  // 🚀 ENVIAR POST
  // ========================
  int httpResponseCode = http.POST(data);

  // ========================
  // 📊 DEBUG HTTP
  // ========================
  if (httpResponseCode > 0) {

    Serial.print("✅ HTTP OK: ");
    Serial.println(httpResponseCode);

  } else {

    Serial.print("❌ Erro HTTP: ");
    Serial.println(httpResponseCode);

    Serial.print("📄 Descrição: ");
    Serial.println(http.errorToString(httpResponseCode));
  }

  // ========================
  // 🔚 FECHAR HTTP
  // ========================
  http.end();
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {

  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);

  delay(2000);

  Serial.println("==========================================");
  Serial.println("RECETOR NRF24 -> INFLUXDB");
  Serial.println("==========================================");
  Serial.println();

  // ========================
  // 📶 WIFI
  // ========================
  connectWiFi();

  // ========================
  // 📡 NRF24
  // ========================
  if (!radio.begin()) {

    Serial.println("❌ NRF24L01 NÃO DETECTADO!");

    while (1);
  }

  Serial.println("✅ NRF24L01 OK");

  // Configurações rádio
  radio.setChannel(80);

  radio.setDataRate(RF24_2MBPS);

  radio.setPALevel(RF24_PA_LOW);

  radio.setAutoAck(false);

  radio.openReadingPipe(0, address);

  radio.startListening();

  Serial.println();
  Serial.println("📡 Configurações:");
  Serial.println("  - Canal: 80");
  Serial.println("  - Taxa: 2MBPS");
  Serial.println("  - AutoAck: false");
  Serial.println();

  Serial.println("✅ Aguardando pacotes...");
  Serial.println();
}

// ========================
// 🔁 LOOP
// ========================
void loop() {

  // ========================
  // 📡 RECEBER TODOS OS PACOTES DISPONÍVEIS
  // ========================
  while (radio.available()) {

    // ========================
    // 📥 LER PACOTE
    // ========================
    radio.read(&packet, sizeof(Packet));

    lastReceiveTime = millis();

    packetsReceived++;

    // ========================
    // 🔍 VALIDAR HEADER
    // ========================
    if (packet.header != 0xAA) {
      continue;
    }

    // ========================
    // 🔐 VALIDAR CHECKSUM
    // ========================
    uint8_t calc = calculateChecksum(&packet);

    if (calc != packet.checksum) {
      continue;
    }

    // ========================
    // 📉 DETETAR PERDAS
    // ========================
    if (!firstPacket) {

      uint8_t expected = lastSequence + 1;

      if (packet.sequence != expected) {

        int lost = packet.sequence - expected;

        if (lost < 0) {
          lost += 256;
        }

        packetsLost += lost;
      }
    }

    firstPacket = false;

    lastSequence = packet.sequence;

    // ========================
    // ⚠️ NÃO ENVIAR HTTP AQUI
    // ========================
    //sendToInfluxDB();

    // ========================
    // 📊 DEBUG REDUZIDO
    // ========================
    if (packet.sequence % 50 == 0) {

      Serial.print("📦 Seq: ");
      Serial.print(packet.sequence);

      Serial.print(" | FSR1: ");
      Serial.print(packet.fsr[0]);

      Serial.print(" | Perdidos: ");
      Serial.println(packetsLost);
    }

    // ========================
    // 💡 LED FEEDBACK
    // ========================
    digitalWrite(LED_PIN, HIGH);

    delayMicroseconds(100);

    digitalWrite(LED_PIN, LOW);
  }

  // ========================
  // 🗄️ ENVIAR PARA INFLUXDB
  // ========================
  // Envia apenas a cada 100ms
  static unsigned long lastInfluxSend = 0;

  if (millis() - lastInfluxSend >= 100) {

    sendToInfluxDB();

    lastInfluxSend = millis();
  }

  // ========================
  // ⚠️ TIMEOUT
  // ========================
  static unsigned long lastWarning = 0;

  if (millis() - lastReceiveTime > 3000 && lastReceiveTime != 0) {

    if (millis() - lastWarning > 5000) {

      Serial.println("⚠️ Sem dados há 3 segundos...");

      lastWarning = millis();
    }
  }
}
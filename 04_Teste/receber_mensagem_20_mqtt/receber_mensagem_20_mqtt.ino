#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// 🔹 WIFI
const char* ssid = "Nunosa";
const char* password = "Panda@135790";

// 🔹 MQTT
const char* mqtt_server = "172.20.10.11";

WiFiClient espClient;
PubSubClient client(espClient);

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
void setup_wifi() {
  Serial.println("📶 Ligando WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n✅ WiFi ligado!");
  Serial.print("IP ESP32: ");
  Serial.println(WiFi.localIP());
}

// ========================
// 🔌 MQTT
// ========================
void reconnect() {
  while (!client.connected()) {
    Serial.print("MQTT...");

    if (client.connect("ESP32_Receiver")) {
      Serial.println("ligado!");
    } else {
      Serial.print("erro=");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

// ========================
// 📡 MQTT SEND
// ========================
void publishPacket() {
  char msg[256];

  uint8_t hora = (packet.timestamp >> 8) & 0xFF;
  uint8_t minuto = packet.timestamp & 0xFF;

  sprintf(msg,
    "{\"seq\":%d,\"hora\":%d,\"min\":%d,\"ax\":%d,\"ay\":%d,\"az\":%d,\"fsr1\":%d,\"fsr2\":%d,\"fsr3\":%d,\"fsr4\":%d}",
    packet.sequence,
    hora,
    minuto,
    packet.accel[0], packet.accel[1], packet.accel[2],
    packet.fsr[0], packet.fsr[1], packet.fsr[2], packet.fsr[3]
  );

  client.publish("boxing/glove", msg);
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {
  pinMode(LED_PIN, OUTPUT);

  Serial.begin(115200);
  delay(2000);

  Serial.println("RECETOR + MQTT");

  if (!radio.begin()) {
    Serial.println("❌ NRF NÃO DETETADO");
    while (1);
  }

  Serial.println("✅ NRF OK");

  // 🔥 IGUAL AO TRANSMISSOR
  radio.setChannel(76);
  radio.setDataRate(RF24_2MBPS);
  radio.setPALevel(RF24_PA_LOW);
  radio.setAutoAck(false);

  radio.openReadingPipe(0, address);
  radio.startListening();

  // WIFI + MQTT
  setup_wifi();
  client.setServer(mqtt_server, 1883);
}

// ========================
// 🔁 LOOP
// ========================
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (radio.available()) {

    radio.read(&packet, sizeof(Packet));

    // validação
    if (packet.header != 0xAA) return;
    if (calculateChecksum(&packet) != packet.checksum) return;

    // detectar perdas
    if (!firstPacket) {
      uint8_t expected = lastSequence + 1;
      if (packet.sequence != expected) {
        Serial.println("⚠️ perda pacote");
      }
    }

    firstPacket = false;
    lastSequence = packet.sequence;

    // debug
    Serial.print("SEQ: ");
    Serial.println(packet.sequence);

    // 🔥 enviar MQTT
    publishPacket();

    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(200);
    digitalWrite(LED_PIN, LOW);
  }
}
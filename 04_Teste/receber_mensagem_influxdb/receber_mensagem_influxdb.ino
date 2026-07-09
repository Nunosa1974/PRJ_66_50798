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
const char* influxdbUrl = "http://192.168.1.13:8086/api/v2/write?org=ISEL&bucket=Projeto_Boxe_V2&precision=ms";
const char* influxToken = "Ct3jZHhPBAs_KSX4qxwFjgvgGk1pzgilYylPLUhJzLNUMZTO_8utLxehs737re4QMpe7DENPz3oodhtafxlRVg==";

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

// ========================
// ⚙️ CONFIGURAÇÕES OTIMIZADAS
// ========================
int BATCH_SIZE = 50;                    // 50 pacotes por batch
int BUFFER_SIZE = 2000;                 // 2000 pacotes de buffer
int INFLUX_SEND_INTERVAL_MS = 100;      // 🔥 Reduzido para 100ms
int MAX_LATENCY_MS = 500;               // Latência máxima tolerada
int WIFI_TIMEOUT_MS = 2000;             // Timeout 2 segundos

// ========================
// 🗃️ BUFFER CIRCULAR
// ========================
Packet* packetBuffer = NULL;
int bufferCount = 0;
int bufferReadIndex = 0;
int bufferWriteIndex = 0;

// ========================
// 📊 ESTATÍSTICAS
// ========================
uint8_t lastSequence = 0;
bool firstPacket = true;
unsigned long lastReceiveTime = 0;
int packetsReceived = 0;
int packetsLost = 0;
int httpErrors = 0;
int batchesSent = 0;
unsigned long lastPpsTime = 0;
int lastPpsCount = 0;

// ========================
// ⏱️ CONTROLO DE TEMPO
// ========================
unsigned long lastBatchSend = 0;
unsigned long lastStatsTime = 0;
unsigned long lastWarning = 0;
unsigned long lastOverflowWarning = 0;
bool isSending = false;

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
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  
  Serial.print("📶 WiFi");
  
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 25) {
    delay(200);
    if (attempts % 3 == 0) Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" ✅");
    Serial.print("   IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" ❌");
  }
}

// ========================
// 🗄️ ENVIAR BATCH
// ========================
void sendBatchToInfluxDB() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  
  if (bufferCount == 0) {
    return;
  }
  
  if (isSending) {
    return;
  }
  isSending = true;
  
  HTTPClient http;
  http.setTimeout(WIFI_TIMEOUT_MS);
  
  http.begin(influxdbUrl);
  http.addHeader("Authorization", String("Token ") + influxToken);
  http.addHeader("Content-Type", "text/plain");
  
  String data = "";
  int packetsToSend = min(BATCH_SIZE, bufferCount);
  data.reserve(10000);
  
  unsigned long batchStartTime = millis();
  
  for (int i = 0; i < packetsToSend; i++) {
    int idx = (bufferReadIndex + i) % BUFFER_SIZE;
    Packet *p = &packetBuffer[idx];
    
    if (i > 0) data += "\n";
    
    data += "boxing,glove=";
    data += String(p->glove_id);
    data += " accelX=";
    data += String(p->accel[0]);
    data += ",accelY=";
    data += String(p->accel[1]);
    data += ",accelZ=";
    data += String(p->accel[2]);
    data += ",gyroX=";
    data += String(p->gyro[0]);
    data += ",gyroY=";
    data += String(p->gyro[1]);
    data += ",gyroZ=";
    data += String(p->gyro[2]);
    data += ",magX=";
    data += String(p->mag[0]);
    data += ",magY=";
    data += String(p->mag[1]);
    data += ",magZ=";
    data += String(p->mag[2]);
    data += ",fsr1=";
    data += String(p->fsr[0]);
    data += ",fsr2=";
    data += String(p->fsr[1]);
    data += ",fsr3=";
    data += String(p->fsr[2]);
    data += ",fsr4=";
    data += String(p->fsr[3]);
    data += ",sequence=";
    data += String(p->sequence);
  }
  
  int httpCode = http.POST(data);
  
  if (httpCode == 204) {
    bufferReadIndex = (bufferReadIndex + packetsToSend) % BUFFER_SIZE;
    bufferCount -= packetsToSend;
    batchesSent++;
    
    if (batchesSent % 10 == 0) {
      Serial.print("⚡ Batch: ");
      Serial.print(packetsToSend);
      Serial.print(" pacotes | Buffer: ");
      Serial.print(bufferCount);
      Serial.print("/");
      Serial.println(BUFFER_SIZE);
    }
  } else {
    httpErrors++;
    
    static unsigned long lastErrorLog = 0;
    if (millis() - lastErrorLog > 10000) {
      Serial.print("❌ HTTP Erro: ");
      Serial.print(httpCode);
      Serial.println();
      lastErrorLog = millis();
    }
  }
  
  http.end();
  isSending = false;
}

// ========================
// 📥 PROCESSAR PACOTE
// ========================
bool processPacket(Packet *p) {
  if (p->header != 0xAA) {
    return false;
  }
  
  if (calculateChecksum(p) != p->checksum) {
    return false;
  }
  
  if (!firstPacket) {
    uint8_t expected = lastSequence + 1;
    if (p->sequence != expected) {
      int lost = p->sequence - expected;
      if (lost < 0) {
        lost += 256;
      }
      packetsLost += lost;
      
      // Mostra apenas perdas grandes
      if (lost > 10) {
        Serial.print("⚠️ Perda: ");
        Serial.print(lost);
        Serial.print(" pacotes (");
        Serial.print(expected);
        Serial.print("→");
        Serial.print(p->sequence);
        Serial.println(")");
      }
    }
  }
  
  lastSequence = p->sequence;
  firstPacket = false;
  packetsReceived++;
  
  return true;
}

// ========================
// 📊 ESTATÍSTICAS
// ========================
void showStatistics() {
  float lossRate = 0;
  int total = packetsReceived + packetsLost;
  if (total > 0) {
    lossRate = (float)packetsLost / total * 100;
  }
  
  int estimatedLatency = bufferCount * 10;
  
  Serial.print("📊 ");
  
  if (lossRate < 5) Serial.print("✅");
  else if (lossRate < 20) Serial.print("⚠️");
  else Serial.print("❌");
  
  Serial.print(" Rx:");
  Serial.print(packetsReceived);
  
  Serial.print(" | Perda:");
  Serial.print(packetsLost);
  Serial.print(" (");
  Serial.print(lossRate, 1);
  Serial.print("%)");
  
  Serial.print(" | Buf:");
  Serial.print(bufferCount);
  Serial.print("/");
  Serial.print(BUFFER_SIZE);
  
  // Pacotes por segundo (aproximado)
  static unsigned long lastPpsTime = 0;
  static int lastPpsCount = 0;
  if (millis() - lastPpsTime >= 1000) {
    int pps = (packetsReceived - lastPpsCount);
    Serial.print(" | ");
    Serial.print(pps);
    Serial.print(" pkt/s");
    lastPpsCount = packetsReceived;
    lastPpsTime = millis();
  }
  
  if (httpErrors > 0) {
    Serial.print(" | HTTP err:");
    Serial.print(httpErrors);
  }
  
  Serial.println();
}

// ========================
// ⚙️ SETUP
// ========================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("==========================================");
  Serial.println("🥊 RECETOR NRF24 -> INFLUXDB (OTIMIZADO)");
  Serial.println("==========================================");
  Serial.println();
  
  packetBuffer = (Packet*)malloc(BUFFER_SIZE * sizeof(Packet));
  if (packetBuffer == NULL) {
    Serial.println("❌ ERRO: Falha ao alocar buffer!");
    while(1);
  }
  
  Serial.print("Buffer: ");
  Serial.print(BUFFER_SIZE);
  Serial.println(" pacotes");
  Serial.print("Batch: ");
  Serial.print(BATCH_SIZE);
  Serial.print(" pacotes | Envio a cada ");
  Serial.print(INFLUX_SEND_INTERVAL_MS);
  Serial.println("ms");
  Serial.println();
  
  connectWiFi();
  
  // Configuração RF24
  if (!radio.begin()) {
    Serial.println("❌ NRF24L01 NÃO DETECTADO!");
    while (1) {
      delay(1000);
      Serial.println("🔁 Tentando...");
      if (radio.begin()) break;
    }
  }
  
  Serial.println("✅ NRF24L01 OK");
  
  // 🔥 CONFIGURAÇÕES RF24 OTIMIZADAS PARA RECEÇÃO RÁPIDA
  radio.setChannel(80);
  radio.setDataRate(RF24_2MBPS);      // 250KBPS mais estável que 2MBPS
  radio.setPALevel(RF24_PA_MAX);        // Potência máxima para melhor sinal
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(sizeof(Packet));
  radio.openReadingPipe(0, address);
  radio.startListening();
  
  Serial.println();
  Serial.println("📡 RF24 Config:");
  Serial.println("  - Canal: 80");
  Serial.println("  - Taxa: 250KBPS");
  Serial.println("  - Potência: MAX");
  Serial.println("  - AutoAck: false");
  Serial.println();
  
  Serial.println("✅ Aguardando pacotes...");
  Serial.println("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
  Serial.println();
}

// ========================
// 🔁 LOOP PRINCIPAL
// ========================
void loop() {
  // ========================
  // PRIORIDADE MÁXIMA: LER TODOS OS PACOTES DO NRF24
  // ========================
  while (radio.available()) {
    // Verifica espaço no buffer
    if (bufferCount >= BUFFER_SIZE) {
      if (millis() - lastOverflowWarning > 5000) {
        Serial.println("⚠️ BUFFER OVERFLOW!");
        lastOverflowWarning = millis();
      }
      bufferReadIndex = (bufferReadIndex + 1) % BUFFER_SIZE;
      bufferCount--;
      packetsLost++;
    }
    
    // Leitura rápida
    radio.read(&packetBuffer[bufferWriteIndex], sizeof(Packet));
    
    // Processamento
    if (processPacket(&packetBuffer[bufferWriteIndex])) {
      bufferWriteIndex = (bufferWriteIndex + 1) % BUFFER_SIZE;
      bufferCount++;
      lastReceiveTime = millis();
      
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(20);
      digitalWrite(LED_PIN, LOW);
    }
  }
  
  // ========================
  // ENVIAR BATCH
  // ========================
  if (bufferCount >= BATCH_SIZE && (millis() - lastBatchSend) >= INFLUX_SEND_INTERVAL_MS) {
    // Só envia se não estiver a receber pacotes no momento
    if (!radio.available()) {
      sendBatchToInfluxDB();
      lastBatchSend = millis();
    }
  }
  
  // ========================
  // EMERGÊNCIA: Buffer a encher
  // ========================
  if (bufferCount > BUFFER_SIZE * 0.7 && (millis() - lastBatchSend) > 50) {
    static unsigned long lastEmergencyLog = 0;
    if (millis() - lastEmergencyLog > 2000) {
      Serial.print("🚨 Buffer: ");
      Serial.print(bufferCount);
      Serial.print("/");
      Serial.println(BUFFER_SIZE);
      lastEmergencyLog = millis();
    }
    sendBatchToInfluxDB();
    lastBatchSend = millis();
  }
  
  // ========================
  // ESTATÍSTICAS
  // ========================
  if (millis() - lastStatsTime >= 3000) {
    showStatistics();
    lastStatsTime = millis();
  }
  
  // ========================
  // TIMEOUT EMISSOR
  // ========================
  if (lastReceiveTime != 0 && (millis() - lastReceiveTime) > 3000) {
    if (millis() - lastWarning > 5000) {
      Serial.println("⚠️ SEM DADOS DO EMISSOR!");
      lastWarning = millis();
    }
  }
  
  // Delay mínimo para dar tempo ao WiFi
  delayMicroseconds(10);
}
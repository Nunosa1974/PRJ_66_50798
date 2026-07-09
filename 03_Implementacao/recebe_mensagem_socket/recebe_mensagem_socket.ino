#include <SPI.h>
#include <RF24.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

// pins do nrf24l01
#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

// set up para o wifi
const char* ssid = "NOS-454D";
const char* password = "SU7T2CL7";

// canal para o websocket
WebSocketsClient webSocket;

const char* websocket_host = "192.168.1.100";
const uint16_t websocket_port = 1880;
const char* websocket_path = "/ws/in";

// endereço onde as mensagens vão ser recebidas
const byte address[6] = "00002";

// estrutura do pacote
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

// configurações paar enviar as mensagens para o node-red paar posteriormente poderem ser guardados
const int BATCH_SIZE = 20;
const int BUFFER_SIZE = 500;
const int SEND_INTERVAL_MS = 100;
const int MAX_LATENCY_MS = 200;

// buffer circular para guardar as mensagens
Packet* packetBuffer = NULL;
int bufferCount = 0;
int bufferReadIndex = 0;
int bufferWriteIndex = 0;

//para a parte de debug
uint8_t lastSequence = 0;
bool firstPacket = true;
unsigned long lastReceiveTime = 0;
int packetsReceived = 0;
int packetsLost = 0;
int batchesSent = 0;
int wsErrors = 0;
bool wsConnected = false;

unsigned long lastBatchSend = 0;
unsigned long lastStatsTime = 0;
unsigned long lastWarning = 0;
unsigned long lastOverflowWarning = 0;

// checksum
uint8_t calculateChecksum(Packet *p) {
  uint8_t *data = (uint8_t*)p;
  uint8_t sum = 0;
  for (int i = 0; i < sizeof(Packet) - 1; i++) {
    sum ^= data[i];
  }
  return sum;
}

// função para conectar-se ao wifi
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("📶 Conectando WiFi");
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(200);
    if (attempts % 5 == 0) Serial.print(".");
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

// eventos do websocket
void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("❌ WebSocket: Desconectado!");
      break;

    case WStype_CONNECTED:
      wsConnected = true;
      wsErrors = 0;
      Serial.println("✅ WebSocket: Conectado ao Node-RED!");
      // Envia mensagem de boas-vindas
      webSocket.sendTXT("{\"type\":\"esp32\",\"status\":\"connected\"}");
      break;

    case WStype_ERROR:
      wsErrors++;
      wsConnected = false;
      Serial.printf("❌ WebSocket: Erro (%d)\n", wsErrors);
      break;

    case WStype_TEXT:
      if (length < 100) {
        Serial.printf("📩 Resposta Node-RED: %s\n", payload);
      }
      break;
      
    case WStype_PING:
      Serial.println("📡 PING recebido");
      break;
      
    case WStype_PONG:
      Serial.println("📡 PONG recebido");
      break;
      
    default:
      break;
  }
}

// para ligar ao websocket
void connectWebSocket() {
  if (wsConnected) return;
  
  Serial.print("🌐 Conectando WebSocket...");
  webSocket.begin(websocket_host, websocket_port, websocket_path);
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(3000);
  Serial.println(" OK");
  Serial.printf("   Servidor: %s:%d%s\n", websocket_host, websocket_port, websocket_path);
}

// função que vai enviar o batch das mensagens
void sendBatchViaWebSocket() {
  if (bufferCount == 0) return;
  if (!wsConnected) return;

  int packetsToSend = (BATCH_SIZE < bufferCount) ? BATCH_SIZE : bufferCount;
  
  for (int i = 0; i < packetsToSend; i++) {
    int idx = (bufferReadIndex + i) % BUFFER_SIZE;
    Packet *p = &packetBuffer[idx];
    
    StaticJsonDocument<200> doc;
    
    doc["gid"] = p->glove_id;
    doc["seq"] = p->sequence;
    doc["ax"] = p->accel[0];
    doc["ay"] = p->accel[1];
    doc["az"] = p->accel[2];
    doc["gx"] = p->gyro[0];
    doc["gy"] = p->gyro[1];
    doc["gz"] = p->gyro[2];
    doc["mx"] = p->mag[0];
    doc["my"] = p->mag[1];
    doc["mz"] = p->mag[2];
    doc["fs0"] = p->fsr[0];
    doc["fs1"] = p->fsr[1];
    doc["fs2"] = p->fsr[2];
    doc["fs3"] = p->fsr[3];
    doc["ts"] = millis() & 0xFFFF;
    
    String json;
    serializeJson(doc, json);
    webSocket.sendTXT(json);
  }

  bufferReadIndex = (bufferReadIndex + packetsToSend) % BUFFER_SIZE;
  bufferCount -= packetsToSend;
  batchesSent++;

  if (batchesSent % 30 == 0) {
    Serial.printf("📤 Batch: %d pacotes | Buffer: %d/%d\n", 
                  packetsToSend, bufferCount, BUFFER_SIZE);
  }
}

// esta função vai processar os pacotes recebidos
bool processPacket(Packet *p) {
  if (p->header != 0xAA) return false;
  if (calculateChecksum(p) != p->checksum) return false;

  if (!firstPacket) {
    uint8_t expected = lastSequence + 1;
    if (p->sequence != expected) {
      int lost = (p->sequence - expected) & 0xFF;
      packetsLost += lost;
      
      if (lost > 10) {
        Serial.printf("⚠️ Perda: %d pacotes (seq %d→%d)\n", 
                      lost, expected, p->sequence);
      }
    }
  }

  lastSequence = p->sequence;
  firstPacket = false;
  packetsReceived++;

  return true;
}

// estatistica do programa
void showStatistics() {
  float lossRate = 0;
  int total = packetsReceived + packetsLost;
  if (total > 0) {
    lossRate = (float)packetsLost / total * 100.0;
  }

  int latency = bufferCount * 10;

  Serial.print(wsConnected ? "🟢" : "🔴");
  Serial.print(latency < 150 ? "🟢" : (latency < 300 ? "🟡" : "🔴"));
  
  Serial.print(" Buf:");
  Serial.print(bufferCount);
  Serial.print("/");
  Serial.print(BUFFER_SIZE);
  
  Serial.print(" | Lat:");
  Serial.print(latency);
  Serial.print("ms");
  
  Serial.print(" | Perda:");
  Serial.print(lossRate, 1);
  Serial.print("%");
  
  Serial.print(" | Rx:");
  Serial.print(packetsReceived);
  
  Serial.print(" | Tx:");
  Serial.print(batchesSent * BATCH_SIZE);
  
  if (wsErrors > 0) {
    Serial.print(" | Err:");
    Serial.print(wsErrors);
  }
  
  if (!wsConnected) {
    Serial.print(" | 🔌WS down");
  }
  
  if (lastReceiveTime > 0) {
    unsigned long idle = (millis() - lastReceiveTime) / 1000;
    if (idle > 3) {
      Serial.print(" | ⚠️No data ");
      Serial.print(idle);
      Serial.print("s");
    }
  } else {
    Serial.print(" | ⏳Waiting...");
  }
  
  Serial.println();
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  delay(2000);

  Serial.println("====================================");
  Serial.println("🥊 RECETOR NRF24 -> WEBSOCKET");
  Serial.println("====================================\n");

  // Alocar buffer
  packetBuffer = (Packet*)malloc(BUFFER_SIZE * sizeof(Packet));
  if (packetBuffer == NULL) {
    Serial.println("❌ ERRO: Falha ao alocar buffer!");
    while (1) delay(1000);
  }

  Serial.printf("Configurações:\n");
  Serial.printf("  Buffer: %d pacotes\n", BUFFER_SIZE);
  Serial.printf("  Batch: %d pacotes\n", BATCH_SIZE);
  Serial.printf("  Intervalo: %dms\n\n", SEND_INTERVAL_MS);

  // Conectar WiFi
  connectWiFi();

  // Conectar WebSocket
  connectWebSocket();

  // Inicializar NRF24
  if (!radio.begin()) {
    Serial.println("❌ NRF24L01 NÃO DETECTADO!");
    while (1) {
      delay(1000);
      Serial.println("🔁 Verificando novamente...");
      if (radio.begin()) break;
    }
  }

  Serial.println("✅ NRF24L01 detectado");

  // Configurar RF24 (MANTIDO COMO TENS NO TRANSMISSOR)
  radio.setChannel(80);
  radio.setDataRate(RF24_2MBPS);      // MANTIDO: 2MBPS
  radio.setPALevel(RF24_PA_LOW);      // MANTIDO: PA_LOW
  radio.setAutoAck(false);
  radio.setRetries(0, 0);
  radio.setPayloadSize(sizeof(Packet));
  radio.openReadingPipe(0, address);
  radio.startListening();

  Serial.println("📡 RF24 configurado:");
  Serial.println("   - Canal: 80");
  Serial.println("   - DataRate: 2MBPS");
  Serial.println("   - PA Level: LOW");
  Serial.println("   - AutoAck: false");
  Serial.printf("   - Payload: %d bytes\n\n", sizeof(Packet));
  
  Serial.println("✅ Sistema pronto! Aguardando dados...\n");
  Serial.println("====================================\n");
}

void loop() {
  // Manter WebSocket
  webSocket.loop();
  
  // Verificar WiFi e WebSocket periodicamente
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 30000) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }
    if (!wsConnected && WiFi.status() == WL_CONNECTED) {
      connectWebSocket();
    }
    lastWifiCheck = millis();
  }

  // Receber pacotes NRF24 (prioridade máxima)
  while (radio.available()) {
    // Verificar buffer overflow
    if (bufferCount >= BUFFER_SIZE) {
      if (millis() - lastOverflowWarning > 5000) {
        Serial.println("⚠️ BUFFER OVERFLOW! Perdendo pacote mais antigo.");
        lastOverflowWarning = millis();
      }
      bufferReadIndex = (bufferReadIndex + 1) % BUFFER_SIZE;
      bufferCount--;
      packetsLost++;
    }

    // Ler pacote
    Packet tempPacket;
    radio.read(&tempPacket, sizeof(Packet));

    // Processar e adicionar ao buffer
    if (processPacket(&tempPacket)) {
      packetBuffer[bufferWriteIndex] = tempPacket;
      bufferWriteIndex = (bufferWriteIndex + 1) % BUFFER_SIZE;
      bufferCount++;
      lastReceiveTime = millis();

      // LED feedback
      digitalWrite(LED_PIN, HIGH);
      delayMicroseconds(50);
      digitalWrite(LED_PIN, LOW);
    }
  }

  // Enviar batch via WebSocket
  if (bufferCount >= BATCH_SIZE && (millis() - lastBatchSend) >= SEND_INTERVAL_MS) {
    sendBatchViaWebSocket();
    lastBatchSend = millis();
  }

  // Envio forçado por latência
  int estimatedLatency = bufferCount * 10;
  if (estimatedLatency > MAX_LATENCY_MS && bufferCount > 0 && (millis() - lastBatchSend) > 50) {
    static unsigned long lastForceLog = 0;
    if (millis() - lastForceLog > 5000) {
      Serial.printf("🚨 Envio forçado (latência: %dms, buffer: %d)\n", 
                    estimatedLatency, bufferCount);
      lastForceLog = millis();
    }
    sendBatchViaWebSocket();
    lastBatchSend = millis();
  }

  // Estatísticas
  if (millis() - lastStatsTime >= 3000) {
    showStatistics();
    lastStatsTime = millis();
  }

  // Timeout do emissor
  if (lastReceiveTime != 0 && (millis() - lastReceiveTime) > 5000) {
    if (millis() - lastWarning > 10000) {
      Serial.println("⚠️ SEM DADOS DO EMISSOR!");
      lastWarning = millis();
    }
  }

  delayMicroseconds(100);
}
#include <SPI.h>
#include <RF24.h>

#define CE_PIN 7
#define CSN_PIN 10
#define LED_PIN 8

RF24 radio(CE_PIN, CSN_PIN); //estou a criar um objeto que vai ser usado para controlar o nRF24l01 e dou os pins do CE e do CSN para ele saber logo quais são

//adress para onde vou receber
const byte address[6] = "00002";

// Vai criar a estrutura do que está a ser utilizado, tem de ser igual  ao do transmissor
#pragma pack(push, 1) //isto obriga a não colocar espaços brancos entre os dados
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

  Serial.println("==========================================");
  Serial.println("RECETOR - Compatível com padrões rotativos");
  Serial.println("==========================================");
  Serial.println();

  // Está a verificar se o NRF24l01 está bem ligado e responde
  if (!radio.begin()) { //eu ao fazer assim o radio.begin já o estou a iniciar e ao memso tempo a averificar se ele exisite ee stã bem colocado
    Serial.println("❌ NRF24L01 NÃO DETECTADO!");
    /*Serial.println("Verifique as ligações:");
    Serial.println("  VCC → 3.3V");
    Serial.println("  GND → GND");
    Serial.println("  CE  → PIN 7");
    Serial.println("  CSN → PIN 10");*/
    while (1);
  }

  Serial.println("✅ NRF24L01 OK");

  // ⚠️ CONFIGURAÇÕES IGUAIS AO TRANSMISSOR
  radio.setChannel(80); //estou a usar o canal 76, ou seja é na banda 2476 Hz, é sempre 2400+nºchannel
  radio.setDataRate(RF24_2MBPS); //a definir a velocidade da transmissão
  radio.setPALevel(RF24_PA_LOW); //RF24_PA_MIN , RF24_PA_LOW, RF24_PA_HIGH E RF24_PA_MAX, estou a defini r apotência do transmissor e quanto maior, maior alcance
  radio.setAutoAck(false);  //IMPORTANTE: igual ao transmissor (false) e como estamos a enviar muitos pacotes de maneira rápida, não faz mal perder um ou outro
  
  // Não é necessário setRetries quando AutoAck=false

  //abri o canal para poder receber as informações do transmissor e fiquei à espera
  radio.openReadingPipe(0, address);
  radio.startListening();

  Serial.println("📡 Configurações:");
  Serial.println("  - Canal: 80");
  Serial.println("  - Taxa: 2MBPS");
  Serial.println("  - Potência: LOW");
  Serial.println("  - AutoAck: false");
  Serial.println("  - Endereço: 00002");
  Serial.println();
  Serial.println("✅ Aguardando dados do transmissor...");
  Serial.println();
}

// ========================
// 🔁 LOOP PRINCIPAL
// ========================
void loop() {
  if (radio.available()) {
    // Ler pacote binário
    radio.read(&packet, sizeof(Packet)); //ao fazer isto ele vai buscar a mensagem ao buffer no nrf, lê os bits nela e coloca esses valores na variável Packet 
    lastReceiveTime = millis();
    packetsReceived++;

    // 🔐 Validar checksum
    uint8_t calc = calculateChecksum(&packet);
    if (calc != packet.checksum) {
      Serial.println("❌ Checksum inválido");
      return;
    }

    // 📉 Detetar perdas de pacotes
    if (!firstPacket) {
      uint8_t expected = lastSequence + 1;
      
      if (packet.sequence != expected) {
        int lost = packet.sequence - expected;
        if (lost < 0) lost += 256;  // Para overflow do uint8_t
        packetsLost += lost;
        
        Serial.print("⚠️ PERDA DETECTADA! Esperado: ");
        Serial.print(expected);
        Serial.print(" | Recebido: ");
        Serial.print(packet.sequence);
        Serial.print(" | Perdidos: ");
        Serial.print(lost);
        Serial.print(" | Total perdidos: ");
        Serial.println(packetsLost);
      }
    }

    firstPacket = false;
    lastSequence = packet.sequence;

    // 📊 Mostrar dados recebidos
    printPacket();

    // LED rápido (feedback visual)
    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(500);
    digitalWrite(LED_PIN, LOW);
  }
  
  // Opcional: Verificar se não recebe pacotes há muito tempo (timeout)
  static unsigned long lastWarning = 0;
  if (millis() - lastReceiveTime > 3000 && lastReceiveTime != 0) {
    if (millis() - lastWarning > 5000) {
      Serial.println("⚠️ Sem dados há 3 segundos... Verifique o transmissor");
      lastWarning = millis();
    }
  }
}

// ========================
// 📄 IMPRIMIR PACOTE
// ========================
void printPacket() {
  // Decodificar horas e minutos do timestamp
  uint8_t hora = (packet.timestamp >> 8) & 0xFF;
  uint8_t minuto = packet.timestamp & 0xFF;
  
  // Mostrar cabeçalho com informações principais
  Serial.print("📦 [#");
  Serial.print(packet.sequence);
  Serial.print("] ⏰ ");
  Serial.print(hora);
  Serial.print(":");
  if (minuto < 10) Serial.print("0");
  Serial.print(minuto);
  
  // Mostrar ACC (acelerômetro)
  Serial.print(" | ACC [");
  Serial.print(packet.accel[0]); 
  Serial.print(",");
  Serial.print(packet.accel[1]); 
  Serial.print(",");
  Serial.print(packet.accel[2]);
  Serial.print("]");
  
  // Mostrar GYRO (giroscópio)
  Serial.print(" | GYR [");
  Serial.print(packet.gyro[0]); 
  Serial.print(",");
  Serial.print(packet.gyro[1]); 
  Serial.print(",");
  Serial.print(packet.gyro[2]);
  Serial.print("]");
  
  // Mostrar MAG (magnetômetro)
  Serial.print(" | MAG [");
  Serial.print(packet.mag[0]); 
  Serial.print(",");
  Serial.print(packet.mag[1]); 
  Serial.print(",");
  Serial.print(packet.mag[2]);
  Serial.print("]");
  
  // Mostrar FSR (sensores de força)
  Serial.print(" | FSR [");
  Serial.print(packet.fsr[0]); 
  Serial.print(",");
  Serial.print(packet.fsr[1]); 
  Serial.print(",");
  Serial.print(packet.fsr[2]); 
  Serial.print(",");
  Serial.print(packet.fsr[3]);
  Serial.print("]");
  
  // Mostrar estatísticas a cada 10 pacotes
  if (packet.sequence % 10 == 0) {
    Serial.print(" | 📊 Total: ");
    Serial.print(packetsReceived);
    Serial.print(" | Perdidos: ");
    Serial.print(packetsLost);
    float lossRate = (float)packetsLost / (packetsReceived + packetsLost) * 100;
    Serial.print(" (");
    Serial.print(lossRate, 1);
    Serial.print("%)");
  }
  
  Serial.println();
}

/*
| nRF24L01 | ESP32-C3 Super Mini |
| -------- | ------------------- |
| VCC      | 3.3V                |
| GND      | GND                 |
| CE       | GPIO7               |
| CSN      | GPIO10              |
| SCK      | GPIO4               |
| MOSI     | GPIO6               |
| MISO     | GPIO5               |
*/
#include <WiFi.h>

const char* ssid = "NOS-454D";
const char* password = "SU7T2CL7";

const char* host = "192.168.1.14";
const int port = 8086;

WiFiClient client;

void setup() {

  Serial.begin(115200);

  delay(2000);

  Serial.println("📶 Ligando WiFi...");

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("✅ WiFi conectado!");

  Serial.println("🌐 A testar TCP...");

  if (client.connect(host, port)) {

    Serial.println("✅ TCP CONNECT OK");

    client.stop();

  } else {

    Serial.println("❌ TCP CONNECT FAILED");
  }
}

void loop() {

}
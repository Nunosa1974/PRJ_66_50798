#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

const char* ssid = "Nunosa";
const char* password = "Panda@135790";

#define INFLUXDB_URL "https://eu-central-1-1.aws.cloud2.influxdata.com"

#define INFLUXDB_TOKEN "DX-OKtaiaM2ANQ2QxA_SwALJTtHsTuajkQc3T6gPVgJkq0oairSkPUl7C8rpLRoXVr3K2v-tCbHkvIS4prdwNw=="

#define INFLUXDB_ORG "Projeto_ISEL"

#define INFLUXDB_BUCKET "Projeto_Boxe22"

#define TZ_INFO "WET0WEST,M3.5.0/1,M10.5.0"

// ↓↓↓ ISTO É A PARTE IMPORTANTE
InfluxDBClient client(
  INFLUXDB_URL,
  INFLUXDB_ORG,
  INFLUXDB_BUCKET,
  INFLUXDB_TOKEN,
  InfluxDbCloud2CACert
);

Point sensor("boxe");

void setup() {

  Serial.begin(115200);

  WiFi.begin(ssid, password);

  Serial.print("Ligando WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi ligado!");

  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  if (client.validateConnection()) {

    Serial.println("Influx Cloud ligada!");

  } else {

    Serial.print("Erro: ");
    Serial.println(client.getLastErrorMessage());
  }
  sensor.addTag("glove", "left");
}

void loop() {

  sensor.clearFields();

  sensor.addField("force", random(0, 500));
  sensor.addField("accelX", random(-100, 100));

  if (client.writePoint(sensor)) {

    Serial.println("Enviado!");

  } else {

    Serial.print("Erro envio: ");
    Serial.println(client.getLastErrorMessage());
  }

  delay(200);
}
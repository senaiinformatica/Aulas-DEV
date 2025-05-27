#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "internet.h"
#include <Bounce2.h>
#include <ezTime.h>
#include <DHT.h>

#define DHTPIN 25
#define DHTTYPE DHT22

#define pinLed 2

WiFiClient espClient;
PubSubClient client(espClient);
Bounce botao = Bounce();
Timezone tempo;
DHT dht(DHTPIN, DHTTYPE);


const char *mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *mqtt_id = "esp32-senai134-thiago";
const char *mqtt_topic_sub = "senai134/mesa08/esp_inscrito";
const char *mqtt_topic_pub = "senai134/mesa08/esp_publicando";

bool estadoLed = false;
bool modoPisca = false;
bool envioMqtt = false;

unsigned long tempoEspera = 500;
unsigned long tempoEnvio = 5000;

void callback(char *, byte *, unsigned int);
void mqttConnect(void);
void controleLed(void);

void setup()
{
  Serial.begin(9600);
  dht.begin();
  pinMode(pinLed, OUTPUT);

  botao.attach(0, INPUT_PULLUP);

  conectaWiFi();

  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  waitForSync();
  tempo.setLocation("America/Sao_Paulo");
  
  pinMode(0, INPUT_PULLUP);
}

void loop()
{
  checkWiFi();

  if (!client.connected())
    mqttConnect();

  client.loop();
  
  static unsigned long tempoAnterior = 0;
  unsigned long agora = millis();
  
  JsonDocument doc;
  String mensagem;
  
  
  botao.update();

  float temperatura, umidade;

  if(agora - tempoAnterior > tempoEnvio)
  {
    temperatura = dht.readTemperature();
    umidade= dht.readHumidity();

    doc["temp"] = temperatura;
    doc["umid"] = umidade;
    envioMqtt = true;
    tempoAnterior = agora;
  }


  if (botao.changed())
  {
    doc["botao"] = !botao.read();
    envioMqtt = true;
  }
  
  if (envioMqtt)
  {
    doc["timestamp"] = tempo.now();
    serializeJson(doc, mensagem);
    Serial.println(mensagem);
    client.publish(mqtt_topic_pub, mensagem.c_str());
    envioMqtt = false;
  }
  controleLed();
}

void callback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("mensagem recebida em %s: ", topic);

  String mensagem = "";
  for (unsigned int i = 0; i < length; i++)
  {
    char c = (char)payload[i];
    mensagem += c;
  }
  Serial.println(mensagem);

  JsonDocument doc;
  deserializeJson(doc, mensagem);

  if (!doc["estadoLed"].isNull())
  {
    estadoLed = doc["estadoLed"];
    modoPisca = 0;
  }

  if (!doc["modoPisca"].isNull())
  {
    modoPisca = doc["modoPisca"];
  }

  if (!doc["velocidade"].isNull())
  {
    tempoEspera = doc["velocidade"];
  }

  if(!doc["tempoEnvio"].isNull())
  {
    tempoEnvio = doc["tempoEnvio"];
  }
}

void mqttConnect()
{
  while (!client.connected())
  {
    Serial.println("Conectando ao MQTT...");

    if (client.connect(mqtt_id))
    {
      Serial.println("Conectado com sucesso");
      client.subscribe(mqtt_topic_sub);
    }

    else
    {
      Serial.print("falha, rc=");
      Serial.println(client.state());
      Serial.println("tentando novamente em 5 segundos");
      delay(5000);
    }
  }
}

void controleLed()
{
  static unsigned long ultimaMudanca = 0;
  unsigned long agora = millis();

  if (modoPisca)
  {
    if (agora - ultimaMudanca > tempoEspera)
    {
      estadoLed = !estadoLed;
      ultimaMudanca = agora;
    }
  }

  digitalWrite(pinLed, estadoLed);
}
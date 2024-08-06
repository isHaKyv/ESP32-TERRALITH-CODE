#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <DHT.h>
#include <PubSubClient.h>

// Credenciales de tu red WiFi
const char *ssid = "Atrizel203";
const char *password = "Angelitori29100";


// MQTT
const char *mqtt_server = "3.92.107.93"; 
const int mqtt_port = 1883;
const char *mqtt_topic = "sensor/data";

WiFiClient espClient;
PubSubClient client(espClient);

// Servidores
AsyncWebServer server(80);
WebSocketsServer webSocket(81);
const unsigned long sendInterval = 1000;

// Sensor de humedad de suelo
const uint16_t dry = 636; 
const uint16_t wet = 265; 
const uint8_t sensorPin = 35;
uint16_t sensorReading;

// Sensor de temperatura DHT11
#define DHTPIN 26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// Sensor de proximidad
const uint8_t proximitySensorPin = 34; 
uint16_t proximityReading;

void setup() {
    Serial.begin(115200);
    analogReadResolution(10);

    // Conectar a la red WiFi
    WiFi.begin(ssid, password);
    Serial.println();
    Serial.print("Conectando a ");
    Serial.println(ssid);

    // Esperar hasta que se conecte a la red WiFi
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
    }

    Serial.println();
    Serial.println("Conectado a la red WiFi");
    Serial.print("Direccion IP: ");
    Serial.println(WiFi.localIP());

    if (SPIFFS.begin(true)) {
        Serial.println("SPIFFS montada satisfactoriamente");

        // Imprime la lista de archivos en el directorio "data" (Opcional)
        File root = SPIFFS.open("/");
        File file = root.openNextFile();
        while (file) {
            Serial.print("Archivo: ");
            Serial.println(file.name());
            file = root.openNextFile();
        }
    } else {
        Serial.println("Error montando SPIFFS");
    }

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/index.html", "text/html");
    });

    // Ruta para cargar el archivo style.css
    server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/style.css", "text/css");
    });

    // Ruta para cargar el archivo script.js
    server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(SPIFFS, "/script.js", "text/javascript");
    });

    server.onNotFound(notFound);
    server.begin();

    webSocket.begin();
    webSocket.onEvent(handleWebSocketMessage);

    dht.begin();

    // Configurar conexión MQTT
    client.setServer(mqtt_server, mqtt_port);
    reconnect();
}

void loop() {
    webSocket.loop(); // WebSocket events
    client.loop(); // MQTT events

    static uint32_t prevMillis = 0;
    if (millis() - prevMillis >= sendInterval) {
        prevMillis = millis();

        // Lectura del sensor de humedad del suelo y conversión a porcentaje
        sensorReading = analogRead(sensorPin);
        uint16_t moisturePercentage = map(sensorReading, wet, dry, 100, 0);
        moisturePercentage = constrain(moisturePercentage, 0, 100);

        // Leer la temperatura del DHT11
        float temperature = dht.readTemperature();

        // Leer el valor del sensor de proximidad
        proximityReading = analogRead(proximitySensorPin);

        String status;
        if (moisturePercentage < 25)
            status = "Suelo muy seco - Regar!";
        else if (moisturePercentage >= 25 && moisturePercentage < 70)
            status = "Humedad de Suelo Ideal";
        else
            status = "Suelo demasiado HUMEDO!";

        sendData(moisturePercentage, temperature, proximityReading, status);
    }
}

void sendData(uint16_t humidityPercentage, float temperature, uint16_t proximity, String statusMessage) {
  
    String data = "{\"humidity\":" + String(humidityPercentage) + ",\"temperature\":" + String(temperature) + ",\"proximity\":" + String(proximity) + ",\"status\":\"" + statusMessage + "\"}";

   
    webSocket.broadcastTXT(data);

    // Publicar en MQTT if (!client.connected()) {
        reconnect();
    }
    client.publish(mqtt_topic, data.c_str());

 
    Serial.println(data);
}

void handleWebSocketMessage(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_DISCONNECTED:
            Serial.printf("[%u] Desconectado!\n", num);
            break;
        case WStype_CONNECTED: {
            IPAddress ip = webSocket.remoteIP(num);
            Serial.printf("[%u] Conectado desde %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        } break;
        case WStype_TEXT:
            Serial.printf("[%u] Texto recibido: %s\n", num, payload);
            break;
    }
}

void notFound(AsyncWebServerRequest *request) {
    request->send(404, "text/plain", "Pagina no encontrada!");
}

void reconnect() {
 
    while (!client.connected()) {
        Serial.print("Attempting MQTT connection...");
  
        if (client.connect("ESP32Client")) {
            Serial.println("connected");
        } else {
            Serial.print("failed, rc=");
            Serial.print(client.state());
            Serial.println(" try again in 5 seconds");
        
            delay(5000);
        }
    }
}

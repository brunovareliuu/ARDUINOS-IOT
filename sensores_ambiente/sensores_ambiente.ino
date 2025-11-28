#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- PARA HTTPS (Azure)!

// Librerías para el sensor de temperatura
#include "DHT.h"
#include "Adafruit_Sensor.h"

// --- 1. Configuración WiFi ---
// const char* ssid = "Tec-IoT";                    
// const char* password = "spotless.magnetic.bridge"; 
const char* ssid = "IZZI-B790";         // <- WiFi actual
const char* password = "2C9569A8B790";  // <- WiFi actual

// --- 2. Configuración API (AZURE) ---
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URLs para ambos sensores
const char* apiURL_Temperatura = "https://%s/Sensores/TemperaturaEstacionamiento";
const char* apiURL_Luz         = "https://%s/Sensores/LuzNocturnaPost";

// --- 3. Configuración Sensor Temperatura ---
// DATA del DHT11 en D2 -> GPIO4
const int DHTPIN = 4;
// LED de temperatura en D4 -> GPIO2
const int LED_TEMP_PIN = 2;
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// --- 4. Configuración Sensor Luz (fotoresistor) ---
const int pinSensorLuz = A0;

// LEDs de luz nocturna (usar GPIO directos)
const int ledOscuroPin = 12; // GPIO12 (D6) - se enciende cuando está oscuro
const int ledClaroPin  = 15; // GPIO15 (D8) - se enciende cuando hay luz
const int ledExtra1    = 14; // GPIO14 (D5)
const int ledExtra2    = 13; // GPIO13 (D7)

// --- 5. Lógica de Estado ---
const int UMBRAL_LDR = 45; 
bool estadoActualOscuro = false;

// --- 6. Intervalos ---
const int intervaloLectura   = 2000;   // leer sensores cada 2 seg
const int intervaloTempPOST  = 30000;  // enviar temp cada 30 seg

// --- 7. Timers ---
unsigned long tiempoAnteriorTempPOST = 0;
float ultimaTempValida = 0;

void setup() {
  Serial.begin(115200);

  // Inicializar sensor de temperatura
  dht.begin();
  pinMode(LED_TEMP_PIN, OUTPUT);
  digitalWrite(LED_TEMP_PIN, LOW);

  // Configurar pines del sensor de luz
  pinMode(pinSensorLuz, INPUT);
  pinMode(ledOscuroPin, OUTPUT);
  pinMode(ledClaroPin, OUTPUT);
  pinMode(ledExtra1, OUTPUT);
  pinMode(ledExtra2, OUTPUT);

  digitalWrite(ledOscuroPin, LOW);
  digitalWrite(ledClaroPin, LOW);
  digitalWrite(ledExtra1, LOW);
  digitalWrite(ledExtra2, LOW);

  // --- Conectar a WiFi ---
  delay(10);
  Serial.println();
  Serial.println("--- Conectando a WiFi (Sensores Combinados) ---");
  WiFi.begin(ssid, password);
  Serial.print("Conectando a: ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP8266: ");
  Serial.println(WiFi.localIP());
  Serial.println("Sistema de Sensores de Ambiente INICIADO.");

  // Detectar estado inicial de luz
  estadoActualOscuro = (analogRead(pinSensorLuz) < UMBRAL_LDR);
  Serial.print("Estado inicial de luz: ");
  Serial.println(estadoActualOscuro ? "Oscuro" : "Claro");

  // Poner LEDs iniciales de luz nocturna
  actualizarLEDsLuz(estadoActualOscuro);
}

void loop() {
  unsigned long tiempoActual = millis();

  // --- 1. Leer sensores cada 2 seg ---
  leerSensores();

  // --- 2. Enviar temperatura cada 30 seg ---
  if (tiempoActual - tiempoAnteriorTempPOST >= (unsigned long)intervaloTempPOST) {
    Serial.println("\n--- Han pasado 30 seg ---");
    Serial.println("Enviando temperatura a la API...");
    enviarTemperatura(ultimaTempValida);
    tiempoAnteriorTempPOST = tiempoActual;
  }

  delay(intervaloLectura);
}

// ==========================================================
// FUNCIÓN PARA LEER SENSORES
// ==========================================================
void leerSensores() {
  // Leer DHT11 (temperatura y humedad)
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (!isnan(t) && !isnan(h)) {
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print(" °C  | Humedad: ");
    Serial.print(h);
    Serial.print(" %");

    ultimaTempValida = t;

    // Condición del LED de temperatura (ejemplo: >25 °C)
    if (t > 25) digitalWrite(LED_TEMP_PIN, HIGH);
    else        digitalWrite(LED_TEMP_PIN, LOW);
  } else {
    Serial.print("Error leyendo DHT11");
  }

  // Leer luz (fotoresistor en A0)
  int nivelLuz = analogRead(pinSensorLuz);
  Serial.print(" | Nivel_Luz: ");
  Serial.println(nivelLuz);

  bool ahoraEstaOscuro = (nivelLuz < UMBRAL_LDR);

  if (ahoraEstaOscuro != estadoActualOscuro) {
    Serial.println("-------------------------------------");
    Serial.println(">>> ¡CAMBIO DE ESTADO DE LUZ! <<<");

    if (ahoraEstaOscuro) Serial.println("Ahora está OSCURO");
    else                 Serial.println("Ahora hay LUZ");

    // Actualizar LEDs de luz nocturna (GPIO 14,12,13,15)
    actualizarLEDsLuz(ahoraEstaOscuro);

    // Enviar nivel de luz a la API
    enviarNivelLuz(nivelLuz);

    estadoActualOscuro = ahoraEstaOscuro;
    Serial.println("-------------------------------------");
  }
}

// ==========================================================
// FUNCIÓN PARA ACTUALIZAR LEDS DE LUZ (D5, D6, D7, D8)
// ==========================================================
void actualizarLEDsLuz(bool esOscuro) {
  if (esOscuro) {
    // Oscuro -> encender TODOS los LEDs de luz
    digitalWrite(ledOscuroPin, HIGH); // GPIO12 (D6) - ON
    digitalWrite(ledClaroPin, HIGH);  // GPIO15 (D8) - ON
    digitalWrite(ledExtra1, HIGH);    // GPIO14 (D5) - ON
    digitalWrite(ledExtra2, HIGH);    // GPIO13 (D7) - ON
  } else {
    // Claro -> apagar TODOS los LEDs de luz
    digitalWrite(ledOscuroPin, LOW);  // GPIO12 (D6) - OFF
    digitalWrite(ledClaroPin, LOW);   // GPIO15 (D8) - OFF
    digitalWrite(ledExtra1, LOW);     // GPIO14 (D5) - OFF
    digitalWrite(ledExtra2, LOW);     // GPIO13 (D7) - OFF
  }
}

// ==========================================================
// FUNCIÓN PARA POST TEMPERATURA
// ==========================================================
void enviarTemperatura(float temp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST Temperatura: No hay WiFi.");
    return;
  }

  if (temp == 0) {
    Serial.println("POST Temperatura: Temperatura inválida, no se enviará.");
    return;
  }

  String payload = "{ \"Temperatura\": " + String(temp) + " }";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_Temperatura, IP_SERVIDOR);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);

    Serial.print("Código respuesta Temperatura: ");
    Serial.println(httpCode);

    if (httpCode == 200 || httpCode == 201) {
      Serial.println("¡Temperatura registrada en Azure!");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP (Temperatura).");
  }
}

// ==========================================================
// FUNCIÓN PARA POST LUZ
// ==========================================================
void enviarNivelLuz(int nivel) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST Luz: No hay WiFi.");
    return;
  }

  String payload = "{ \"Nivel_Luz\": " + String(nivel) + " }";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_Luz, IP_SERVIDOR);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);

    Serial.print("Código respuesta Luz: ");
    Serial.println(httpCode);

    if (httpCode == 200 || httpCode == 201) {
      Serial.println("¡Estado de luz registrado en Azure!");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP (Luz).");
  }
}
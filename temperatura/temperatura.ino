#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Librerías para el sensor
#include "DHT.h"
#include "Adafruit_Sensor.h"

// --- 1. Configuración WiFi (Nuestros datos) ---
const char* ssid = "Tec-IoT";
const char* password = "spotless.magnetic.bridge";

// --- 2. Configuración API (Nuestra IP) ---
const char* IP_DE_TU_COMPUTADORA = "10.22.195.99"; // <--- Tu IP
const char* apiURL_plantilla = "http://%s:5074/Sensores/TemperaturaEstacionamiento"; 

// --- 3. Configuración Sensor (Tus pines) ---
#define DHTPIN D4       // D4 = GPIO 2
#define DHTTYPE DHT11
#define LEDPIN D1       // D1 = GPIO 5

DHT dht(DHTPIN, DHTTYPE);

// --- 4. Intervalos (Tu lógica) ---
const int intervaloLectura = 2000;   // Leer y mostrar cada 2 segundos
const int intervaloPOST = 30000;     // Enviar a la API cada 30 segundos

// --- 5. Timers (millis()) ---
unsigned long tiempoAnteriorPOST = 0;
float ultimaTempValida = 0; // Guardar la última temp para el POST

void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW);

  // --- Conectar a WiFi (Nuestra lógica) ---
  delay(10); 
  Serial.println();
  Serial.println("--- Conectando a WiFi ---");
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
  
  Serial.println("Sensor de Temperatura INICIADO.");
}

void loop() {
  // --- 1. LEER Y MOSTRAR (Cada 2 segundos) ---
  
  // Leer Sensor
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    Serial.println("Error leyendo el sensor DHT11");
  } else {
    // Si la lectura es válida:
    // Imprimir localmente
    Serial.print("Lectura en vivo -> Temp: ");
    Serial.print(t);
    Serial.print(" °C  |  Humedad: ");
    Serial.print(h);
    Serial.println(" %");

    // Guardar el valor para el POST
    ultimaTempValida = t; 

    // Actuar localmente (LED en D1)
    if (t > 25) { 
      digitalWrite(LEDPIN, HIGH);
    } else {
      digitalWrite(LEDPIN, LOW);
    }
  }

  // --- 2. ENVIAR A LA API (Cada 30 segundos) ---
  
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoAnteriorPOST >= intervaloPOST) {
    
    Serial.println("\n--- Han pasado 30 seg ---");
    Serial.println("Enviando última temperatura válida a la API...");
    
    // Enviar la última temperatura que leímos
    enviarTemperatura(ultimaTempValida);
    
    // Reiniciar el timer
    tiempoAnteriorPOST = tiempoActual;
    Serial.println("--------------------------\n");
  }

  // Esperar 2 segundos antes de la siguiente LECTURA
  delay(intervaloLectura); 
}


// --- Función para enviar los POST (Sin cambios) ---
void enviarTemperatura(float temp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi.");
    return;
  }
  
  if (temp == 0) {
    Serial.println("POST: Temperatura inválida (0.0), no se enviará.");
    return;
  }

  // Crear el payload JSON: {"Temperatura": 31.0}
  String payload = "{";
  payload += "\"Temperatura\": ";
  payload += String(temp);
  payload += "}";

  Serial.print("Enviando JSON: ");
  Serial.println(payload);

  WiFiClient client;
  HTTPClient http;

  // Construir la URL completa
  char urlCompleta[100];
  sprintf(urlCompleta, apiURL_plantilla, IP_DE_TU_COMPUTADORA);

  Serial.print("URL destino: ");
  Serial.println(urlCompleta);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(payload);
    
    Serial.print("Código de respuesta HTTP: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("¡Temperatura registrada en la API!");
    } else {
      Serial.println("Error al registrar en la API.");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}
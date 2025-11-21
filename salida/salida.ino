#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- ¡CAMBIO: Para HTTPS (Azure)!
#include <ArduinoJson.h> 
#include <time.h>        
#include <sys/time.h>    

// --- 1. Configuración WiFi ---
const char* ssid = "Tec-IoT";
const char* password = "spotless.magnetic.bridge";

// --- 2. Configuración API (AZURE) ---
// Ponemos el dominio SIN "https://" y SIN "/" al final
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net"; 

// URLs actualizadas para HTTPS y sin puerto 5074
const char* apiURL_POST_Salida = "https://%s/Sensores/SalidaRegistro";
const char* apiURL_GET_Disponibilidad = "https://%s/Sensores/Disponibilidad";

// --- 3. Configuración Pines ---
const int botonPin = D1;
const int ledVerdePin = D2; // Verde = Hay Lugares
const int ledRojoPin = D4;  // Rojo = Lleno

// --- 4. Configuración de Timers ---
int estadoBotonAnterior = HIGH; 
unsigned long tiempoAnteriorGET = 0;
const long intervaloGET = 5000; // 5 segundos

// --- Configuración de Hora (NTP) ---
const char* ntpServer = "pool.ntp.org";

void setup() {
  Serial.begin(115200);

  // Configurar pines
  pinMode(botonPin, INPUT_PULLUP); 
  pinMode(ledVerdePin, OUTPUT);
  pinMode(ledRojoPin, OUTPUT);

  // Estado inicial de LEDs (Rojo = "Conectando...")
  digitalWrite(ledVerdePin, LOW);
  digitalWrite(ledRojoPin, HIGH); 

  // --- Conectar a WiFi ---
  Serial.println();
  Serial.println("--- Conectando a WiFi (Modo Azure) ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP8266: ");
  Serial.println(WiFi.localIP());

  // --- Sincronizar la hora ---
  Serial.println("Sincronizando hora...");
  configTime(0, 0, ntpServer); 
  while (time(nullptr) < 8 * 3600 * 2) { 
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Hora sincronizada!");
  
  Serial.println("Sistema de Salida y Disponibilidad INICIADO.");
  
  // Primera llamada para actualizar LEDs
  actualizarDisponibilidad(); 
}


void loop() {
  // --- TRABAJO 1: Chequear el botón (POST) ---
  chequearBoton();

  // --- TRABAJO 2: Chequear la disponibilidad (GET) ---
  unsigned long tiempoActual = millis();
  if (tiempoActual - tiempoAnteriorGET >= intervaloGET) {
    tiempoAnteriorGET = tiempoActual; // Reiniciar el timer
    Serial.println("\nActualizando disponibilidad (GET)...");
    actualizarDisponibilidad();
  }
}

// --- Esta función se encarga SOLO de los LEDs (GET) ---
void actualizarDisponibilidad() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("GET: No hay WiFi.");
    digitalWrite(ledVerdePin, LOW);
    digitalWrite(ledRojoPin, HIGH);
    return;
  }

  // --- CAMBIO: Usar WiFiClientSecure para HTTPS ---
  WiFiClientSecure client;
  client.setInsecure(); // <--- IMPORTANTE: Confiar en el certificado de Azure

  HTTPClient http;

  // Construir la URL del GET
  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_GET_Disponibilidad, IP_SERVIDOR);
  
  Serial.print("GET URL: ");
  Serial.println(urlCompleta);

  if (http.begin(client, urlCompleta)) {
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      Serial.println("GET: Respuesta OK (200).");
      String payload = http.getString();
      Serial.println(payload); 

      // --- Leer JSON ---
      StaticJsonDocument<128> doc; 
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("deserializeJson() falló: ");
        Serial.println(error.c_str());
        digitalWrite(ledVerdePin, LOW);
        digitalWrite(ledRojoPin, HIGH);
      } else {
        // ¡Lectura exitosa!
        int disponibles = doc["disponibles"];
        Serial.print("Lugares disponibles: ");
        Serial.println(disponibles);

        // --- LA LÓGICA DE LOS LEDS ---
        if (disponibles > 0) {
          Serial.println("Estado: HAY LUGARES");
          digitalWrite(ledVerdePin, HIGH); // HAY LUGAR
          digitalWrite(ledRojoPin, LOW);
        } else {
          Serial.println("Estado: LLENO");
          digitalWrite(ledVerdePin, LOW);
          digitalWrite(ledRojoPin, HIGH); // LLENO
        }
      }

    } else {
      Serial.print("GET: Error en la llamada, código: ");
      Serial.println(httpCode);
      digitalWrite(ledVerdePin, LOW);
      digitalWrite(ledRojoPin, HIGH);
    }
    http.end();
  } else {
    Serial.println("GET: No se pudo conectar a la URL.");
  }
}


// --- Esta función se encarga SOLO del botón (POST) ---
void chequearBoton() {
  int estadoBotonActual = digitalRead(botonPin);

  // Detectar "flanco de bajada"
  if (estadoBotonActual == LOW && estadoBotonAnterior == HIGH) {
    Serial.println("\n¡Botón presionado! Registrando salida (POST)...");
    enviarRegistroSalida();
    delay(200);
  }
  
  estadoBotonAnterior = estadoBotonActual;
}

// --- Esta función SOLO envía el POST ---
void enviarRegistroSalida() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi para enviar el registro.");
    return; 
  }
    
  // --- Crear el Timestamp ---
  struct timeval tv;
  gettimeofday(&tv, nullptr); 
  char timestampBase[40];
  strftime(timestampBase, sizeof(timestampBase), "%Y-%m-%dT%H:%M:%S", gmtime(&tv.tv_sec));
  char timestampFinal[50];
  sprintf(timestampFinal, "%s.%03ldZ", timestampBase, tv.tv_usec / 1000);

  // Crear el payload JSON
  String payload = "{";
  payload += "\"id_salida_Pk\": 0,"; 
  payload += "\"fecha_Hora_Salida\": \""; 
  payload += timestampFinal;
  payload += "\"}";

  Serial.print("POST: Enviando JSON: ");
  Serial.println(payload);

  // --- CAMBIO: Usar WiFiClientSecure para HTTPS ---
  WiFiClientSecure client;
  client.setInsecure(); // <--- IMPORTANTE

  HTTPClient http;

  // Construir la URL del POST
  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_POST_Salida, IP_SERVIDOR);

  Serial.print("POST URL: ");
  Serial.println(urlCompleta);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(payload);
    
    Serial.print("POST: Código de respuesta HTTP: ");
    Serial.println(httpCode);
    
    if (httpCode == HTTP_CODE_OK || httpCode == 200 || httpCode == 201) { 
      Serial.println("¡Salida registrada en Azure con éxito!");
    } else {
      Serial.println("POST: Error al registrar.");
    }
    http.end();
  } else {
    Serial.println("POST: Error al iniciar conexión HTTP.");
  }
}
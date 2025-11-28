#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <ArduinoJson.h> 
#include <time.h>       
#include <sys/time.h>   

// --- 1. Configuración WiFi ---
const char* ssid = "IZZI-B790";
const char* password = "2C9569A8B790";

// --- 2. AZURE API ---
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net"; 
String apiURL_POST = "https://" + String(IP_SERVIDOR) + "/Sensores/EntradaRegistro";
String apiURL_GET  = "https://" + String(IP_SERVIDOR) + "/Sensores/Disponibilidad";

// --- 3. HARDWARE ---
const int trigPin = D1; 
const int echoPin = D2; 

// LEDS
const int ledRojo = D6;   
const int ledVerde = D7;  
const int ledAzul = D5;   

// SEÑAL AL ARDUINO (Nuevo)
const int pinSenalArduino = D8; 

const int UMBRAL_DISTANCIA = 5; 

// --- 4. VARIABLES DE CONTROL ---
bool carroPresente = false;
unsigned long ultimoChequeo = 0;
bool hayLugares = true;
bool entradaAutorizada = false;  // Para parpadeo visual
bool barreraAbierta = false;     // Para saber si se envió señal al Arduino
unsigned long ultimoParpadeo = 0;

// --- HORA ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600; 
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);
  pinMode(ledRojo, OUTPUT); pinMode(ledVerde, OUTPUT); pinMode(ledAzul, OUTPUT);
  
  // Configurar pin de señal para el Arduino
  pinMode(pinSenalArduino, OUTPUT);
  digitalWrite(pinSenalArduino, LOW); // Inicialmente LOW (Barrera cerrada)

  // Test inicial
  digitalWrite(ledRojo, HIGH); delay(200); digitalWrite(ledRojo, LOW);
  digitalWrite(ledVerde, HIGH); delay(200); digitalWrite(ledVerde, LOW);
  digitalWrite(ledAzul, HIGH);  delay(200); digitalWrite(ledAzul, LOW);

  Serial.println("\n--- SISTEMA ESP8266: CEREBRO DE ENTRADA ---");
  Serial.println("--- ENVIANDO SEÑAL A ARDUINO POR D8 ---");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledAzul, HIGH); delay(100); digitalWrite(ledAzul, LOW); delay(100);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  checarSemaforo();
}

void loop() {
  long duration;
  int distance;
  
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  Serial.print("Distancia: "); Serial.println(distance); 

  // CASO ESPECIAL: DISTANCIA CERO (Error)
  if (distance == 0) {
    digitalWrite(ledAzul, HIGH);
    digitalWrite(ledVerde, LOW);
    digitalWrite(ledRojo, LOW);
    delay(200);
    return;
  }

  // CASO A: CARRO DETECTADO (< 5cm)
  if (distance > 0 && distance <= UMBRAL_DISTANCIA) {
    
    if (carroPresente == false) {
      Serial.println("\n>>> 🚗 CARRO LLEGANDO...");
      digitalWrite(ledAzul, LOW);

      // INICIAR PARPADEO VERDE INMEDIATAMENTE
      entradaAutorizada = true;
      digitalWrite(ledRojo, LOW);
      ultimoParpadeo = millis();
      Serial.println("🔄 PROCESANDO...");
      digitalWrite(ledVerde, HIGH); // Primer flash

      int resultado = enviarRegistroEntrada();
      
      if (resultado == 200 || resultado == 201) {
        Serial.println("✅ AUTORIZADO. ABRIENDO BARRERA...");

        // --- ENVIAR SEÑAL AL ARDUINO PARA ABRIR ---
        Serial.println(">>> 📡 MANDANDO SIGNAL AL ARDUINO (HIGH en D8)...");
        digitalWrite(pinSenalArduino, HIGH);
        barreraAbierta = true; // Marcar que la barrera se abrió
        // -----------------------------------------
      }
      else {
        Serial.println("❌ DENEGADO.");
        entradaAutorizada = false;
        barreraAbierta = false; // No se abrió la barrera
        digitalWrite(ledVerde, LOW);
        for(int i=0; i<5; i++) { digitalWrite(ledRojo, LOW); delay(100); digitalWrite(ledRojo, HIGH); delay(100); }
        digitalWrite(ledRojo, HIGH);

        // NO enviar señal al Arduino (no autorizado)
        // digitalWrite(pinSenalArduino, LOW); // Comentado - no enviar
      }

      carroPresente = true; 
    }
  }

  // CASO B: EL CARRO SE FUE (> 5cm)
  else {
    if (carroPresente == true) {
      Serial.println("\n>>> 💨 CARRO SE FUE. CERRANDO.");

      // --- CERRAR BARRERA SOLO SI REALMENTE SE ABRIÓ ---
      if (barreraAbierta) {
        Serial.println(">>> 📡 CORTANDO SIGNAL AL ARDUINO (LOW en D8)...");
        digitalWrite(pinSenalArduino, LOW);
        barreraAbierta = false; // Resetear estado
      } else {
        Serial.println(">>> 📡 NO SE ENVÍA SEÑAL (carro no autorizado)...");
      }
      // -------------------------------------------------

      digitalWrite(ledVerde, LOW);
      digitalWrite(ledRojo, LOW);
      entradaAutorizada = false;
      digitalWrite(ledAzul, LOW);

      carroPresente = false;
      checarSemaforo();
    }

    // MODO SEMÁFORO NORMAL
    if (millis() - ultimoChequeo > 2000) {
      checarSemaforo(); 
      ultimoChequeo = millis();
      
      if (hayLugares) {
        digitalWrite(ledVerde, HIGH); 
        digitalWrite(ledRojo, LOW);
      } else {
        digitalWrite(ledVerde, LOW);
        digitalWrite(ledRojo, HIGH); 
      }
      digitalWrite(ledAzul, LOW);
    }
  }

  // CONTROL DE PARPADEO DEL LED VERDE
  if (carroPresente && entradaAutorizada) {
    if (millis() - ultimoParpadeo >= 300) { 
      ultimoParpadeo = millis();
      digitalWrite(ledVerde, !digitalRead(ledVerde)); 
    }
  } else if (carroPresente && !entradaAutorizada) {
    digitalWrite(ledVerde, LOW);
  }
  
  delay(200); 
}

// --- FUNCIONES AUXILIARES ---
void checarSemaforo() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client; client.setInsecure(); HTTPClient http;
  if (http.begin(client, apiURL_GET)) {
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<96> doc; deserializeJson(doc, payload);
      int disponibles = doc["disponibles"];
      hayLugares = (disponibles > 0);
    }
    http.end();
  }
}

int enviarRegistroEntrada() {
  if (WiFi.status() != WL_CONNECTED) return -1;
  struct timeval tv; gettimeofday(&tv, nullptr); 
  char timestampBase[40]; strftime(timestampBase, sizeof(timestampBase), "%Y-%m-%dT%H:%M:%S", localtime(&tv.tv_sec));
  char timestampFinal[50]; sprintf(timestampFinal, "%s.%03ld", timestampBase, tv.tv_usec / 1000);
  String payload = "{ \"id_entrada_Pk\": 0, \"fecha_Hora_Entrada\": \"" + String(timestampFinal) + "\"}";

  WiFiClientSecure client; client.setInsecure();
  HTTPClient http; http.setTimeout(10000); 
  int httpCode = -1;
  Serial.print("Enviando POST... ");
  if (http.begin(client, apiURL_POST)) {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(payload);
    Serial.printf("Código: %d\n", httpCode);
    http.end();
  }
  return httpCode;
}

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> 
#include <time.h>       
#include <sys/time.h>   

// --- 1. Configuración WiFi ---
// const char* ssid = "Tec-IoT";                    // <- WiFi anterior (comentado para documentación)
// const char* password = "spotless.magnetic.bridge"; // <- WiFi anterior (comentado para documentación)

const char* ssid = "IZZI-B790";         // <- WiFi actual
const char* password = "2C9569A8B790";  // <- WiFi actual

// --- 2. Configuración API ---
const char* apiURL = "https://estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net/Sensores/EntradaRegistro";

// --- 3. Configuración Sensor y LEDs ---
const int trigPin = D1; 
const int echoPin = D2; 
const int UMBRAL_DISTANCIA = 5; 

const int ledRojoEsperando = D3; 
const int ledVerdeDetectado = D5;  

// --- Configuración de Hora ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -21600; 
const int daylightOffset_sec = 0;

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(ledRojoEsperando, OUTPUT); 
  pinMode(ledVerdeDetectado, OUTPUT);
  
  digitalWrite(ledRojoEsperando, HIGH); // Rojo ON (Inicio)
  digitalWrite(ledVerdeDetectado, LOW);

  Serial.println("\n--- Conectando a WiFi ---");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(ledRojoEsperando, LOW);
    digitalWrite(ledRojoEsperando, HIGH);
    Serial.print(".");
  }
  Serial.println("\n¡WiFi conectado!");
  
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Sincronizando hora...");
  while (time(nullptr) < 8 * 3600 * 2) { }
  Serial.println("¡Hora lista!");
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

  // --- LÓGICA PRINCIPAL CORREGIDA ---
  
  if (distance > 0 && distance <= UMBRAL_DISTANCIA) {
    Serial.println("¡Objeto! Validando con Azure...");
    
    // 1. Señal de "Procesando" (Ambos apagados o parpadeo rápido)
    digitalWrite(ledRojoEsperando, LOW);
    digitalWrite(ledVerdeDetectado, LOW);

    // 2. Enviar a Azure y OBTENER RESPUESTA
    int respuestaAzure = enviarRegistroEntrada();

    // 3. TOMAR DECISIÓN SEGÚN RESPUESTA
    if (respuestaAzure == 200 || respuestaAzure == 201) {
        // --- CASO ÉXITO (Hay lugar) ---
        Serial.println("✅ ACCESO CONCEDIDO (Verde)");
        digitalWrite(ledVerdeDetectado, HIGH); // VERDE ON
        digitalWrite(ledVerdeDetectado, LOW);
    }
    else {
        // --- CASO FALLO (Lleno o Error) ---
        Serial.println("❌ ACCESO DENEGADO / LLENO (Rojo)");
        // Parpadear Rojo 5 veces rápido para indicar "LLENO"
        for(int i=0; i<5; i++){
            digitalWrite(ledRojoEsperando, HIGH);
            digitalWrite(ledRojoEsperando, LOW);
        }
        // NO prendemos el verde, NO esperamos 5 segundos largos.
        // Regresamos a detectar rápido.
    }
    
    // Volver a estado base
    digitalWrite(ledRojoEsperando, HIGH); 
    
  }
  else {
    // Nada detectado
    digitalWrite(ledRojoEsperando, HIGH);
    digitalWrite(ledVerdeDetectado, LOW);
  }
}

// --- AHORA ESTA FUNCIÓN REGRESA UN 'int' (El código de error) ---
int enviarRegistroEntrada() {
  if (WiFi.status() != WL_CONNECTED) return -1;
    
  struct timeval tv;
  gettimeofday(&tv, nullptr); 
  char timestampBase[40];
  strftime(timestampBase, sizeof(timestampBase), "%Y-%m-%dT%H:%M:%S", localtime(&tv.tv_sec));
  char timestampFinal[50];
  sprintf(timestampFinal, "%s.%03ld", timestampBase, tv.tv_usec / 1000);

  String payload = "{";
  payload += "\"id_entrada_Pk\": 0,";
  payload += "\"fecha_Hora_Entrada\": \"";
  payload += timestampFinal;
  payload += "\"}";

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setTimeout(15000); 

  int httpCode = -1; // Valor por defecto si falla conexión

  if (http.begin(client, apiURL)) {
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(payload); // Guardamos el código (200, 400, etc)
    Serial.print("Respuesta Azure: ");
    Serial.println(httpCode);
    http.end();
  }
  
  return httpCode; // <-- Regresamos el código al loop para que decida los LEDs
}
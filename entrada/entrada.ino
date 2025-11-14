#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <time.h>       // Para obtener la hora
#include <sys/time.h>   // Para obtener los milisegundos

// --- 1. Configuración WiFi (Tus datos) ---
const char* ssid = "Tec-IoT";
const char* password = "spotless.magnetic.bridge";


// --- 2. Configuración API (¡AQUÍ CAMBIA LA IP!) ---
const char* IP_DE_TU_COMPUTADORA = "10.22.195.99"; // <--- ¡REEMPLAZA ESTO! (ej: "192.168.1.100")
// (Nota: Asumí que la URL no tiene espacio: "EntradaRegistro")
const char* apiURL_plantilla = "http://%s:5074/Sensores/EntradaRegistro"; 

// --- 3. Configuración Sensor ---
const int trigPin = D1; // Pin Trig
const int echoPin = D2; // Pin Echo
const int UMBRAL_DISTANCIA = 2; // Umbral de 2 cm

// --- Configuración de Hora (NTP) ---
const char* ntpServer = "pool.ntp.org";

void setup() {
  Serial.begin(115200);

  // Configurar pines del sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // --- Conectar a WiFi ---
  delay(10); 
  Serial.println();
  Serial.println("--- Conectando a WiFi --- CODIGO ENTRADA REGISTRO ---");
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

  // --- Sincronizar la hora ---
  Serial.println("Sincronizando hora desde NTP...");
  configTime(0, 0, ntpServer); // Configura para UTC (hora 'Z')

  while (time(nullptr) < 8 * 3600 * 2) { // Espera a que la hora esté lista
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n¡Hora sincronizada!");
}

void loop() {
  // 1. Obtener la distancia
  long duration;
  int distance;

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  Serial.print("Distancia: ");
  Serial.print(distance);
  Serial.println(" cm");

  // 2. Verificar la condición
  // (distance > 0 es para filtrar lecturas erróneas)
  if (distance >= 0 && distance <= UMBRAL_DISTANCIA) {
    Serial.println("¡Objeto detectado! Registrando entrada...");
    
    // 3. Enviar el registro a la API
    enviarRegistroEntrada();
    
    // 4. Esperar 5 segundos para no saturar la base de datos
    Serial.println("Esperando 5 segundos antes de re-armar...");
    delay(5000);
  }

  delay(250); // Pequeña pausa entre mediciones
}

void enviarRegistroEntrada() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay WiFi para enviar el registro.");
    return;
  }
    
  // --- Crear el Timestamp EXACTO (con milisegundos) ---
  struct timeval tv;
  gettimeofday(&tv, nullptr); // Obtiene tiempo con microsegundos
  
  char timestampBase[40];
  // Formatea la parte YYYY-MM-DDTHH:MM:SS
  strftime(timestampBase, sizeof(timestampBase), "%Y-%m-%dT%H:%M:%S", gmtime(&tv.tv_sec));
  
  char timestampFinal[50];
  // Añade los milisegundos y la 'Z' (formato UTC)
  sprintf(timestampFinal, "%s.%03ldZ", timestampBase, tv.tv_usec / 1000);
  // --- Fin del Timestamp ---

  // Crear el payload JSON dinámicamente
  String payload = "{";
  payload += "\"id_entrada_Pk\": 0,";
  payload += "\"fecha_Hora_Entrada\": \"";
  payload += timestampFinal;
  payload += "\"}";

  Serial.print("Enviando JSON: ");
  Serial.println(payload);

  // Enviar el POST
  WiFiClient client;
  HTTPClient http;

  // Construir la URL completa con la IP de tu PC
  char urlCompleta[100];
  sprintf(urlCompleta, apiURL_plantilla, IP_DE_TU_COMPUTADORA);

  Serial.print("URL destino: ");
  Serial.println(urlCompleta);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.POST(payload);
    
    Serial.print("Código de respuesta HTTP: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK || httpCode == 201) { // 200 (OK) o 201 (Created)
      Serial.println("¡Entrada registrada en la BD con éxito!");
    } else {
      Serial.println("Error al registrar, el servidor respondió.");
    }
    
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- ¡CAMBIO: Para HTTPS (Azure)!
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <Servo.h>

// --- 1. Configuración WiFi ---
const char* ssid = "Tec-IoT";                    // <- WiFi anterior (comentado para documentación)
const char* password = "spotless.magnetic.bridge"; // <- WiFi anterior (comentado para documentación)

// --- 2. Configuración API (AZURE) ---
// Ponemos el dominio SIN "https://" y SIN "/" al final
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URLs actualizadas para HTTPS y sin puerto 5074
const char* apiURL_POST_Salida = "https://%s/Sensores/SalidaRegistro";

// --- 3. Configuración Pines ---
const int botonPin = D1;
const int ledVerdePin = D2; // Verde = Salida permitida
const int ledRojoPin = D4;  // Rojo = Sistema ocupado/esperando
const int servoPin = D7;    // Pin para el servo de la barrera

// --- 4. Variables del Sistema ---
int estadoBotonAnterior = HIGH;
bool sistemaOcupado = false; // Para evitar múltiples presiones simultáneas

// --- Configuración de Zona Horaria ---
const char* ntpServer = "pool.ntp.org";

// --- Objeto Servo ---
Servo barrera;

void setup() {
  Serial.begin(115200);

  // Configurar pines
  pinMode(botonPin, INPUT_PULLUP);
  pinMode(ledVerdePin, OUTPUT);
  pinMode(ledRojoPin, OUTPUT);

  // Configurar servo
  barrera.attach(servoPin);
  barrera.write(195); // Barrera arriba inicialmente

  // Estado inicial: LEDs apagados durante configuración
  digitalWrite(ledVerdePin, LOW);
  digitalWrite(ledRojoPin, LOW);

  // --- 1. Conectar a WiFi ---
  Serial.println();
  Serial.println("--- Conectando a WiFi (Sistema de Salida con Barrera) ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP8266: ");
  Serial.println(WiFi.localIP());

  // --- 2. Configurar zona horaria (sin sincronización NTP para acelerar arranque) ---
  configTime(-6 * 3600, 0, ntpServer); // GMT-6 (México)

  // --- 3. Sistema listo: Encender LED rojo ---
  digitalWrite(ledRojoPin, HIGH); // ← LED rojo se enciende SOLO al final

  Serial.println("Sistema de Salida con Barrera INICIADO.");
  Serial.println("Estado inicial: Esperando salida (LED Rojo, Barrera arriba)");
}


void loop() {
  // --- Chequear el botón para activar secuencia de salida ---
  chequearBoton();
}

// --- Esta función maneja la secuencia completa de salida ---
void chequearBoton() {
  int estadoBotonActual = digitalRead(botonPin);

  // Detectar "flanco de subida" y verificar que el sistema no esté ocupado
  if (estadoBotonActual == HIGH && estadoBotonAnterior == LOW && !sistemaOcupado) {
    Serial.println("\n¡Botón presionado! Iniciando secuencia de salida...");

    // Marcar sistema como ocupado para evitar múltiples presiones
    sistemaOcupado = true;

    // 1. PRIMERO: Cambiar a verde y bajar barrera (operación inmediata)
    Serial.println("Activando salida: LED Verde, Barrera abajo");
    digitalWrite(ledVerdePin, HIGH);
    digitalWrite(ledRojoPin, LOW);
    barrera.write(0); // Bajar barrera (ajusta el ángulo si es necesario)

    // 2. Mantener abierto por 5 segundos
    delay(5000);

    // 3. Subir barrera y volver a rojo
    Serial.println("Cerrando salida: LED Rojo, Barrera arriba");
    barrera.write(195); // Subir barrera
    delay(1000); // Pequeño delay para que suba completamente
    digitalWrite(ledVerdePin, LOW);
    digitalWrite(ledRojoPin, HIGH);

    // 4. DESPUÉS de completar la secuencia física: Registrar salida en la API
    Serial.println("Registrando salida en el sistema...");
    enviarRegistroSalida();

    // 5. Liberar sistema
    sistemaOcupado = false;
    Serial.println("Secuencia de salida completada. Sistema listo para nueva salida.");
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
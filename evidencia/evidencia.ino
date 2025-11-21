#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- PARA HTTPS (Azure)!
#include <NewPing.h> // Tu librería de sensor

// --- 1. Configuración WiFi (Nuestros datos) ---
// const char* ssid = "Tec-IoT";                    // <- WiFi anterior (comentado para documentación)
// const char* password = "spotless.magnetic.bridge"; // <- WiFi anterior (comentado para documentación)

const char* ssid = "IZZI-B790";         // <- WiFi actual
const char* password = "2C9569A8B790";  // <- WiFi actual

// --- 2. Configuración API (AZURE) ---
// Ponemos el dominio SIN "https://" y SIN "/" al final
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URL actualizada para HTTPS y sin puerto 5074
const char* apiURL_plantilla = "https://%s/Caja/CambiarEstado"; 

// --- 3. Hardware (Tu lógica nueva) ---
// Sensor (Tus pines)
#define TRIGGER_PIN D5 // (GPIO14)
#define ECHO_PIN D6    // (GPIO12)
#define MAX_DISTANCE 250

// LEDs
#define LED_VERDE D2 // LED para "Lleno"
#define LED_ROJO_1 D1 // LEDs para "Vacio" (molestar)

// Objeto del sensor (Tu código)
NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE);

// --- 4. Lógica de Estado (Tu lógica) ---
const float DistanciaActivar = 8.0; // 5 cm
bool estadoActualLleno = false; // El "cerebro" que recuerda el estado

void setup() {
  Serial.begin(115200);

  // Configurar pines de LEDs
  pinMode(LED_VERDE, OUTPUT);
  pinMode(LED_ROJO_1, OUTPUT);

  // CODIGO DE WIFI QUE HEMOS ESTADO USANDO
  delay(10); 
  Serial.println();
  Serial.println("Conectando a WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP8266: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Sensor de Caja INICIADO.");

  delay(50); 
  float distanciaInicial = sonar.ping_cm();
  if (distanciaInicial > 0 && distanciaInicial < DistanciaActivar) {
    estadoActualLleno = true;
  } else {
    estadoActualLleno = false;
  }
  
  Serial.print("Estado inicial detectado: ");
  Serial.println(estadoActualLleno ? "Lleno" : "Vacio");

}


void loop() {
  float distancia = sonar.ping_cm();

  // 2. Determinar estado actual
  // (Si la lectura es 0.0 (error) o > 8cm, está "Vacio")
  bool ahoraDebeEstarLleno = (distancia > 0 && distancia < DistanciaActivar);

  // 3. Imprimir log
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.print(" cm.  Estado: ");
  Serial.println(ahoraDebeEstarLleno ? "Lleno" : "Vacio");

  // 4. Lógica de "Máquina de Estados" (Solo actuar si hay CAMBIO)
  if (ahoraDebeEstarLleno != estadoActualLleno) {
    Serial.println("-------------------------------------");
    Serial.println(">>> ¡CAMBIO DE ESTADO DETECTADO! <<<");
    
    // Actualizar el "cerebro"
    estadoActualLleno = ahoraDebeEstarLleno; 
    
    // Enviar el POST a la API (CajaController)
    enviarEstadoAPI(estadoActualLleno);
    Serial.println("-------------------------------------");
  }

  // 5. Lógica de LEDs (Se ejecuta siempre)
  if (estadoActualLleno) {
    // Está Lleno: 1 LED verde prendido
    cajaLlenaLEDs();
  } else {
    // Está Vacío: 4 LEDs rojos parpadeando (molestar)
    cajaVaciaLEDs_parpadear();
  }
  
  // (Usamos el delay de tu código original)
  delay(500);
}


// --- Funciones de LEDs (Tu lógica nueva) ---

void cajaLlenaLEDs() {
  // Verde PRENDIDO
  digitalWrite(LED_VERDE, HIGH);
  // Rojos APAGADOS
  digitalWrite(LED_ROJO_1, LOW);
}

void cajaVaciaLEDs_parpadear() {
  // Verde APAGADO
  digitalWrite(LED_VERDE, LOW);

  // Rojos PRENDIDOS
  digitalWrite(LED_ROJO_1, HIGH);

  // Pequeña pausa (este delay SÍ es bloqueante,
  // pero como el loop se corre cada 500ms, está bien)
  delay(100);

  // Rojos APAGADOS
  digitalWrite(LED_ROJO_1, LOW);
}


// --- Función para enviar el POST a la API ---
void enviarEstadoAPI(bool estaLleno) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi.");
    return;
  }

  String estadoString;
  if (estaLleno) {
    estadoString = "Lleno";
  } else {
    estadoString = "Vacio";
  }

  String payload = "{";
  payload += "\"Estado\": \"";
  payload += estadoString;
  payload += "\"}";

  Serial.println(payload);

  // --- CAMBIO: Usar WiFiClientSecure para HTTPS ---
  WiFiClientSecure client;
  client.setInsecure(); // <--- IMPORTANTE: Confiar en el certificado de Azure

  HTTPClient http;

  // Construir la URL completa
  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_plantilla, IP_SERVIDOR);

  Serial.print("URL destino: ");
  Serial.println(urlCompleta);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST(payload);

    Serial.print("Código de respuesta HTTP: ");
    Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK || httpCode == 200 || httpCode == 201) {
      Serial.println("¡Estado de la caja registrado en Azure!");
    } else {
      Serial.println("Error al registrar en Azure.");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}
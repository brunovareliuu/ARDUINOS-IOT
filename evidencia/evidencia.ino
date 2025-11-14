#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <NewPing.h> // Tu librería de sensor

// --- 1. Configuración WiFi (Nuestros datos) ---
const char* ssid = "Tec-IoT";
const char* password = "spotless.magnetic.bridge";

// --- 2. Configuración API (¡NUEVA API!) ---
const char* IP_DE_TU_COMPUTADORA = "10.22.195.99"; // <--- Tu IP
// (Endpoint del NUEVO CajaController)
const char* apiURL_plantilla = "http://%s:5074/Caja/CambiarEstado"; 

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
  digitalWrite(LED_ROJO_2, HIGH);
  
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

  WiFiClient client;
  HTTPClient http;

  // 3. Construir la URL completa
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
      Serial.println("¡Estado de la caja registrado en la API!");
    } else {
      Serial.println("Error al registrar en la API.");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// --- 1. Configuración WiFi (Nuestros datos) ---
const char* ssid = "Tec-IoT";
const char* password = "spotless.magnetic.bridge";

// --- 2. Configuración API (Nuestra IP) ---
const char* IP_DE_TU_COMPUTADORA = "10.22.195.99"; // <--- Tu IP
const char* apiURL_plantilla = "http://%s:5074/Sensores/LuzNocturnaPost"; 

// --- 3. Configuración Sensor ---
const int pinSensorLuz = A0; // Pin Analógico A0

// --- 4. Configuración LEDs (Tus pines) ---
const int ledPrendidoPin = D6; // LED Verde (Oscuro = Focos Prendidos)
const int ledApagadoPin = D8;  // LED Rojo (Luz = Focos Apagados)

// --- 5. Lógica de Estado (¡LÓGICA VOLTEADA!) ---
// ***** CAMBIO REALIZADO (UMBRAL) *****
const int UMBRAL_LDR = 850; // <--- UMBRAL ACTUALIZADO
// ************************************

bool estadoActualPrendido = false; // El "cerebro" que recuerda el estado

void setup() {
  Serial.begin(115200);
  pinMode(pinSensorLuz, INPUT);
  pinMode(ledPrendidoPin, OUTPUT);
  pinMode(ledApagadoPin, OUTPUT);

  // --- Conectar a WiFi (Nuestra lógica) ---
  delay(10); 
  Serial.println();
  Serial.println("--- Conectando a WiFi ---");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡WiFi conectado!");
  Serial.print("IP del ESP8266: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("Sensor de Luz (LDR) INICIADO.");
  
  // Detectar estado inicial
  estadoActualPrendido = (analogRead(pinSensorLuz) < UMBRAL_LDR); // Lógica volteada
  Serial.print("Estado inicial detectado: ");
  Serial.println(estadoActualPrendido ? "Prendido (Oscuro)" : "Apagado (Luz)");
  
  // Poner LEDs iniciales
  if (estadoActualPrendido) {
    digitalWrite(ledPrendidoPin, HIGH); // D6 ON
    digitalWrite(ledApagadoPin, LOW);
  } else {
    digitalWrite(ledPrendidoPin, LOW);
    digitalWrite(ledApagadoPin, HIGH); // D8 ON
  }
}

void loop() {
  // 1. Leer el valor del sensor (0-1024)
  int nivelLuzActual = analogRead(pinSensorLuz);
  
  // 2. Imprimir el log en vivo (Como pediste)
  Serial.print("Lectura en vivo -> Nivel_Luz: ");
  Serial.println(nivelLuzActual);

  // 3. Determinar si DEBERÍA estar prendido (Lógica volteada)
  bool ahoraDebeEstarPrendido = (nivelLuzActual < UMBRAL_LDR);

  // 4. Revisar si el estado CAMBIÓ
  if (ahoraDebeEstarPrendido != estadoActualPrendido) {
    // ¡SÍ CAMBIÓ!
    Serial.println("-------------------------------------");
    Serial.println(">>> ¡CAMBIO DE ESTADO DETECTADO! <<<"); 
    
    if (ahoraDebeEstarPrendido) {
      Serial.println("Se PRENDIÓ la luz (Está oscuro).");
      digitalWrite(ledPrendidoPin, HIGH); // D6 ON
      digitalWrite(ledApagadoPin, HIGH);
    } else {
      Serial.println("Se APAGÓ la luz (Hay luz).");
      digitalWrite(ledPrendidoPin, LOW);
      digitalWrite(ledApagadoPin, LOW); // D8 ON
    }
    
    // 5. Enviar el POST a la API
    enviarNivelLuz(nivelLuzActual);
    
    // 6. Actualizar el "cerebro"
    estadoActualPrendido = ahoraDebeEstarPrendido;
    Serial.println("-------------------------------------");
  }

  // Esperar 2 segundos antes de la siguiente LECTURA
  delay(2000); 
}


// --- Función para enviar los POST (Sin cambios) ---
void enviarNivelLuz(int nivel) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi.");
    return;
  }

  // Crear el payload JSON: {"Nivel_Luz": 10}
  String payload = "{";
  payload += "\"Nivel_Luz\": ";
  payload += String(nivel);
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
      Serial.println("¡Estado de luz registrado en la API!");
    } else {
      Serial.println("Error al registrar en la API.");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}
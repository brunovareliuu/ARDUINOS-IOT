#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- PARA HTTPS (Azure)!

// --- 1. CONFIGURACIÓN DEL CAJÓN (¡MUY IMPORTANTE!) ---
const int ID_DE_ESTE_CAJON = 3; // <--- CAMBIADO: Este es el Cajón 3

// --- 2. Configuración WiFi (Tus datos) ---
// const char* ssid = "Tec-IoT";                    // <- WiFi anterior (comentado para documentación)
// const char* password = "spotless.magnetic.bridge"; // <- WiFi anterior (comentado para documentación)

const char* ssid = "IZZI-B790";         // <- WiFi actual
const char* password = "2C9569A8B790";  // <- WiFi actual

// --- 3. Configuración API (AZURE) ---
// Ponemos el dominio SIN "https://" y SIN "/" al final
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URLs actualizadas para HTTPS y sin puerto 5074
const char* apiURL_Ocupar = "https://%s/Sensores/OcuparCajon";
const char* apiURL_Liberar = "https://%s/Sensores/LiberarCajon";

// --- 4. Configuración Sensor Ultrasónico ---
const int trigPin = D1; // Pin Trig
const int echoPin = D2; // Pin Echo
const int UMBRAL_DISTANCIA = 4; // Umbral de 4 cm

// --- 4.B. Configuración de LEDs (Tu lógica) ---
const int ledVerdePin = D3;
const int ledRojoPin = D4;

// --- 5. Lógica de Tiempos (Tu lógica) ---
const unsigned long tiempoParaOcupar = 10000; // 10 segundos (en ms)
const unsigned long tiempoParaLiberar = 20000; // 20 segundos (en ms)

// --- 6. Variables de Estado (El "cerebro") ---
bool estaOcupado = false; // El estado actual que recordamos
unsigned long tiempoDeCambio = 0; // El temporizador (usa millis())

void setup() {
  Serial.begin(115200);

  // Configurar pines del sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Configurar pines de LEDs
  pinMode(ledVerdePin, OUTPUT);
  pinMode(ledRojoPin, OUTPUT);

  // --- Conectar a WiFi ---
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

  Serial.printf("Sensor del Cajón #%d INICIADO.\n", ID_DE_ESTE_CAJON);

  // Poner estado inicial de LEDs (LIBRE)
  digitalWrite(ledVerdePin, HIGH);
  digitalWrite(ledRojoPin, LOW);
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

  // --- IMPRIMIR VALOR CADA CICLO ---
  Serial.print("Distancia: ");
  Serial.print(distance);
  Serial.println(" cm");
  // --- FIN DE LA IMPRESIÓN ---

  // Filtro de ruido (ignora lecturas > 200cm o < 0)
  if (distance < 0 || distance > 200) {
    return; // Lectura errónea, saltar este ciclo
  }

  // --- 2. LA MÁQUINA DE ESTADOS (Tu lógica) ---

  unsigned long tiempoActual = millis(); // El reloj actual

  if (estaOcupado == false)
  {
    // MODO: "LIBRE" (Buscando un coche que LLEGUE)
    if (distance <= UMBRAL_DISTANCIA) {
      // Objeto detectado.
      if (tiempoDeCambio == 0) {
        // Es la primera vez que lo vemos, iniciar timer
        Serial.printf("Objeto detectado... iniciando timer de %lu seg.\n", tiempoParaOcupar / 1000);
        tiempoDeCambio = tiempoActual;
      }
      else if (tiempoActual - tiempoDeCambio > tiempoParaOcupar) {
        // El objeto SIGUE AHÍ después de 10 segundos.
        Serial.println("¡CAJÓN OCUPADO!");

        // --- Encender LED Rojo ---
        digitalWrite(ledVerdePin, LOW);
        digitalWrite(ledRojoPin, HIGH);

        llamarAPI(apiURL_Ocupar); // Llamar al POST /OcuparCajon
        estaOcupado = true;      // Cambiar de modo
        tiempoDeCambio = 0;      // Resetear timer
      }
    }
    else {
      // No hay objeto. Si había una falsa alarma, resetear el timer.
      if (tiempoDeCambio > 0) {
         Serial.println("Falsa alarma. Objeto se fue antes de tiempo.");
      }
      tiempoDeCambio = 0;
    }
  }
  else
  {
    // MODO: "OCUPADO" (Buscando un coche que SE VAYA)
    if (distance > UMBRAL_DISTANCIA) {
      // Objeto ya no está.
      if (tiempoDeCambio == 0) {
        // Es la primera vez que no lo vemos, iniciar timer
        Serial.printf("Objeto no detectado... iniciando timer de %lu seg.\n", tiempoParaLiberar / 1000);
        tiempoDeCambio = tiempoActual;
      }
      else if (tiempoActual - tiempoDeCambio > tiempoParaLiberar) {
        // El objeto SIGUE SIN ESTAR después de 20 segundos.
        Serial.println("¡CAJÓN LIBERADO!");

        // --- Encender LED Verde ---
        digitalWrite(ledVerdePin, HIGH);
        digitalWrite(ledRojoPin, LOW);

        llamarAPI(apiURL_Liberar); // Llamar al POST /LiberarCajon
        estaOcupado = false;     // Cambiar de modo
        tiempoDeCambio = 0;      // Resetear timer
      }
    }
    else {
      // El objeto sigue ahí. Resetear el timer de liberación.
      if (tiempoDeCambio > 0) {
         Serial.println("El coche sigue ahí. Reseteando timer de liberación.");
      }
      tiempoDeCambio = 0;
    }
  }

  // Pausa de 2 segundos entre mediciones
  delay(2000);
}


// --- 3. Función para enviar los POST ---
void llamarAPI(const char* apiURL_plantilla) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi.");
    return;
  }

  // Crear el payload JSON: {"Id_Cajon": 3}
  String payload = "{";
  payload += "\"Id_Cajon\": ";
  payload += String(ID_DE_ESTE_CAJON);
  payload += "}";

  Serial.print("Enviando JSON: ");
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
      Serial.println("¡Registro de cajón exitoso en Azure!");
    } else {
      Serial.println("Error al registrar en Azure.");
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP.");
  }
}

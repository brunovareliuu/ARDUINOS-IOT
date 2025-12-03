#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- PARA HTTPS (Azure)!

// --- 1. CONFIGURACIÓN DEL CAJÓN (¡MUY IMPORTANTE!) ---
const int ID_DE_ESTE_CAJON = 1; // <--- Si este es el Cajón 1, pon 1. Si es el 2, pon 2.

// --- 2. Configuración WiFi (Tus datos) ---
const char* ssid = "Tec-IoT";                    // <- WiFi anterior (comentado para documentación)
const char* password = "spotless.magnetic.bridge"; // <- WiFi anterior (comentado para documentación)

// --- 3. Configuración API (AZURE) ---
// Ponemos el dominio SIN "https://" y SIN "/" al final
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URLs actualizadas para HTTPS y sin puerto 5074
const char* apiURL_Ocupar = "https://%s/Sensores/OcuparCajon";
const char* apiURL_Liberar = "https://%s/Sensores/LiberarCajon"; 

// --- 4. Configuración Sensor Ultrasónico ---
const int trigPin = D1; // Pin Trig
const int echoPin = D2; // Pin Echo
const int UMBRAL_DISTANCIA = 7;

// --- 4.B. Configuración de LEDs (Tu lógica) ---
const int ledVerdePin = D5; 
const int ledRojoPin = D3;


// --- 5. Lógica de Debounce (Tu nueva lógica) ---
const int LECTURAS_OCUPAR = 2;   // 2 lecturas para ocupar (LIBRE → OCUPADO)
const int LECTURAS_LIBERAR = 3;  // 5 lecturas para liberar (OCUPADO → LIBRE)

// --- 6. Variables de Estado (El "cerebro") ---
bool estaOcupado = false; // El estado actual que recordamos
int contadorOcupar = 0;    // Cuenta lecturas consecutivas para ocupar
int contadorLiberar = 0;   // Cuenta lecturas consecutivas para liberar

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

  // --- ACTUALIZAR LEDs SEGÚN ESTADO ACTUAL ---
  // Esto asegura que los LEDs siempre reflejen el estado correcto
  if (estaOcupado) {
    // OCUPADO: LED Rojo
    digitalWrite(ledVerdePin, LOW);
    digitalWrite(ledRojoPin, HIGH);
  } else {
    // LIBRE: LED Verde
    digitalWrite(ledVerdePin, HIGH);
    digitalWrite(ledRojoPin, LOW);
  }

  // --- 2. SISTEMA DE DEBOUNCE (Tu nueva lógica) ---

  // Mostrar contadores actuales
  Serial.printf("Contadores - Ocupar: %d/%d, Liberar: %d/%d\n",
                contadorOcupar, LECTURAS_OCUPAR,
                contadorLiberar, LECTURAS_LIBERAR);

  if (estaOcupado == false)
  {
    // MODO: "LIBRE" (Buscando un coche que LLEGUE)
    if (distance <= UMBRAL_DISTANCIA) {
      // Objeto detectado - incrementar contador para ocupar
      contadorOcupar++;
      contadorLiberar = 0; // Resetear contador opuesto

      Serial.printf("Objeto detectado (%d cm). Contador ocupar: %d/%d\n",
                   distance, contadorOcupar, LECTURAS_OCUPAR);

      // Si llega a 2 lecturas consecutivas, ocupar el cajón
      if (contadorOcupar >= LECTURAS_OCUPAR) {
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║  🚗 ¡CAJÓN OCUPADO! (10/10)          ║");
        Serial.println("╚══════════════════════════════════════╝");

        // --- Encender LED Rojo ---
        digitalWrite(ledVerdePin, LOW);
        digitalWrite(ledRojoPin, HIGH);

        llamarAPI(apiURL_Ocupar); // Llamar al POST /OcuparCajon
        estaOcupado = true;      // Cambiar de modo
        contadorOcupar = 0;      // Resetear contador
      }
    }
    else {
      // No hay objeto - resetear contador de ocupar
      if (contadorOcupar > 0) {
         Serial.printf("Falsa alarma. Objeto se fue (%d cm). Reseteando contador.\n", distance);
         contadorOcupar = 0;
      }
    }
  }
  else
  {
    // MODO: "OCUPADO" (Buscando un coche que SE VAYA)
    if (distance > UMBRAL_DISTANCIA) {
      // Objeto ya no está - incrementar contador para liberar
      contadorLiberar++;
      contadorOcupar = 0; // Resetear contador opuesto

      Serial.printf("Objeto no detectado (%d cm). Contador liberar: %d/%d\n",
                   distance, contadorLiberar, LECTURAS_LIBERAR);

      // Si llega a 5 lecturas consecutivas, liberar el cajón
      if (contadorLiberar >= LECTURAS_LIBERAR) {
        Serial.println("╔══════════════════════════════════════╗");
        Serial.println("║  ✅ ¡CAJÓN LIBERADO! (10/10)         ║");
        Serial.println("╚══════════════════════════════════════╝");

        // --- Encender LED Verde ---
        digitalWrite(ledVerdePin, HIGH);
        digitalWrite(ledRojoPin, LOW);

        llamarAPI(apiURL_Liberar); // Llamar al POST /LiberarCajon
        estaOcupado = false;     // Cambiar de modo
        contadorLiberar = 0;     // Resetear contador
      }
    }
    else {
      // El objeto sigue ahí - resetear contador de liberar
      if (contadorLiberar > 0) {
         Serial.printf("El coche sigue ahí (%d cm). Reseteando contador de liberación.\n", distance);
         contadorLiberar = 0;
      }
    }
  }

  // Pausa de 2 segundos entre mediciones
  delay(2000); // <--- CAMBIO
}


// --- 3. Función para enviar los POST ---
void llamarAPI(const char* apiURL_plantilla) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST: No hay WiFi.");
    return;
  }

  // Crear el payload JSON: {"Id_Cajon": 1}
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
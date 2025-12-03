#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> // <--- PARA HTTPS (Azure)!

// Librerías para el sensor de temperatura
#include "DHT.h"
#include "Adafruit_Sensor.h"

// --- 1. Configuración WiFi ---
const char* ssid = "Tec-IoT";                    
const char* password = "spotless.magnetic.bridge"; 

// --- 2. Configuración API (AZURE) ---
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";

// URLs para ambos sensores
const char* apiURL_Temperatura = "https://%s/Sensores/TemperaturaEstacionamiento";
const char* apiURL_Luz         = "https://%s/Sensores/LuzNocturnaPost";

// --- 3. Configuración Sensor Temperatura ---
// DATA del DHT11 en D2 -> GPIO4
const int DHTPIN = 4;
// LED de temperatura en D4 -> GPIO2
const int LED_TEMP_PIN = D4;
// LED de API (parpadea cuando envía datos) en D1 -> GPIO5
const int LED_API_PIN = D1;
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
// --- 4. Configuración  Luz (fotoresistor) ---
const int pinSensorLuz = A0;

// LEDs de luz nocturna (usar GPIO directos)
const int ledOscuroPin = 12; // GPIO12 (D6) - se enciende cuando está oscuro
const int ledClaroPin  = 15; // GPIO15 (D8) - se enciende cuando hay luz
const int ledExtra1    = 14; // GPIO14 (D5)
const int ledExtra2    = 13; // GPIO13 (D7)

// --- 5. Lógica de Estado ---
// Fotoresistor en ESP8266: 0 (muy oscuro) a 1024 (muy claro)
// ⚠️ IMPORTANTE: Este umbral debe coincidir con el de la API (850)
// API: Nivel_Luz < 850 = "Prendido", Nivel_Luz >= 850 = "Apagado"
// Valores < 850 = OSCURO (PRENDIDO), Valores >= 850 = CLARO (APAGADO)
const int UMBRAL_LDR = 105;
bool estadoActualOscuro = false;

// --- 5.B. Lógica de Debounce para sensor de luz (como en los cajones) ---
const int LECTURAS_OSCURO = 2;   // 2 lecturas para cambiar a oscuro (LUZ → OSCURO)
const int LECTURAS_CLARO = 3;    // 3 lecturas para cambiar a claro (OSCURO → LUZ)
int contadorOscuro = 0;           // Cuenta lecturas consecutivas para oscuro
int contadorClaro = 0;            // Cuenta lecturas consecutivas para claro

// --- 6. Intervalos ---
const int intervaloLectura   = 2000;   // leer sensores cada 2 seg
const int intervaloTempPOST  = 30000;  // enviar temp cada 30 seg

// --- 7. Timers ---
unsigned long tiempoAnteriorTempPOST = 0;
float ultimaTempValida = 0;

void setup() {
  Serial.begin(115200);

  // Inicializar sensor de temperatura
  dht.begin();
  Serial.println("DHT11 inicializado en pin D2 (GPIO4)");
  pinMode(LED_TEMP_PIN, OUTPUT);
  digitalWrite(LED_TEMP_PIN, LOW);
  Serial.println("LED de temperatura configurado en pin D4 (GPIO2)");
  
  // Configurar LED de API
  pinMode(LED_API_PIN, OUTPUT);
  digitalWrite(LED_API_PIN, LOW);
  Serial.println("LED de API configurado en pin D1 (GPIO5)");

  // Configurar pines del sensor de luz
  pinMode(pinSensorLuz, INPUT);
  pinMode(ledOscuroPin, OUTPUT);
  pinMode(ledClaroPin, OUTPUT);
  pinMode(ledExtra1, OUTPUT);
  pinMode(ledExtra2, OUTPUT);

  digitalWrite(ledOscuroPin, LOW);
  digitalWrite(ledClaroPin, LOW);
  digitalWrite(ledExtra1, LOW);
  digitalWrite(ledExtra2, LOW);

  // --- Conectar a WiFi ---
  delay(10);
  Serial.println();
  Serial.println("--- Conectando a WiFi (Sensores Combinados) ---");
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
  Serial.println("Sistema de Sensores de Ambiente INICIADO.");

  // Prueba inicial del DHT11
  Serial.println("Probando DHT11...");
  delay(2000); // Esperar a que se estabilice
  float tempTest = dht.readTemperature();
  float humTest = dht.readHumidity();

  if (!isnan(tempTest) && !isnan(humTest)) {
    Serial.print("✅ DHT11 OK - Temp: ");
    Serial.print(tempTest);
    Serial.print("°C, Hum: ");
    Serial.print(humTest);
    Serial.println("%");
  } else {
    Serial.println("❌ DHT11 ERROR en prueba inicial");
    Serial.println("REVISAR: Conexión física, alimentación, sensor dañado");
  }

  // ⚠️ IMPORTANTE: Estado inicial SIEMPRE es APAGADO (LEDs apagados)
  // El estacionamiento normalmente tiene luz al inicio
  // Esto asegura que el primer cambio real (a PRENDIDO) se detecte correctamente
  int lecturaInicial = analogRead(pinSensorLuz);
  estadoActualOscuro = false; // ← FORZAR estado CLARO (LEDs apagados)
  
  Serial.print("Estado inicial FORZADO: LEDs APAGADOS (CLARO) | Lectura sensor: ");
  Serial.println(lecturaInicial);

  // Poner LEDs apagados
  actualizarLEDsLuz(false); // false = LEDs APAGADOS

  // Enviar estado inicial a la API
  // ⚠️ CRÍTICO: Enviar valor MUY ALTO para que API registre como "Apagado"
  // LEDs APAGADOS = Hay luz = Valor alto del fotoresistor
  Serial.println("╔══════════════════════════════════════╗");
  Serial.println("║  📡 Estado INICIAL: LEDs APAGADOS   ║");
  Serial.println("╚══════════════════════════════════════╝");
  delay(1000); // Esperar 1 seg para estabilizar WiFi
  enviarNivelLuz(1000); // Valor MUY ALTO = LEDs APAGADOS (hay luz)
}

void loop() {
  unsigned long tiempoActual = millis();

  // --- 1. Leer sensores cada 2 seg ---
  leerSensores();

  // --- 2. Enviar temperatura cada 30 seg ---
  if (tiempoActual - tiempoAnteriorTempPOST >= (unsigned long)intervaloTempPOST) {
    Serial.println("\n--- Han pasado 30 seg ---");
    Serial.println("Enviando temperatura a la API...");
    enviarTemperatura(ultimaTempValida);
    tiempoAnteriorTempPOST = tiempoActual;
  }

  delay(intervaloLectura);
}

// ==========================================================
// FUNCIÓN PARA LEER SENSORES
// ==========================================================
void leerSensores() {
  // Leer DHT11 (temperatura y humedad)
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // Debug: mostrar valores crudos antes de validar
  Serial.print("DEBUG - Temp: ");
  Serial.print(t);
  Serial.print(" Hum: ");
  Serial.print(h);
  Serial.print(" | ");

  if (!isnan(t) && !isnan(h)) {
    Serial.print("Temp: ");
    Serial.print(t);
    Serial.print(" °C  | Humedad: ");
    Serial.print(h);
    Serial.print(" %");

    ultimaTempValida = t;

    // Condición del LED de temperatura (rojo: >=15 °C)
    if (t >= 23) digitalWrite(LED_TEMP_PIN, HIGH);
    else         digitalWrite(LED_TEMP_PIN, LOW);
  } else {
    Serial.print("Error leyendo DHT11");
  }

  // Leer luz (fotoresistor en A0)
  int nivelLuz = analogRead(pinSensorLuz);
  Serial.print(" | Nivel_Luz: ");
  Serial.println(nivelLuz);

  bool ahoraEstaOscuro = (nivelLuz < UMBRAL_LDR);

  // --- SISTEMA DE DEBOUNCE PARA SENSOR DE LUZ (como en los cajones) ---

  // Mostrar contadores actuales y estado
  Serial.printf("Estado: %s | Contadores - Oscuro: %d/%d, Claro: %d/%d\n",
                estadoActualOscuro ? "OSCURO(PRENDIDO)" : "CLARO(APAGADO)",
                contadorOscuro, LECTURAS_OSCURO,
                contadorClaro, LECTURAS_CLARO);

  if (estadoActualOscuro == false)
  {
    // MODO: "CLARO" (Buscando oscurecer)
    if (ahoraEstaOscuro) {
      // Detectado oscuro - incrementar contador para oscurecer
      contadorOscuro++;
      contadorClaro = 0; // Resetear contador opuesto

      Serial.printf("Detectado OSCURO (%d). Contador oscurecer: %d/%d\n",
                   nivelLuz, contadorOscuro, LECTURAS_OSCURO);

      // Si llega a 2 lecturas consecutivas, cambiar a oscuro
      if (contadorOscuro >= LECTURAS_OSCURO) {
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println("║  🌙 ¡LUZ CAMBIÓ A OSCURO! (PRENDIDO)║");
        Serial.println("╚══════════════════════════════════════╝");

        // ✅ PRIMERO: Actualizar estado y LEDs INMEDIATAMENTE
        estadoActualOscuro = true;
        actualizarLEDsLuz(true);

        // ✅ DESPUÉS: Enviar a API
        Serial.println("📡 Enviando cambio a OSCURO (PRENDIDO) a API...");
        enviarNivelLuz(nivelLuz);

        contadorOscuro = 0; // Resetear contador
        Serial.println("✅ Cambio completado\n");
      }
    }
    else {
      // Sigue claro - resetear contador de oscurecer
      if (contadorOscuro > 0) {
         Serial.printf("Sigue CLARO (%d). Reseteando contador de oscurecer.\n", nivelLuz);
         contadorOscuro = 0;
      }
    }
  }
  else
  {
    // MODO: "OSCURO" (Buscando aclarar)
    if (!ahoraEstaOscuro) {
      // Detectado claro - incrementar contador para aclarar
      contadorClaro++;
      contadorOscuro = 0; // Resetear contador opuesto

      Serial.printf("Detectado CLARO (%d). Contador aclarar: %d/%d\n",
                   nivelLuz, contadorClaro, LECTURAS_CLARO);

      // Si llega a 3 lecturas consecutivas, cambiar a claro
      if (contadorClaro >= LECTURAS_CLARO) {
        Serial.println("\n╔══════════════════════════════════════╗");
        Serial.println("║  ☀️ ¡LUZ CAMBIÓ A CLARO! (APAGADO)  ║");
        Serial.println("╚══════════════════════════════════════╝");

        // ✅ PRIMERO: Actualizar estado y LEDs INMEDIATAMENTE
        estadoActualOscuro = false;
        actualizarLEDsLuz(false);

        // ✅ DESPUÉS: Enviar a API
        Serial.println("📡 Enviando cambio a CLARO (APAGADO) a API...");
        enviarNivelLuz(nivelLuz);

        contadorClaro = 0; // Resetear contador
        Serial.println("✅ Cambio completado\n");
      }
    }
    else {
      // Sigue oscuro - resetear contador de aclarar
      if (contadorClaro > 0) {
         Serial.printf("Sigue OSCURO (%d). Reseteando contador de aclarar.\n", nivelLuz);
         contadorClaro = 0;
      }
    }
  }
}

// ==========================================================
// FUNCIÓN PARA ACTUALIZAR LEDS DE LUZ (D5, D6, D7, D8)
// ==========================================================
void actualizarLEDsLuz(bool esOscuro) {
  if (esOscuro) {
    // Oscuro -> encender TODOS los LEDs de luz
    digitalWrite(ledOscuroPin, HIGH); // GPIO12 (D6) - ON
    digitalWrite(ledClaroPin, HIGH);  // GPIO15 (D8) - ON
    digitalWrite(ledExtra1, HIGH);    // GPIO14 (D5) - ON
    digitalWrite(ledExtra2, HIGH);    // GPIO13 (D7) - ON
  } else {
    // Claro -> apagar TODOS los LEDs de luz
    digitalWrite(ledOscuroPin, LOW);  // GPIO12 (D6) - OFF
    digitalWrite(ledClaroPin, LOW);   // GPIO15 (D8) - OFF
    digitalWrite(ledExtra1, LOW);     // GPIO14 (D5) - OFF
    digitalWrite(ledExtra2, LOW);     // GPIO13 (D7) - OFF
  }
}

// ==========================================================
// FUNCIÓN PARA POST TEMPERATURA
// ==========================================================
void enviarTemperatura(float temp) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST Temperatura: No hay WiFi.");
    return;
  }

  if (temp == 0) {
    Serial.println("POST Temperatura: Temperatura inválida, no se enviará.");
    return;
  }

  // ✅ INICIAR PARPADEO DEL LED DE API
  digitalWrite(LED_API_PIN, HIGH);
  Serial.println("📡 Enviando a API... (LED API ON)");

  String payload = "{ \"Temperatura\": " + String(temp) + " }";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_Temperatura, IP_SERVIDOR);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    
    // Parpadeo durante el envío
    digitalWrite(LED_API_PIN, LOW);
    delay(100);
    digitalWrite(LED_API_PIN, HIGH);
    
    int httpCode = http.POST(payload);

    Serial.print("Código respuesta Temperatura: ");
    Serial.println(httpCode);

    if (httpCode == 200 || httpCode == 201) {
      Serial.println("¡Temperatura registrada en Azure!");
      // Parpadeo de éxito (3 veces rápido)
      for(int i = 0; i < 3; i++) {
        digitalWrite(LED_API_PIN, LOW);
        delay(100);
        digitalWrite(LED_API_PIN, HIGH);
        delay(100);
      }
    } else {
      // Parpadeo de error (lento)
      for(int i = 0; i < 2; i++) {
        digitalWrite(LED_API_PIN, LOW);
        delay(300);
        digitalWrite(LED_API_PIN, HIGH);
        delay(300);
      }
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP (Temperatura).");
  }
  
  // ✅ APAGAR LED DE API
  digitalWrite(LED_API_PIN, LOW);
  Serial.println("📡 Envío completado (LED API OFF)");
}

// ==========================================================
// FUNCIÓN PARA POST LUZ
// ==========================================================
void enviarNivelLuz(int nivel) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("POST Luz: No hay WiFi.");
    return;
  }

  // ✅ INICIAR PARPADEO DEL LED DE API
  digitalWrite(LED_API_PIN, HIGH);
  Serial.println("📡 Enviando a API... (LED API ON)");

  String payload = "{ \"Nivel_Luz\": " + String(nivel) + " }";

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;

  char urlCompleta[150];
  sprintf(urlCompleta, apiURL_Luz, IP_SERVIDOR);

  if (http.begin(client, urlCompleta)) {
    http.addHeader("Content-Type", "application/json");
    
    // Parpadeo durante el envío
    digitalWrite(LED_API_PIN, LOW);
    delay(100);
    digitalWrite(LED_API_PIN, HIGH);
    
    int httpCode = http.POST(payload);

    Serial.print("Código respuesta Luz: ");
    Serial.println(httpCode);

    if (httpCode == 200 || httpCode == 201) {
      Serial.println("¡Estado de luz registrado en Azure!");
      // Parpadeo de éxito (3 veces rápido)
      for(int i = 0; i < 3; i++) {
        digitalWrite(LED_API_PIN, LOW);
        delay(100);
        digitalWrite(LED_API_PIN, HIGH);
        delay(100);
      }
    } else {
      // Parpadeo de error (lento)
      for(int i = 0; i < 2; i++) {
        digitalWrite(LED_API_PIN, LOW);
        delay(300);
        digitalWrite(LED_API_PIN, HIGH);
        delay(300);
      }
    }
    http.end();
  } else {
    Serial.println("Error al iniciar conexión HTTP(Luz).");
  }
  
  // ✅ APAGAR LED DE API
  digitalWrite(LED_API_PIN, LOW);
  Serial.println("📡 Envío completado (LED API OFF)");
}
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Servo.h>

// --- 1. Configuración WiFi ---
const char* ssid = "IZZI-B790";
const char* password = "2C9569A8B790";

// --- 2. AZURE API ---
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net";
String apiURL_ENTRADAS = "https://" + String(IP_SERVIDOR) + "/Sensores/Entradas";
String apiURL_DISPONIBILIDAD = "https://" + String(IP_SERVIDOR) + "/Sensores/Disponibilidad";

// --- 3. HARDWARE ---
// SERVO
const int pinServo = D8;
Servo barrera;

// --- 4. VARIABLES DE CONTROL ---
int ultimoIdEntrada = -1;  // Para rastrear el último ID procesado
bool entradaActiva = false; // Si hay una entrada esperando ser procesada
int totalEntradas = 0;     // Total de entradas registradas
int totalSalidas = 0;      // Total de salidas registradas
unsigned long ultimoPolling = 0;
const unsigned long INTERVALO_POLLING = 500; // 500ms - más responsive pero no sobrecarga
const unsigned long TIEMPO_ABIERTA = 2000;    // 2 segundos cerrada después de nueva entrada
unsigned long tiempoEntradaDetectada = 0;

void setup() {
  Serial.begin(115200);

  // Configurar servo
  barrera.attach(pinServo);
  barrera.write(0); // Inicia cerrado

  Serial.println("\n--- CONTROLADOR SERVO: POLLING DE ENTRADAS (2s cerrada) ---");

  // Conectar WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado a WiFi!");
}

void loop() {
  if (millis() - ultimoPolling >= INTERVALO_POLLING) {
    ultimoPolling = millis();

    if (WiFi.status() == WL_CONNECTED) {
      verificarDisponibilidad();
      verificarNuevasEntradas();
    } else {
      Serial.println("❌ WiFi desconectado");
    }
  }

  // Control del servo basado en estado
  controlarBarrera();

  delay(100);
}

// --- FUNCIONES AUXILIARES ---
void verificarDisponibilidad() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  if (http.begin(client, apiURL_DISPONIBILIDAD)) {
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();

      // Parsear JSON
      StaticJsonDocument<96> doc;
      deserializeJson(doc, payload);

      int entradas = doc["totalEntradas"] | 0;
      int salidas = doc["totalSalidas"] | 0;

      totalEntradas = entradas;
      totalSalidas = salidas;

      // Calcular entradas activas (entradas - salidas)
      int entradasActivas = totalEntradas - totalSalidas;

      Serial.printf("📊 Estado: Entradas=%d, Salidas=%d, Activas=%d\n",
                   totalEntradas, totalSalidas, entradasActivas);

    } else {
      Serial.printf("❌ Error disponibilidad: %d\n", httpCode);
    }

    http.end();
  }
}

void verificarNuevasEntradas() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  // Solo mostrar logs cada 5 consultas para no saturar
  static int contadorLogs = 0;
  contadorLogs++;
  if (contadorLogs % 5 == 1) {
    Serial.print("Consultando entradas... ");
  }

  if (http.begin(client, apiURL_ENTRADAS)) {
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      if (contadorLogs % 5 == 1) {
        Serial.println("✅ OK");
      }

      // Parsear JSON
      DynamicJsonDocument doc(4096); // Buffer para array de entradas
      DeserializationError error = deserializeJson(doc, payload);

      if (error) {
        Serial.print("❌ Error parseando JSON: ");
        Serial.println(error.c_str());
        return;
      }

      // El endpoint devuelve un array, el primer elemento es el más reciente
      if (doc.size() > 0) {
        int idMasReciente = doc[0]["id_entrada_Pk"];

        Serial.printf("Último ID en BD: %d\n", idMasReciente);
        Serial.printf("Último ID procesado: %d\n", ultimoIdEntrada);

        // Si es un ID nuevo (diferente al último procesado)
        if (idMasReciente != ultimoIdEntrada && ultimoIdEntrada != -1) {
          Serial.printf("🚗 ¡NUEVA ENTRADA DETECTADA! ID: %d\n", idMasReciente);
          // Activar modo entrada por tiempo determinado
          entradaActiva = true;
          tiempoEntradaDetectada = millis();
          ultimoIdEntrada = idMasReciente;
        } else if (ultimoIdEntrada == -1) {
          // Primera vez, solo guardar el ID sin activar
          Serial.printf("📝 Inicializando con ID: %d\n", idMasReciente);
          ultimoIdEntrada = idMasReciente;
        } else {
          Serial.println("📭 Sin cambios");
        }
      } else {
        Serial.println("⚠️ No hay registros de entrada");
      }

    } else {
      Serial.printf("❌ Error HTTP: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("❌ Error conectando a API");
  }
}

void controlarBarrera() {
  // Si hay entrada activa y no ha pasado el tiempo, mantener cerrada (0°)
  if (entradaActiva && (millis() - tiempoEntradaDetectada) < TIEMPO_ABIERTA) {
    // Mantener en 0° mientras hay entrada activa
    barrera.write(0);
    return;
  }

  // Si el tiempo ya pasó, desactivar entrada activa
  if (entradaActiva && (millis() - tiempoEntradaDetectada) >= TIEMPO_ABIERTA) {
    Serial.println("⏱️ Tiempo de entrada terminado (2s) - abriendo barrera");
    entradaActiva = false;
  }

  // Estado normal: barrera abierta (195°)
  barrera.write(195);
}

// Función removida - el control es automático en controlarBarrera()

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h> 

// --- 1. CONFIGURACIÓN DEL CAJÓN ---
// Cambia esto según el cajón físico (1, 2 o 3)
const int ID_CAJON = 2; 

// --- 2. WIFI ---
// const char* ssid = "Tec-IoT";
// const char* password = "spotless.magnetic.bridge";
const char* ssid = "IZZI-B790";         // <- WiFi actual
const char* password = "2C9569A8B790";  // <- WiFi actual

// --- 3. AZURE API ---
const char* IP_SERVIDOR = "estacionamientoiot-a2gbhzbpfvcfgnbf.canadacentral-01.azurewebsites.net"; 

// Endpoints
String apiURL_Ocupar = "https://" + String(IP_SERVIDOR) + "/Sensores/OcuparCajon";
String apiURL_Liberar = "https://" + String(IP_SERVIDOR) + "/Sensores/LiberarCajon";

// --- 4. HARDWARE ---
const int trigPin = D1; 
const int echoPin = D2; 

// LEDS RGB (Ánodo Común)
const int pinRojo = D6;  
const int pinVerde = D7; 
const int pinAzul = D5;  

const int UMBRAL_DISTANCIA = 7; // cm

// --- 5. VARIABLES DE CONTROL ---
bool estaOcupado = false; // Memoria del estado actual

void setup() {
  Serial.begin(115200);
  
  pinMode(trigPin, OUTPUT); pinMode(echoPin, INPUT);
  pinMode(pinRojo, OUTPUT); pinMode(pinVerde, OUTPUT); pinMode(pinAzul, OUTPUT);

  // --- Test de colores (Ánodo Común: LOW = Prende) ---
  // Rojo
  digitalWrite(pinRojo, LOW); digitalWrite(pinVerde, HIGH); digitalWrite(pinAzul, HIGH);
  delay(500);
  // Verde
  digitalWrite(pinRojo, HIGH); digitalWrite(pinVerde, LOW); digitalWrite(pinAzul, HIGH);
  delay(500);
  // Azul
  digitalWrite(pinRojo, HIGH); digitalWrite(pinVerde, HIGH); digitalWrite(pinAzul, LOW);
  delay(500);
  
  // Iniciar en VERDE (Libre por defecto)
  ponerColorVerde(); 

  Serial.println("\n--- SENSOR DE CAJON INICIADO ---");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    // Parpadeo azul mientras conecta
    digitalWrite(pinAzul, LOW); delay(100); digitalWrite(pinAzul, HIGH); delay(100);
    Serial.print(".");
  }
  Serial.println("\n¡Conectado!");
  
  // Restaurar verde
  ponerColorVerde();
}

void loop() {
  long duration;
  int distance;
  
  // Medir
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  Serial.print("Cajon "); Serial.print(ID_CAJON);
  Serial.print(" | Distancia: "); Serial.print(distance); 
  Serial.println(estaOcupado ? " [OCUPADO]" : " [LIBRE]");

  // --- LÓGICA DE CAMBIO DE ESTADO ---
  
  // CASO 1: DETECTA CARRO (y antes estaba libre)
  if (distance > 0 && distance <= UMBRAL_DISTANCIA && !estaOcupado) {
    Serial.println(">>> 🚗 CARRO LLEGÓ. CAMBIANDO A OCUPADO (ROJO)...");
    
    // 1. Cambiar visualmente primero (Feedback inmediato)
    ponerColorRojo();
    
    // 2. Avisar a Azure
    enviarEstado(true); // true = Ocupar
    
    estaOcupado = true; // Actualizar memoria
  }
  
  // CASO 2: SE VA EL CARRO (y antes estaba ocupado)
  else if ((distance > UMBRAL_DISTANCIA || distance == 0) && estaOcupado) {
    Serial.println(">>> 💨 CARRO SE FUE. CAMBIANDO A LIBRE (VERDE)...");
    
    // 1. Cambiar visualmente
    ponerColorVerde();
    
    // 2. Avisar a Azure
    enviarEstado(false); // false = Liberar
    
    estaOcupado = false; // Actualizar memoria
  }
  
  delay(1000); // Revisar cada segundo
}

// --- FUNCIONES DE COLOR (Ánodo Común) ---
// Recordatorio: LOW prende, HIGH apaga

void ponerColorRojo() {
  digitalWrite(pinRojo, LOW);  // Prende Rojo
  digitalWrite(pinVerde, HIGH); // Apaga Verde
  digitalWrite(pinAzul, HIGH);  // Apaga Azul
}

void ponerColorVerde() {
  digitalWrite(pinRojo, HIGH);  // Apaga Rojo
  digitalWrite(pinVerde, LOW);  // Prende Verde
  digitalWrite(pinAzul, HIGH);  // Apaga Azul
}

// --- FUNCIÓN DE ENVÍO A AZURE ---
void enviarEstado(bool ocupar) {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Importante
  HTTPClient http;
  
  // Elegir URL según la acción
  String url = ocupar ? apiURL_Ocupar : apiURL_Liberar;
  
  // JSON simple: {"Id_Cajon": 1}
  String payload = "{\"Id_Cajon\": " + String(ID_CAJON) + "}";
  
  Serial.print("Enviando a Azure: "); Serial.println(url);
  
  if (http.begin(client, url)) {
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(payload);
    
    if (httpCode == 200) {
      Serial.println("✅ Azure actualizado correctamente.");
    } else {
      Serial.print("❌ Error Azure: "); Serial.println(httpCode);
    }
    http.end();
  } else {
    Serial.println("Error conexión.");
  }
}
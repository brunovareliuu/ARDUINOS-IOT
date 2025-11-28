#include <Servo.h>

// --- CONFIGURACIÓN ARDUINO UNO ---
const int PIN_SENAL = 7;    // Entrada digital desde ESP8266
const int PIN_SERVO = 9;    // Salida PWM al Servo

// --- ÁNGULOS SERVO ---
const int ANGULO_CERRADO = 150;
const int ANGULO_ABIERTO = 60;

Servo barrera;
int estadoAnterior = LOW;

void setup() {
  Serial.begin(9600);
  
  // Configurar pines
  pinMode(PIN_SENAL, INPUT);
  barrera.attach(PIN_SERVO);
  
  // Iniciar cerrado
  barrera.write(ANGULO_CERRADO);
  
  Serial.println("--- ARDUINO UNO: CONTROL SERVO SLAVE ---");
  Serial.println("Esperando señal HIGH en Pin 7...");
}

void loop() {
  // Leer señal del ESP8266
  int estadoActual = digitalRead(PIN_SENAL);
  
  // Solo actuar si hay cambio de estado
  if (estadoActual != estadoAnterior) {
    if (estadoActual == HIGH) {
      Serial.println("✅ Señal RECIBIDA (HIGH) -> Abriendo Barrera");
      barrera.write(ANGULO_ABIERTO);
    } 
    else {
      Serial.println("🛑 Señal CORTADA (LOW) -> Cerrando Barrera");
      delay(2000); // Esperar 2 seg antes de cerrar (seguridad)
      barrera.write(ANGULO_CERRADO);
    }
    estadoAnterior = estadoActual;
  }
  
  delay(50); // Pequeña pausa para estabilidad
}

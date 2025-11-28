#include <Servo.h>

// --- CONFIGURACIÓN ---
const int pinServo = D8;
Servo barrera;

// ** CONFIGURA LOS ÁNGULOS A PROBAR **
const int ANGULO_A = 0;    // <-- AQUI EL ANGULO INICIAL (alineado)
const int ANGULO_B = 195;   // <-- AQUI EL ANGULO FINAL (abierto)

void setup() {
  Serial.begin(115200);

  // Configurar servo
  barrera.attach(pinServo);
  barrera.write(0); // Inicia cerrado

  Serial.printf("=== PRUEBA DE SERVO: %d° → %d° ===\n", ANGULO_A, ANGULO_B);
  Serial.println("Cable Naranja -> D8 (señal)");
  Serial.println("Cable Rojo -> VCC (5-6V externo)");
  Serial.println("Cable Café -> GND");
  Serial.println("");

  // Secuencia única: A → B → delay 10s → A → terminar
  ejecutarPrueba();
}

void loop() {
  // No hacer nada - el programa termina después de setup()
  delay(1000);
}

void ejecutarPrueba() {
  // 1. Ir a ángulo A
  Serial.printf("📍 Posición inicial: %d°\n", ANGULO_A);
  barrera.write(ANGULO_A);
  delay(1000);

  // 2. Ir a ángulo B
  Serial.printf("🔄 Moviendo a: %d°\n", ANGULO_B);
  barrera.write(ANGULO_B);
  delay(1000);

  // 3. Esperar 10 segundos
  Serial.println("⏱️  Esperando 10 segundos...");
  delay(10000);

  // 4. Regresar a ángulo A
  Serial.printf("🔄 Regresando a: %d°\n", ANGULO_A);
  barrera.write(ANGULO_A);
  delay(1000);

  // 5. Programa terminado
  Serial.println("✅ PRUEBA TERMINADA");
  Serial.println("Cambia ANGULO_A y ANGULO_B para nueva prueba");
}

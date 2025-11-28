#include <Servo.h>

const int PIN_SERVO = 9; // Asegúrate de conectar el servo al Pin 9
Servo barrera;

void setup() {
  Serial.begin(9600);
  barrera.attach(PIN_SERVO);

  Serial.println("--- PRUEBA PASO A PASO: 10° cada segundo ---");
  Serial.println("Moviendo de 0° a 180° en incrementos de 10°");

  // Iniciar en 0
  barrera.write(0);
  delay(1000);
}

void loop() {
  // SUBIDA: 0° → 180° (incrementos de 10°)
  Serial.println("🔄 SUBIENDO: 0° → 180°");
  for(int angulo = 0; angulo <= 180; angulo += 10) {
    Serial.print("📍 Ángulo: ");
    Serial.print(angulo);
    Serial.println("°");
    barrera.write(angulo);
    delay(1000); // 1 segundo por paso
  }

  delay(2000); // Pausa de 2 segundos en la cima

  // BAJADA: 180° → 0° (decrementos de 10°)
  Serial.println("🔄 BAJANDO: 180° → 0°");
  for(int angulo = 180; angulo >= 0; angulo -= 10) {
    Serial.print("📍 Ángulo: ");
    Serial.print(angulo);
    Serial.println("°");
    barrera.write(angulo);
    delay(1000); // 1 segundo por paso
  }

  delay(3000); // Pausa de 3 segundos antes de repetir
  Serial.println("--- CICLO COMPLETADO ---");
}

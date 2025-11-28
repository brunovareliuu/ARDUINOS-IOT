// --- PRUEBA SIMPLE: SOLO FOTORESISTOR ---

// --- Configuración Sensor Luz (fotoresistor) ---
const int pinSensorLuz = A0;

// --- LEDs de luz ---
const int ledOscuroPin = 12; // GPIO12 (D6) - se enciende cuando está oscuro
const int ledClaroPin  = 15; // GPIO15 (D8) - se enciende cuando hay luz
const int ledExtra1    = 14; // GPIO14 (D5)
const int ledExtra2    = 13; // GPIO13 (D7)

// --- Configuración ---
const int UMBRAL_LDR = 45; // Umbral para considerar "oscuro" (aumentado para evitar fluctuaciones)
const int intervaloLectura = 1000; // Leer cada 1 segundo

void setup() {
  Serial.begin(115200);

  // Configurar pines del sensor de luz
  pinMode(pinSensorLuz, INPUT);
  pinMode(ledOscuroPin, OUTPUT);
  pinMode(ledClaroPin, OUTPUT);
  pinMode(ledExtra1, OUTPUT);
  pinMode(ledExtra2, OUTPUT);

  // Inicializar LEDs apagados
  digitalWrite(ledOscuroPin, LOW);
  digitalWrite(ledClaroPin, LOW);
  digitalWrite(ledExtra1, LOW);
  digitalWrite(ledExtra2, LOW);

  Serial.println("--- PRUEBA SIMPLE: FOTORESISTOR ---");
  Serial.println("Conecta fotoresistor a A0 con resistencia 10kΩ a GND");
  Serial.println("Umbral: < 200 = OSCURO (LEDs ON)");
  Serial.println("         > 200 = CLARO (LEDs OFF)");
  Serial.println("");
}

void loop() {
  // Leer luz (fotoresistor en A0)
  int nivelLuz = analogRead(pinSensorLuz);
  Serial.print("Nivel_Luz: ");
  Serial.println(nivelLuz);

  bool esOscuro = (nivelLuz < UMBRAL_LDR);

  if (esOscuro) {
    Serial.println(">>> OSCURO: LEDs ON");
    // Oscuro -> encender TODOS los LEDs
    digitalWrite(ledOscuroPin, HIGH); // D6 - ON
    digitalWrite(ledClaroPin, HIGH);  // D8 - ON
    digitalWrite(ledExtra1, HIGH);    // D5 - ON
    digitalWrite(ledExtra2, HIGH);    // D7 - ON
  } else {
    Serial.println(">>> CLARO: LEDs OFF");
    // Claro -> apagar TODOS los LEDs
    digitalWrite(ledOscuroPin, LOW);  // D6 - OFF
    digitalWrite(ledClaroPin, LOW);   // D8 - OFF
    digitalWrite(ledExtra1, LOW);     // D5 - OFF
    digitalWrite(ledExtra2, LOW);     // D7 - OFF
  }

  delay(intervaloLectura);
}

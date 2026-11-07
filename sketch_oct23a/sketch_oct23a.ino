// --- PINES DEL SENSOR ULTRASÓNICO ---
const int trigPin = 22;
const int echoPin = 23;

// --- PINES DE LOS 4 LEDS RGB (Pines PWM 2 al 13 en el Arduino Mega) ---
const int r1 = 2;  const int g1 = 3;  const int b1 = 4;
const int r2 = 5;  const int g2 = 6;  const int b2 = 7;
const int r3 = 8;  const int g3 = 9;  const int b3 = 10;
const int r4 = 11; const int g4 = 12; const int b4 = 13;

// Variable para controlar que el ciclo de 4 segundos ocurra una sola vez por acercamiento
bool cicloEspecialCompletado = false;

void setup() {
  // Configurar sensor
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // Configurar todos los pines de los LEDs como salida (255 = Apagado)
  int pines[] = {r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4};
  for (int i = 0; i < 12; i++) {
    pinMode(pines[i], OUTPUT);
    analogWrite(pines[i], 255); 
  }

  Serial.begin(9600);
}

void loop() {
  long duracion;
  int distancia;

  // Pulso del sensor ultrasónico
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  duracion = pulseIn(echoPin, HIGH);
  distancia = duracion * 0.034 / 2;

  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.println(" cm");

  // --- LÓGICA DE PROXIMIDAD ---
  if (distancia > 10 || distancia <= 0) {
    // Si se aleja a más de 10 cm, reiniciamos el estado especial
    cicloEspecialCompletado = false;
    Serial.print("intro");
    if (distancia > 40) {
      apagarTodo();
      delay(100);
    } 
    else if (distancia > 30) {
      // Nivel 1: Verde
      fijarColor(1, 255, 0, 255);   
      fijarColor(2, 255, 255, 255); 
      fijarColor(3, 255, 255, 255); 
      fijarColor(4, 255, 255, 255); 
      delay(100);
    } 
    else if (distancia > 20) {
      // Nivel 2: Verde y Amarillo
      fijarColor(1, 255, 0, 255);   
      fijarColor(2, 0, 0, 255);     
      fijarColor(3, 255, 255, 255); 
      fijarColor(4, 255, 255, 255); 
      delay(100);
    } 
    else if (distancia > 10) {
      // Nivel 3: Verde, Amarillo y Azul
      fijarColor(1, 255, 0, 255);   
      fijarColor(2, 0, 0, 255);     
      fijarColor(3, 255, 255, 0);   
      fijarColor(4, 255, 255, 255); 
      delay(100);
    }
  } 
  else {
    // --- MENOS DE 10 CM (< 10 cm) ---
    if (!cicloEspecialCompletado) {
      //------Seleccion de aura
      // 1. Ejecutar el modo aleatorio durante 4 segundos (4000 milisegundos)
      Serial.print("seleccion");
      unsigned long tiempoInicio = millis();
      
      while (millis() - tiempoInicio < 4000) {
        fijarColor(1, random(256), random(256), random(256));
        fijarColor(2, random(256), random(256), random(256));
        fijarColor(3, random(256), random(256), random(256));
        fijarColor(4, random(256), random(256), random(256));
        delay(150); // Velocidad de cambio en el modo aleatorio
      }

      // 2. Elegir un color fijo al azar entre: Celeste, Amarillo, Rosa o Violeta
      int colorElegido = random(4);
      int rFijo, gFijo, bFijo;

      if (colorElegido == 0) {
        // Celeste (Verde + Azul)
        rFijo = 255; gFijo = 0;   bFijo = 0;
        Serial.print("nivis")
      } 
      else if (colorElegido == 1) {
        // Amarillo (Rojo + Verde)
        rFijo = 0;   gFijo = 0;   bFijo = 255;
        Serial.print("sahra")
      } 
      else if (colorElegido == 2) {
        // Rosa (Rojo + un toque de azul)
        rFijo = 0;   gFijo = 150; bFijo = 80;
        Serial.print("jangal")
      } 
      else {
        // Violeta (Rojo + Azul intenso)
        rFijo = 80;  gFijo = 255; bFijo = 0;
        Serial.print("skog")
      }

      // 3. Aplicar el color fijo a los 4 LEDs y bloquear hasta que se alejen
      fijarColor(1, rFijo, gFijo, bFijo);
      fijarColor(2, rFijo, gFijo, bFijo);
      fijarColor(3, rFijo, gFijo, bFijo);
      fijarColor(4, rFijo, gFijo, bFijo);

      cicloEspecialCompletado = true; // Marca que ya hizo los 4s y se congeló
    }
    
    delay(10000); // Pequeña pausa mientras se mantiene estático en el color elegido
    Serial.print("intro");
  }
}

// Función auxiliar para fijar colores usando PWM (Ánodo Común)
void fijarColor(int numeroLed, int r, int g, int b) {
  if (numeroLed == 1) { analogWrite(r1, r); analogWrite(g1, g); analogWrite(b1, b); }
  if (numeroLed == 2) { analogWrite(r2, r); analogWrite(g2, g); analogWrite(b2, b); }
  if (numeroLed == 3) { analogWrite(r3, r); analogWrite(g3, g); analogWrite(b3, b); }
  if (numeroLed == 4) { analogWrite(r4, r); analogWrite(g4, g); analogWrite(b4, b); }
}

void apagarTodo() {
  fijarColor(1, 255, 255, 255);
  fijarColor(2, 255, 255, 255);
  fijarColor(3, 255, 255, 255);
  fijarColor(4, 255, 255, 255);
}
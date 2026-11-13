// --- PINES DEL SENSOR ULTRASÓNICO ---
const int trigPin = 22;
const int echoPin = 23;

// --- PINES DE LOS 4 LEDS RGB (TRONCO) - ÁNODO COMÚN (255 = apagado, 0 = máximo) ---
const int r1 = 2;  const int g1 = 3;  const int b1 = 4;
const int r2 = 5;  const int g2 = 6;  const int b2 = 7;
const int r3 = 8;  const int g3 = 9;  const int b3 = 10;
const int r4 = 11; const int g4 = 12; const int b4 = 13;

// --- PINES DE LOS 4 LEDS FLORES (HIGH = apagado, LOW = encendido) ---
const int ledNivis  = 30;
const int ledSahra  = 31;
const int ledJangal = 32;
const int ledSkog   = 33;

// --- LEDS INDICADORES (LEDs simples de 2 patas, con fade in) ---
// Cada LED va a un pin PWM del Mega con una resistencia (220 ohm) en serie.
// Todos suben y bajan juntos. Pines PWM libres en el Mega: 44, 45 y 46.
// Para usar menos LEDs, quita pines de la lista y ajusta NUM_INDICADORES.
const int indicadorPines[] = {44, 45, 46};
const int NUM_INDICADORES = 3;
const int INDICADOR_MAX = 255;                  // Brillo máximo (0-255)
const bool INDICADOR_ACTIVO_EN_BAJO = true;     // true: igual que las flores (LED entre 5V y el pin, LOW = enciende)
                                                // false: LED entre el pin y GND (HIGH = enciende)

// --- PARÁMETROS AJUSTABLES ---
const int DISTANCIA_MANO_CM = 10;                       // Menos de esto = hay manos
const unsigned long INTERVALO_LECTURA = 60;             // Cada cuánto se mide (ms)
const int LECTURAS_PARA_APAGAR = 3;                     // Lecturas seguidas sin manos para apagar (evita falsos apagados)
const unsigned long DURACION_FADE_IN_INDICADOR = 4000;  // Cuánto tarda el indicador en encenderse del todo (ms)
const unsigned long ESPERA_ANTES_SELECCION = 1000;      // Cuánto espera encendido antes de empezar la selección (ms)
const unsigned long DURACION_SELECCION = 29000;          // Duración de la ruleta (ms)
const unsigned long VELOCIDAD_RULETA = 150;             // Cambio de color en la ruleta (ms)
const unsigned long DURACION_FADE = 2000;               // Duración del apagado gradual (ms)
const unsigned long INTERVALO_FADE = 15;                // Cada cuánto se actualiza el fade (ms)
const unsigned long TIEMPO_FLOR_ENCENDIDA = 1000;      // Cuánto queda prendida la flor después del apagado (ms). 0 = hasta la próxima vez

// --- ESTADOS ---
// ESPERA -> INTRO_FADE (indicador sube) -> INTRO_ESPERA (indicador al máximo, pausa)
//        -> SELECCION (ruleta) -> FIJO (bioma elegido) -> APAGANDO -> ESPERA
enum Estado { ESPERA, INTRO_FADE, INTRO_ESPERA, SELECCION, FIJO, APAGANDO };
Estado estado = ESPERA;

// Color actual de cada LED del tronco (para poder hacer el fade desde donde estén)
int colorActual[4][3] = {{255,255,255},{255,255,255},{255,255,255},{255,255,255}};
int colorInicioFade[4][3];
unsigned long tInicioFade = 0;
unsigned long tUltimoPasoFade = 0;

// Indicador
int brilloIndicador = 0;          // 0 = apagado, 255 = máximo
int brilloIndicadorInicio = 0;    // Brillo desde el que arranca el fade in o el fade out
unsigned long tInicioIntro = 0;
unsigned long tInicioEsperaIntro = 0;

unsigned long tUltimaLectura = 0;
unsigned long tInicioSeleccion = 0;
unsigned long tUltimoCambioRuleta = 0;
int lecturasSinMano = 0;

// Flor elegida
bool florEncendida = false;
unsigned long tFinApagado = 0;    // Momento en que terminó el apagado (aquí empieza a contar TIEMPO_FLOR_ENCENDIDA)


void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  int pines[] = {r1, g1, b1, r2, g2, b2, r3, g3, b3, r4, g4, b4};
  for (int i = 0; i < 12; i++) {
    pinMode(pines[i], OUTPUT);
    analogWrite(pines[i], 255);
  }

  pinMode(ledNivis, OUTPUT);
  pinMode(ledSahra, OUTPUT);
  pinMode(ledJangal, OUTPUT);
  pinMode(ledSkog, OUTPUT);
  apagarBiomas();

  for (int i = 0; i < NUM_INDICADORES; i++) {
    pinMode(indicadorPines[i], OUTPUT);
  }
  setIndicador(0);

  randomSeed(analogRead(A0)); // Pin A0 sin conectar para que el azar varíe en cada arranque
  Serial.begin(9600);
}

void loop() {
  unsigned long ahora = millis();

  // ---------- 1. VERIFICAR MANOS TODO EL TIEMPO ----------
  if (ahora - tUltimaLectura >= INTERVALO_LECTURA) {
    tUltimaLectura = ahora;

    int distancia = medirDistancia();
    bool hayMano = (distancia > 0 && distancia <= DISTANCIA_MANO_CM);

   // Serial.print("Distancia: ");
   // Serial.print(distancia);
   // Serial.println(" cm");

    if (hayMano) {
      lecturasSinMano = 0;
      // Si hay manos y estaba apagado (o apagándose), arranca la secuencia desde el indicador
      if (estado == ESPERA || estado == APAGANDO) {
        iniciarIntro(ahora);
      }
    } else {
      lecturasSinMano++;
      // Sin manos (confirmado por varias lecturas): apagado gradual desde cualquier fase activa
      if (lecturasSinMano >= LECTURAS_PARA_APAGAR && estado != ESPERA && estado != APAGANDO) {
        iniciarApagado(ahora);
      }
    }
  }

  // ---------- 2. FADE IN DEL INDICADOR (sin bloquear) ----------
  if (estado == INTRO_FADE) {
    unsigned long transcurrido = ahora - tInicioIntro;
    if (transcurrido >= DURACION_FADE_IN_INDICADOR) {
      setIndicador(255);
      estado = INTRO_ESPERA;
      tInicioEsperaIntro = ahora;
      Serial.println("indicador al maximo, esperando");
    } else {
      setIndicador(brilloIndicadorInicio + (255 - brilloIndicadorInicio) * transcurrido / DURACION_FADE_IN_INDICADOR);
    }
  }

  // ---------- 3. PAUSA CON EL INDICADOR ENCENDIDO ----------
  if (estado == INTRO_ESPERA) {
    if (ahora - tInicioEsperaIntro >= ESPERA_ANTES_SELECCION) {
      iniciarSeleccion(ahora);
    }
  }

  // ---------- 4. ANIMACIÓN DE SELECCIÓN (sin bloquear) ----------
  if (estado == SELECCION) {
    if (ahora - tInicioSeleccion < DURACION_SELECCION) {
      if (ahora - tUltimoCambioRuleta >= VELOCIDAD_RULETA) {
        tUltimoCambioRuleta = ahora;
        for (int i = 1; i <= 4; i++) {
          fijarColor(i, random(256), random(256), random(256));
        }
      }
    } else {
      elegirBioma();
      estado = FIJO;
    }
  }
  // En estado FIJO no hay nada que hacer: el color queda quieto
  // y el sensor sigue vigilando hasta que se retiren las manos.

  // ---------- 5. APAGADO GRADUAL (sin bloquear) ----------
  if (estado == APAGANDO) {
    actualizarFade(ahora);
  }

    // ---------- 6. APAGAR LA FLOR TRAS UN TIEMPO SIN USO ----------
  if (estado == ESPERA && florEncendida && TIEMPO_FLOR_ENCENDIDA > 0
      && ahora - tFinApagado >= TIEMPO_FLOR_ENCENDIDA) {
    apagarBiomas();
    florEncendida = false;
    Serial.println("Flor apagada por tiempo");
    Serial.println("inicio");
  }

}

// ---------- FUNCIONES ----------

int medirDistancia() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000); // Timeout 30 ms (~5 m); devuelve 0 si no hay eco
  return duracion * 0.034 / 2;
}

// Escribe el brillo de los indicadores (0-255) respetando el máximo y la polaridad
void setIndicador(int brillo) {
  brilloIndicador = constrain(brillo, 0, 255);
  // (long) evita desbordamiento: en el Mega un int es de 16 bits y 255*255 no cabe
  int valor = (long)brilloIndicador * INDICADOR_MAX / 255;
  if (INDICADOR_ACTIVO_EN_BAJO) valor = 255 - valor;
  for (int i = 0; i < NUM_INDICADORES; i++) {
    analogWrite(indicadorPines[i], valor);
  }
}

// Arranca la secuencia: primero el indicador sube con fade in
void iniciarIntro(unsigned long ahora) {
  Serial.println("manos detectadas -> fade in del indicador");
  apagarTronco();
  apagarBiomas();
  florEncendida = false;
  brilloIndicadorInicio = brilloIndicador; // Si venía apagándose, sube desde donde estaba
  tInicioIntro = ahora;
  estado = INTRO_FADE;
}

void iniciarSeleccion(unsigned long ahora) {
  apagarBiomas();
  estado = SELECCION;
  tInicioSeleccion = ahora;
  tUltimoCambioRuleta = 0; // fuerza un primer cambio inmediato
  Serial.println("seleccionando");
}

void elegirBioma() {
  int colorElegido = random(4);
  int rFijo, gFijo, bFijo;

  apagarBiomas();

  if (colorElegido == 0) {          // Nivis (celeste)
    rFijo = 255; gFijo = 0;   bFijo = 0;
    Serial.println("nivis");
    digitalWrite(ledNivis, LOW);
  } else if (colorElegido == 1) {   // Sahra (amarillo)
    rFijo = 0;   gFijo = 0;   bFijo = 255;
    Serial.println("sahra");
    digitalWrite(ledSahra, LOW);
  } else if (colorElegido == 2) {   // Jangal (rosa)
    rFijo = 0;   gFijo = 150; bFijo = 80;
    Serial.println("jangal");
    digitalWrite(ledJangal, LOW);
  } else {                          // Skog (violeta)
    rFijo = 80;  gFijo = 255; bFijo = 0;
    Serial.println("skog");
    digitalWrite(ledSkog, LOW);
  }

  for (int i = 1; i <= 4; i++) {
    fijarColor(i, rFijo, gFijo, bFijo);
  }
  florEncendida = true;
}



void iniciarApagado(unsigned long ahora) {
  Serial.println("Manos retiradas -> apagando gradualmente");
  for (int i = 0; i < 4; i++) {
    for (int c = 0; c < 3; c++) {
      colorInicioFade[i][c] = colorActual[i][c];
    }
  }
  brilloIndicadorInicio = brilloIndicador;
  tInicioFade = ahora;
  tUltimoPasoFade = 0;
  estado = APAGANDO;
  if(!florEncendida){
      Serial.println("inicio");
  }
}

void actualizarFade(unsigned long ahora) {
  if (ahora - tUltimoPasoFade < INTERVALO_FADE) return;
  tUltimoPasoFade = ahora;

  unsigned long transcurrido = ahora - tInicioFade;

  if (transcurrido >= DURACION_FADE) {
    // Fade terminado: tronco e indicador apagados (la flor elegida sigue encendida)
    apagarTodo();
    estado = ESPERA;
    tFinApagado = ahora;
    Serial.println("Apagado completo");
    return;
  }

  // Los 3 canales de cada LED se acercan a 255 (apagado) al mismo ritmo,
  // así el color no cambia de tono mientras se desvanece
  for (int i = 0; i < 4; i++) {
    int r = colorInicioFade[i][0] + (255 - colorInicioFade[i][0]) * transcurrido / DURACION_FADE;
    int g = colorInicioFade[i][1] + (255 - colorInicioFade[i][1]) * transcurrido / DURACION_FADE;
    int b = colorInicioFade[i][2] + (255 - colorInicioFade[i][2]) * transcurrido / DURACION_FADE;
    fijarColor(i + 1, r, g, b);
  }

  // El indicador se apaga junto con el tronco
  setIndicador(brilloIndicadorInicio * (DURACION_FADE - transcurrido) / DURACION_FADE);
}

// Fija color con PWM (ánodo común) y recuerda el valor actual para el fade
void fijarColor(int numeroLed, int r, int g, int b) {
  colorActual[numeroLed - 1][0] = r;
  colorActual[numeroLed - 1][1] = g;
  colorActual[numeroLed - 1][2] = b;
  if (numeroLed == 1) { analogWrite(r1, r); analogWrite(g1, g); analogWrite(b1, b); }
  if (numeroLed == 2) { analogWrite(r2, r); analogWrite(g2, g); analogWrite(b2, b); }
  if (numeroLed == 3) { analogWrite(r3, r); analogWrite(g3, g); analogWrite(b3, b); }
  if (numeroLed == 4) { analogWrite(r4, r); analogWrite(g4, g); analogWrite(b4, b); }
}

void apagarBiomas() {
  digitalWrite(ledNivis, HIGH);
  digitalWrite(ledSahra, HIGH);
  digitalWrite(ledJangal, HIGH);
  digitalWrite(ledSkog, HIGH);
}

// Apaga solo los 4 LEDs RGB del tronco
void apagarTronco() {
  for (int i = 1; i <= 4; i++) {
    fijarColor(i, 255, 255, 255);
  }
}

// Apaga el tronco RGB y el indicador. La flor elegida NO se apaga aquí:
// queda encendida hasta que se inicie la próxima secuencia (iniciarIntro).
void apagarTodo() {
  apagarTronco();
  setIndicador(0);
}

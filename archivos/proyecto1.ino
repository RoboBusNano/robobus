#include <SoftwareSerial.h>

SoftwareSerial BT(2, 3);

// --- Pines motores ---
const int enA = 10; const int in1 = 7; const int in2 = 8;
const int enB = 11; const int in3 = 12; const int in4 = 13;

// --- LEDs ---
const int led1 = 5; const int led2 = 6;

// --- Ultrasonico FRONTAL ---
const int trigFront = A0;
const int echoFront = A1;

// --- Ultrasonico TRASERO ---
const int trigBack = A2;
const int echoBack = A3;

// --- Buzzer activo ---
const int buzzer = A4;

// --- Estados de maniobra ---
#define EST_AVANZAR  0
#define EST_PITAR    1
#define EST_RETRO    2
#define EST_GIRAR    3

// --- Variables ---
bool modoAutonomo = false;
int distFront, distBack;
unsigned long ultimaMedicion = 0;
unsigned long tiempoEstado = 0;
int estadoManiobra = EST_AVANZAR;
int contPitido = 0;
bool obstaculoFrontal = false;

void setup() {
  pinMode(enA, OUTPUT); pinMode(enB, OUTPUT);
  pinMode(in1, OUTPUT); pinMode(in2, OUTPUT);
  pinMode(in3, OUTPUT); pinMode(in4, OUTPUT);
  pinMode(led1, OUTPUT); pinMode(led2, OUTPUT);

  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);
  pinMode(trigBack,  OUTPUT); pinMode(echoBack,  INPUT);
  pinMode(buzzer, OUTPUT);
  digitalWrite(buzzer, LOW);

  Serial.begin(9600);
  BT.begin(9600);
  BT.println("VEHICULO LISTO");
}

void loop() {
  // 1. Bluetooth siempre primero
  if (BT.available()) {
    char comando = BT.read();
    ejecutarComando(comando);
  }

  // 2. Medir sensores cada 100ms siempre
  if (millis() - ultimaMedicion >= 100) {
    ultimaMedicion = millis();
    distFront = medirDistancia(trigFront, echoFront);
    delay(5);
    distBack = medirDistancia(trigBack, echoBack);

    // Seguridad en modo manual
    if (!modoAutonomo) {
      if (distFront > 0 && distFront < 15) {
        parar();
        digitalWrite(buzzer, HIGH);
        digitalWrite(led1, HIGH); digitalWrite(led2, HIGH);
      } else if (distBack > 0 && distBack < 10) {
        parar();
        digitalWrite(buzzer, HIGH);
        digitalWrite(led1, HIGH); digitalWrite(led2, HIGH);
      } else {
        digitalWrite(buzzer, LOW);
        digitalWrite(led1, LOW); digitalWrite(led2, LOW);
      }
    }
  }

  // 3. Logica autonoma sin delay
  if (modoAutonomo) {
    maquinaEstados();
  }
}

void maquinaEstados() {
  unsigned long ahora = millis();

  switch (estadoManiobra) {

    case EST_AVANZAR:
      if (distFront > 0 && distFront < 15) {
        parar();
        obstaculoFrontal = true;
        contPitido = 0;
        estadoManiobra = EST_PITAR;
        tiempoEstado = ahora;
      } else if (distBack > 0 && distBack < 10) {
        parar();
        obstaculoFrontal = false;
        contPitido = 0;
        estadoManiobra = EST_PITAR;
        tiempoEstado = ahora;
      } else {
        adelante();
        digitalWrite(buzzer, LOW);
      }
      break;

    case EST_PITAR:
      if (contPitido < 3) {
        int ciclo = contPitido * 350;
        int t = (ahora - tiempoEstado) - ciclo;
        if (t >= 0 && t < 200) {
          digitalWrite(buzzer, HIGH);
          digitalWrite(led1, HIGH); digitalWrite(led2, HIGH);
        } else if (t >= 200 && t < 350) {
          digitalWrite(buzzer, LOW);
          digitalWrite(led1, LOW); digitalWrite(led2, LOW);
        } else if (t >= 350) {
          contPitido++;
        }
      } else {
        digitalWrite(buzzer, LOW);
        digitalWrite(led1, LOW); digitalWrite(led2, LOW);
        // Frontal: retrocede — Trasero: avanza
        if (obstaculoFrontal) {
          atras();
        } else {
          adelante();
        }
        estadoManiobra = EST_RETRO;
        tiempoEstado = ahora;
      }
      break;

    case EST_RETRO:
      if (ahora - tiempoEstado >= 500) {
        parar();
        estadoManiobra = EST_GIRAR;
        tiempoEstado = ahora;
      }
      break;

    case EST_GIRAR:
      if (ahora - tiempoEstado < 100) {
        parar();
      } else if (ahora - tiempoEstado < 500) {
        // Frontal gira — Trasero no gira, vuelve directo
        if (obstaculoFrontal) {
          derecha();
        } else {
          parar();
        }
      } else {
        parar();
        estadoManiobra = EST_AVANZAR;
      }
      break;
  }
}

void ejecutarComando(char c) {
  if (c >= 'a' && c <= 'z') c = c - 32;

  switch (c) {
    case 'F': modoAutonomo = false; estadoManiobra = EST_AVANZAR; adelante();  break;
    case 'B': modoAutonomo = false; estadoManiobra = EST_AVANZAR; atras();     break;
    case 'L': modoAutonomo = false; estadoManiobra = EST_AVANZAR; izquierda(); break;
    case 'R': modoAutonomo = false; estadoManiobra = EST_AVANZAR; derecha();   break;
    case 'S': modoAutonomo = false; estadoManiobra = EST_AVANZAR; parar();     break;
    case 'A':
      modoAutonomo = true;
      estadoManiobra = EST_AVANZAR;
      BT.println("MODO AUTONOMO");
      break;
    case '1': digitalWrite(led1, HIGH); digitalWrite(led2, HIGH); break;
    case '0': digitalWrite(led1, LOW);  digitalWrite(led2, LOW);  break;
    case 'H':
      digitalWrite(buzzer, HIGH);
      delay(300);
      digitalWrite(buzzer, LOW);
      break;
  }
}

int medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long dur = pulseIn(echo, HIGH, 30000);
  return (dur == 0) ? 999 : dur * 0.034 / 2;
}

void adelante() {
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  analogWrite(enA, 200);
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);  analogWrite(enB, 200);
}

void atras() {
  digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); analogWrite(enA, 200);
  digitalWrite(in3, LOW);  digitalWrite(in4, HIGH); analogWrite(enB, 200);
}

void izquierda() {
  digitalWrite(in1, LOW);  digitalWrite(in2, HIGH); analogWrite(enA, 180);
  digitalWrite(in3, HIGH); digitalWrite(in4, LOW);  analogWrite(enB, 180);
}

void derecha() {
  digitalWrite(in1, HIGH); digitalWrite(in2, LOW);  analogWrite(enA, 180);
  digitalWrite(in3, LOW);  digitalWrite(in4, HIGH); analogWrite(enB, 180);
}

void parar() {
  analogWrite(enA, 0); analogWrite(enB, 0);
  digitalWrite(buzzer, LOW);
  digitalWrite(led1, LOW); digitalWrite(led2, LOW);
}
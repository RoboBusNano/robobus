#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ================= CONFIGURACIÓN DE LA PANTALLA LCD =================
// Dirección I2C de la LCD (0x27 o 0x3F - probar si no funciona)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ================= AJUSTES RÁPIDOS DEL USUARIO =================
// Distancias de los sensores (en cm) - MODIFICA AQUÍ LOS VALORES
#define DISTANCIA_FRONTAL_PELIGRO  15   // Si el obstáculo frontal está a menos de 15cm, frena y pita
#define DISTANCIA_TRASERA_PELIGRO  10   // Si el obstáculo trasero está a menos de 10cm, frena y pita

// Tiempo que se muestra un mensaje enviado por Bluetooth (en milisegundos)
#define TIEMPO_MENSAJE_LCD_MS  8000    // 8 segundos - MODIFICA AQUÍ EL TIEMPO

// ================= FIN DE LOS AJUSTES RÁPIDOS =================

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

// --- Buzzer activo (cambiado a pin 9 para liberar A4 y usar I2C) ---
const int buzzer = 9;

// --- Estados de maniobra para modo autónomo ---
#define EST_AVANZAR  0
#define EST_PITAR    1
#define EST_RETRO    2
#define EST_GIRAR    3

// --- Variables globales ---
bool modoAutonomo = false;
int distFront, distBack;
unsigned long ultimaMedicion = 0;
unsigned long tiempoEstado = 0;
int estadoManiobra = EST_AVANZAR;
int contPitido = 0;
bool obstaculoFrontal = false;

// --- Para mensajes temporales en LCD ---
unsigned long tiempoMensajeLCD = 0;
String mensajeTemporal = "";
bool mostrandoMensaje = false;

// --- Para recibir texto por Bluetooth ---
bool esperandoTexto = false;
String textoRecibido = "";

// --- Variable para mostrar estado de freno/obstáculo en LCD ---
String estadoFreno = "";

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

  // Inicializar LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Robot Listo");
  lcd.setCursor(0, 1);
  lcd.print("Esperando BT...");
  delay(2000);
  lcd.clear();
}

void loop() {
  // 1. Procesar comandos Bluetooth (incluyendo mensajes de texto)
  while (BT.available()) {
    char c = BT.read();
    procesarBluetooth(c);
  }

  // 2. Medir sensores cada 100ms
  if (millis() - ultimaMedicion >= 100) {
    ultimaMedicion = millis();
    distFront = medirDistancia(trigFront, echoFront);
    delay(5);
    distBack = medirDistancia(trigBack, echoBack);

    // Seguridad en modo manual (freno automático si hay obstáculo)
    if (!modoAutonomo) {
      if (distFront > 0 && distFront < DISTANCIA_FRONTAL_PELIGRO) {
        parar();
        estadoFreno = "FRENO FRONTAL";
        digitalWrite(buzzer, HIGH);
        digitalWrite(led1, HIGH); digitalWrite(led2, HIGH);
      } else if (distBack > 0 && distBack < DISTANCIA_TRASERA_PELIGRO) {
        parar();
        estadoFreno = "FRENO TRASERO";
        digitalWrite(buzzer, HIGH);
        digitalWrite(led1, HIGH); digitalWrite(led2, HIGH);
      } else {
        digitalWrite(buzzer, LOW);
        digitalWrite(led1, LOW); digitalWrite(led2, LOW);
        estadoFreno = "";  // No hay freno activo
      }
    }

    // Actualizar LCD (si no hay mensaje temporal o pasó el tiempo configurado)
    if (!mostrandoMensaje || (millis() - tiempoMensajeLCD >= TIEMPO_MENSAJE_LCD_MS)) {
      mostrandoMensaje = false;
      actualizarLCDInfo();
    }
  }

  // 3. Lógica autónoma
  if (modoAutonomo) {
    maquinaEstados();
  }
}

void procesarBluetooth(char c) {
  // Modo de recepción de texto después del comando 'T'
  if (esperandoTexto) {
    if (c == '\n' || c == '\r') {
      if (textoRecibido.length() > 0) {
        mostrarMensajeLCD(textoRecibido);
      }
      esperandoTexto = false;
      textoRecibido = "";
    } else {
      textoRecibido += c;
      if (textoRecibido.length() >= 32) {
        mostrarMensajeLCD(textoRecibido);
        esperandoTexto = false;
        textoRecibido = "";
      }
    }
    return;
  }

  // Comandos originales (mayúsculas y minúsculas aceptadas)
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
      mostrarMensajeLCD("Modo Autonomo");
      break;
    case '1': digitalWrite(led1, HIGH); digitalWrite(led2, HIGH); mostrarMensajeLCD("LEDs ON"); break;
    case '0': digitalWrite(led1, LOW);  digitalWrite(led2, LOW);  mostrarMensajeLCD("LEDs OFF"); break;
    case 'H':
      digitalWrite(buzzer, HIGH);
      delay(300);
      digitalWrite(buzzer, LOW);
      mostrarMensajeLCD("Bip");
      break;
    case 'T':
      esperandoTexto = true;
      textoRecibido = "";
      break;
  }
}

void mostrarMensajeLCD(String msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  if (msg.length() <= 16) {
    lcd.print(msg);
  } else {
    lcd.print(msg.substring(0, 16));
    lcd.setCursor(0, 1);
    lcd.print(msg.substring(16, 32));
  }
  mostrandoMensaje = true;
  tiempoMensajeLCD = millis();
}

void actualizarLCDInfo() {
  lcd.clear();
  
  // Primera línea: modo y distancias
  lcd.setCursor(0, 0);
  lcd.print(modoAutonomo ? "AUTO" : "MANUAL");
  lcd.print(" F:");
  lcd.print(distFront > 99 ? 99 : distFront);
  lcd.print("cm T:");
  lcd.print(distBack > 99 ? 99 : distBack);
  lcd.print("cm");
  
  // Segunda línea: estado de freno/obstáculo o comando OK
  lcd.setCursor(0, 1);
  if (estadoFreno != "") {
    // Mostrar el mensaje de freno (corto)
    if (estadoFreno == "FRENO FRONTAL") lcd.print("OBST FRENO!");
    else if (estadoFreno == "FRENO TRASERO") lcd.print("OBST TRASERO!");
    else lcd.print(estadoFreno);
  } 
  else if (!modoAutonomo) {
    lcd.print("Cmd OK");
  }
  else {
    // En modo autónomo, mostrar en qué estado está la máquina de estados
    switch (estadoManiobra) {
      case EST_AVANZAR: lcd.print("Avanzando"); break;
      case EST_PITAR:   lcd.print("Pitando..."); break;
      case EST_RETRO:   lcd.print("Retrocedo"); break;
      case EST_GIRAR:   lcd.print("Girando"); break;
      default: lcd.print("Esperando");
    }
  }
}

// ================= MÁQUINA DE ESTADOS (MODO AUTÓNOMO) =================
void maquinaEstados() {
  unsigned long ahora = millis();

  switch (estadoManiobra) {
    case EST_AVANZAR:
      if (distFront > 0 && distFront < DISTANCIA_FRONTAL_PELIGRO) {
        parar();
        estadoFreno = "OBST FRONTAL";
        obstaculoFrontal = true;
        contPitido = 0;
        estadoManiobra = EST_PITAR;
        tiempoEstado = ahora;
      } else if (distBack > 0 && distBack < DISTANCIA_TRASERA_PELIGRO) {
        parar();
        estadoFreno = "OBST TRASERO";
        obstaculoFrontal = false;
        contPitido = 0;
        estadoManiobra = EST_PITAR;
        tiempoEstado = ahora;
      } else {
        adelante();
        digitalWrite(buzzer, LOW);
        estadoFreno = "";
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

// ================= FUNCIÓN DE MEDICIÓN DE DISTANCIA =================
int medirDistancia(int trig, int echo) {
  digitalWrite(trig, LOW);
  delayMicroseconds(2);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  long dur = pulseIn(echo, HIGH, 30000);
  return (dur == 0) ? 999 : dur * 0.034 / 2;
}

// ================= MOVIMIENTOS DEL ROBOT =================
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

// --- PINI MOTOARE (CNC Shield V3) ---
const int X_STEP_PIN = 2; // Baza (Rotire - A4988 FULL STEP)
const int X_DIR_PIN = 5;
const int Y_STEP_PIN = 3; // Turnul (Sus-Jos - DRV8825 1/32 MICROSTEPPING)
const int Y_DIR_PIN = 6;
const int EN_PIN = 8;

// --- PINI INTRĂRI (Joystick & Butoane) ---
const int pinX = A5;       // Axa X Joystick
const int pinY = A4;       // Axa Y Joystick
const int pinJoyBtn = A0;  // Abort - Toggle Manual/Auto
const int pinBtnAuto = A1; // Hold - START Secvență (Buton Verde)
const int pinBtnHome = A2; // Resume - OVERRIDE / RESET (Buton Roșu)

// --- MATEMATICA SISTEMULUI ---
const long PASI_370_GRADE = 411; // Rotatie totala (Reductie 2:1, Full Step)

// =========================================================================
// !!! AICI SE MODIFICA DISTANTA PENTRU BRATUL VERTICAL !!!
// Cureaua GT2 (cu scripete de 20 dinti si 1/32 microstepping) necesita
// fix 160 de pasi pentru a parcurge 1 milimetru real.
//
// FORMULA: Distanta dorita (in mm) * 160 = Valoarea de introdus.
// Exemplul curent: Vrei o cursa de 130 mm -> 130 * 160 = 20800 pasi.
// Daca vrei 150 mm -> 150 * 160 = 24000 pasi.
// Schimba numarul de mai jos:
// =========================================================================
const long PASI_CURSA_Y = 20800; 
// =========================================================================

const int NUMAR_POZITII = 8; 
const long PASI_X_PER_POZITIE = PASI_370_GRADE / NUMAR_POZITII; 

long pozitieX = 0; 
long pozitieY = 0; 
bool modManualActiv = false; 

unsigned long timpCurent = 0;
unsigned long timpAnteriorX = 0;
unsigned long timpAnteriorY = 0;
unsigned long intervalX = 0; 
unsigned long intervalY = 0;
int dirX = 1;
int dirY = 1;

void setup() {
  Serial.begin(9600);
  
  pinMode(X_STEP_PIN, OUTPUT); pinMode(X_DIR_PIN, OUTPUT);
  pinMode(Y_STEP_PIN, OUTPUT); pinMode(Y_DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  digitalWrite(EN_PIN, LOW); // Activează motoarele

  pinMode(pinJoyBtn, INPUT_PULLUP);
  pinMode(pinBtnAuto, INPUT_PULLUP);
  pinMode(pinBtnHome, INPUT_PULLUP);

  Serial.println("Sistem initializat. Mod curent: STANDBY (astept comenzi auto)");
}

void loop() {
  timpCurent = micros(); 

  static unsigned long ultimulApasatJoy = 0;
  if (digitalRead(pinJoyBtn) == LOW && millis() - ultimulApasatJoy > 500) {
    modManualActiv = !modManualActiv;
    Serial.println(modManualActiv ? "MOD: MANUAL (Fara limite de distanta)" : "MOD: STANDBY (Astept comenzi auto)");
    ultimulApasatJoy = millis();
  }

  if (digitalRead(pinBtnHome) == LOW) {
    Serial.println("! OVERRIDE MANUAL ! Revenire la 0...");
    mergiLaPozitiaZero();
  }

  if (!modManualActiv && digitalRead(pinBtnAuto) == LOW) {
    Serial.println("START SCANARE (8 Cadrane / 2 minute)...");
    ruleazaSecventaAuto();
  }

  // --- MOD MANUAL (FARA LIMITE SOFTWARE) ---
  if (modManualActiv) {
    int valX = analogRead(pinX);
    int valY = analogRead(pinY);

    // CONTROL AXA X (Rotatie - FULL STEP) 
    // Au fost sterse conditionarile "&& pozitieX > 0" si "&& pozitieX < PASI_370_GRADE"
    if (valX < 400) { 
      digitalWrite(X_DIR_PIN, HIGH); 
      dirX = -1;
      intervalX = 5000; // Viteza fixa de croaziera (~60 RPM)
    } else if (valX > 600) { 
      digitalWrite(X_DIR_PIN, LOW); 
      dirX = 1;
      intervalX = 5000; // Viteza fixa de croaziera (~60 RPM)
    } else { 
      intervalX = 0; 
    }

    // CONTROL AXA Y (Turn - 1/32 MICROSTEP)
    // Au fost sterse conditionarile de limite spatiale
    if (valY < 400) { 
      digitalWrite(Y_DIR_PIN, HIGH); 
      dirY = -1;
      intervalY = map(valY, 400, 0, 1000, 150); 
    } else if (valY > 600) { 
      digitalWrite(Y_DIR_PIN, LOW); 
      dirY = 1;
      intervalY = map(valY, 600, 1023, 1000, 150);
    } else { 
      intervalY = 0; 
    }

    // EXECUTIE PASI NON-BLOCKING (Fluid)
    if (intervalX > 0 && timpCurent - timpAnteriorX >= intervalX) {
      impulsScurt(X_STEP_PIN);
      pozitieX += dirX; // Continuam sa contorizam pasii ca sa stim mereu unde e "Home"
      timpAnteriorX = timpCurent;
    }
    
    if (intervalY > 0 && timpCurent - timpAnteriorY >= intervalY) {
      impulsScurt(Y_STEP_PIN);
      pozitieY += dirY; // Continuam sa contorizam pasii
      timpAnteriorY = timpCurent;
    }
  }
}

bool verificareOverride() {
  if (digitalRead(pinBtnHome) == LOW) {
    Serial.println("! OVERRIDE AUTO INTERCEPTAT !");
    mergiLaPozitiaZero();
    return true; 
  }
  return false;
}

void ruleazaSecventaAuto() {
  // Preluam pozitia de start curenta ca sa stim de unde ne intoarcem, 
  // in caz ca nu am plecat fix din 0,0 din cauza ca ne-am miscat manual fara limite
  long pozitieStartRotatie = pozitieX;
  
  for (int faza = 0; faza < NUMAR_POZITII; faza++) {
    Serial.print("Scanare faza: "); Serial.println(faza + 1);

    // --- 1. RIDICARE Y ---
    digitalWrite(Y_DIR_PIN, LOW); 
    for (long y = 0; y < PASI_CURSA_Y; y++) {
      if (verificareOverride()) return; 
      impulsDelaiat(Y_STEP_PIN, 150); 
      pozitieY++;
    }

    // --- 2. COBORARE Y + ROTATIE X (Interpolare Concurenta Optimizata) ---
    digitalWrite(Y_DIR_PIN, HIGH); 
    digitalWrite(X_DIR_PIN, LOW);  
    
    long pasiY_efectuati = 0;
    long pasiX_efectuati = 0;
    
    unsigned long ultimulTimpY = micros();
    unsigned long ultimulTimpX = micros();
    
    unsigned long intervalSincronizatX = (150UL * PASI_CURSA_Y) / PASI_X_PER_POZITIE;

    while (pasiY_efectuati < PASI_CURSA_Y || pasiX_efectuati < PASI_X_PER_POZITIE) {
      if (verificareOverride()) return;
      unsigned long acum = micros();

      if (pasiY_efectuati < PASI_CURSA_Y && acum - ultimulTimpY >= 150) {
        impulsScurt(Y_STEP_PIN);
        pozitieY--;
        pasiY_efectuati++;
        ultimulTimpY = acum;
      }

      if (pasiX_efectuati < PASI_X_PER_POZITIE && acum - ultimulTimpX >= intervalSincronizatX) {
        impulsScurt(X_STEP_PIN);
        pozitieX++;
        pasiX_efectuati++;
        ultimulTimpX = acum;
      }
    }
  }
  Serial.println("SECVENTA FINALIZATA CU SUCCES!");
}

void mergiLaPozitiaZero() {
  Serial.println("Incepere rulaj catre coordonatele X=0, Y=0...");

  digitalWrite(Y_DIR_PIN, (pozitieY > 0) ? HIGH : LOW); 
  while (pozitieY != 0) {
    impulsDelaiat(Y_STEP_PIN, 150); 
    pozitieY += (pozitieY > 0) ? -1 : 1;
  }

  digitalWrite(X_DIR_PIN, (pozitieX > 0) ? HIGH : LOW); 
  while (pozitieX != 0) {
    impulsDelaiat(X_STEP_PIN, 5000); 
    pozitieX += (pozitieX > 0) ? -1 : 1;
  }
  
  Serial.println("Revenit exact la coordonatele 0,0.");
  modManualActiv = false; 
}

void impulsScurt(int pinAxa) {
  digitalWrite(pinAxa, HIGH);
  delayMicroseconds(30); 
  digitalWrite(pinAxa, LOW);
}

void impulsDelaiat(int pinAxa, int delayUs) {
  digitalWrite(pinAxa, HIGH);
  delayMicroseconds(30); 
  digitalWrite(pinAxa, LOW);
  delayMicroseconds(delayUs);
}

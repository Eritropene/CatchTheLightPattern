#include <EnableInterrupt.h>
#include <avr/sleep.h>

#define LED(X) (X+10)
#define BUTTON(X) (X+6)
#define R_LED 5
#define POT A0

#define IDLE_TIME 1500
#define START_WAIT_TIME 1500
#define BASE_DECREASE_FACTOR 5
#define N_PENALTIES 3
#define PULSE_SPEED 0.02

enum State {WAIT_FOR_INPUT, SLEEP, DISPLAY_PATTERN, INPUT_PATTERN, PENALTY, WAIT_FOR_DISPLAY, GAME_OVER};

State curState = WAIT_FOR_INPUT;
int penalty = 0;
int score = 0;
int level = 1;
int difficulty = 1;
int pattern = 0;
int guess = 0;
long timer;
int t1 = 2000;
int t2 = 5000;

float pulse, pulseDirection;

bool buttonState[4]; // buttons state
int button1State = 0; // 0-> LOW, 1-> RISING, 2-> HIGH
int button1Debounce = 0;

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A1));
  /* Initialize pins */
  for (int i=0; i<4; i++) {
    pinMode(BUTTON(i), INPUT_PULLUP);
    enableInterrupt(BUTTON(i), buttonInterrupt, RISING); // attach interrupt handler to buttons
    pinMode(LED(i), OUTPUT);
  }

  waitForInput();
}

void loop() {
  /* Update buttons state */
  for (int i = 1; i < 4; i++) {
    buttonState[i] = !((bool)digitalRead(BUTTON(i)));
  }
  if (buttonState[0] == (bool)digitalRead(BUTTON(0))) {
    button1Debounce = millis();
    buttonState[0] = !((bool)digitalRead(BUTTON(0)));
  }
  if ((millis() - button1Debounce) > 50) {
    buttonState[0] = !((bool)digitalRead(BUTTON(0)));
  }
  if (buttonState[0] && button1State == 0 ) {
    button1State = 1;
  } else if (buttonState[0] && button1State == 1) {
    button1State = 2;
  } else if (!buttonState[0]) {
    button1State = 0;
  }

  /* Update game state */
  switch (curState) {
    case WAIT_FOR_INPUT:
      if (timer >= millis()) { // timer = 10s
        /* Led Pulse */
        pulse += pulseDirection;
        if (pulse > 255 || pulse < 0) pulseDirection *= -1;
        analogWrite(R_LED, (int)pulse);

        /* Button T1 check */
        if (buttonState[0]) {
          startGame();
        }
      } else {
        sleep();
      }
      break;
    case SLEEP:
      
      break;
    case DISPLAY_PATTERN:
      for (int x = 0; x < 4; x++) {
        if (buttonState[x]) {
          givePenalty();          
        }        
      }
      if (millis() > timer) {
        inputPattern();
      }
      break;
    case INPUT_PATTERN:
      for (int x = 0; x < 4; x++) {
        if (buttonState[x]) {
          guess | 1 << x;
          digitalWrite(LED(x), HIGH);
        }
      }
      if (millis() > timer) {
        if (guess == pattern) {
          level++;
          score += level * difficulty;
          t1 *= 1 - ((15 * ((float)(difficulty) / 2) + 0.5) / 100);
          t2 *= 1 - ((15 * ((float)(difficulty) / 2) + 0.5) / 100);
          Serial.print("New point! Score: ");
          Serial.println(score);
          waitForDisplay();
        } else {
          givePenalty();
        }
      }
      break;
    case PENALTY:
      if (millis() > timer) {
        if (penalty >= N_PENALTIES) {
          Serial.print("Game Over! Final score: ");
          Serial.println(score);
          gameOver();
        }
      }
      break;
    case WAIT_FOR_DISPLAY:
      if (millis() > timer) {
        displayPattern();        
      }
      break;
    case GAME_OVER:
      if (millis() > timer) {
        waitForInput();
      }
      break;
  }
}

void randomizeLeds() {
  pattern = random(); // create random pattern (Es: 1011)
  pattern = (pattern % 14) + 1;

  for (int x = 0; x < 4; x++) { // turn on the leds
    int mask = 1 << x;
    digitalWrite(LED(x), pattern & mask);
  }
}

void sleep() {
  curState = SLEEP;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_mode();
  // WAIT FOR INTERRUPT
  waitForInput();  
}

/* Interrupt function, used to wake up the microcontroller */
void buttonInterrupt() {}

void waitForInput() {
  curState = WAIT_FOR_INPUT;
  Serial.println("Welcome to the Catch the Light Pattern Game! Press Key T1 to Start.");
  timer = 10000 + millis();

  pulse = 0;
  pulseDirection = PULSE_SPEED;
}

void startGame() {
  Serial.println("Go!");
  digitalWrite(R_LED, LOW);
  difficulty = (analogRead(POT) / 255) + 1;
  penalty = 0;
  level = 0;
  score = 0;
  t1 = 5000;
  t2 = 10000;
  waitForDisplay();
}

void waitForDisplay() {
  turnOffLeds();
  curState = WAIT_FOR_DISPLAY;
  timer = millis() + 1000 + (random() % 1000);
}

void displayPattern() {
  randomizeLeds();
  curState = DISPLAY_PATTERN;
  timer = millis() + t1;
}

void inputPattern() {
  turnOffLeds();
  curState = INPUT_PATTERN;
  timer = millis() + t2;
}

void gameOver() {
  curState = GAME_OVER;
  timer =  millis() + 10000;
}

void givePenalty() {
  penalty++;
  Serial.println("Penalty!");
  digitalWrite(R_LED, HIGH);
  curState = PENALTY;
  timer = millis() + 1000;  
}

void turnOffLeds() {
  for (int x = 0; x < 4; x++) { // turn off the leds
    digitalWrite(LED(x), LOW);
  }
}


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
  
// This implementation uses predefined states to describe every possible game condition.
// Every state has it's own code describing how the game should interact with the player and LEDs in the loop.
enum State {WAIT_FOR_INPUT, SLEEP, DISPLAY_PATTERN, INPUT_PATTERN, PENALTY, WAIT_FOR_DISPLAY, GAME_OVER};

// Current state is stored in the curState variable, and the default state is idle waiting for player input.
State curState = WAIT_FOR_INPUT;
int penalty = 0;
int score = 0;
int level = 1;
int difficulty = 1;
int pattern = 0;
int guess = 0;
long timer; // This is the timer used by every state that needs to change after a certain delay (almost all of them)
int t1 = 4000; // T1 is the amount of time the pattern is displayed, starts at 4 seconds
int t2 = 8000; // T2 is the amount of time the player has to input the correct pattern, starts at 8 seconds

float pulse, pulseDirection;

bool buttonState[4]; // buttons state
int button1State = 0; // 0-> LOW, 1-> RISING, 2-> HIGH
int button1Debounce = 0;

void setup() {
  Serial.begin(9600);
  randomSeed(analogRead(A1));
  // Initialize pins
  for (int i=0; i<4; i++) {
    pinMode(BUTTON(i), INPUT_PULLUP);
    enableInterrupt(BUTTON(i), buttonInterrupt, RISING); // Attach interrupt handler to buttons
    pinMode(LED(i), OUTPUT);
  }
  // Start game
  waitForInput();
}

void loop() {
  // Update buttons 2 -> 4
  for (int i = 1; i < 4; i++) {
    buttonState[i] = !((bool)digitalRead(BUTTON(i)));
  }
  // Button 1 has a debouncer to prevent instantly starting the game after waking from sleep
  if (buttonState[0] == (bool)digitalRead(BUTTON(0))) {
    button1Debounce = millis();
    buttonState[0] = !((bool)digitalRead(BUTTON(0)));
  }
  if ((millis() - button1Debounce) > 50) {
    buttonState[0] = !((bool)digitalRead(BUTTON(0)));
  }
  
  // This is used to start the game on button T1 press
  if (buttonState[0] && button1State == 0 ) {
    button1State = 1; // T1 pressed
  } else if (buttonState[0] && button1State == 1) {
    button1State = 2; // T1 held
  } else if (!buttonState[0]) {
    button1State = 0; // T1 released
  }
  
  // Change loop behaviour depending on the game state.
  switch (curState) {
    case WAIT_FOR_INPUT:
      // This is the default state. Waiting 10 seconds goes into sleep mode, and pressing button T1 starts the game.
      if (timer >= millis()) { // timer = 10 seconds
        // Led Pulse
        pulse += pulseDirection;
        if (pulse >= 255 || pulse <= 0) pulseDirection *= -1;
        analogWrite(R_LED, (int)pulse);

        // Button T1 pressed
        if (button1State == 1) {
          startGame();
        }
      } else {
        sleep();
      }
      break;
    case SLEEP:
      // Wait for interrupt
      break;
    case DISPLAY_PATTERN:
      // This state lasts for t1 milliseconds and displays the pattern to the player
      // Pressing a button here results in a penalty
      for (int x = 0; x < 4; x++) {
        if (buttonState[x]) { // If player presses a button now, give penalty
          givePenalty();
        }        
      }
      if (millis() > timer) { // timer = t1
        inputPattern();
      }
      break;
    case INPUT_PATTERN:
      // This state lasts for t2 milliseconds and is used to collect player input to recreate the pattern
      for (int x = 0; x < 4; x++) {
        if (buttonState[x]) { // If player presses a button now,
          guess | 1 << x;     // Update the player's "guess" binary pattern
          digitalWrite(LED(x), HIGH); // And turn on the appropriate LED
        }
      }
      if (millis() > timer) { // timer = t2
        if (guess == pattern) { // Player guesses right, add score, increase difficulty and return to WAIT_FOR_DISPLAY state
          level++;
          score += level * difficulty; // Score awarded is higher each level, and is increased for higher difficulties
          // Decrease time for display and input by a value F, calculated using the game difficulty
          float F = 1 - ((7.5 * (difficulty + 1)) / 100); // Time is decreased by 15% / 22.5% / 30% / 37.5%
          t1 *= F;
          t2 *= F;
          Serial.print("New point! Score: ");
          Serial.println(score);
          waitForDisplay();
        } else { // Player guesses wrong, give penalty (giving a penalty returns the state to WAIT_FOR_DISPLAY)
          givePenalty();
        }
      }
      break;
    case PENALTY:
      if (millis() > timer) { // timer = 1 second
        // If player has reached 3 penalties, the game ends
        if (penalty >= N_PENALTIES) {
          Serial.print("Game Over! Final score: ");
          Serial.println(score);
          gameOver();
        } else { // Otherwise, return to WAIT_FOR_DISPLAY state
          digitalWrite(R_LED, LOW);
          waitForDisplay();
        }
      }
      break;
    case WAIT_FOR_DISPLAY:
      // This state lasts for 1 ~ 2 seconds before generating a new pattern and going to DISPLAY_PATTERN state
      if (millis() > timer) { // timer = 1 second
        displayPattern();        
      }
      break;
    case GAME_OVER:
      // The game has ended. Wait 10 seconds before returning to default state
      if (millis() > timer) { // timer = 10 seconds
        waitForInput();
      }
      break;
  }
}

void randomizeLeds() {
  pattern = random(); // Generate random 4 digit binary pattern (Es: 1011)
  pattern = (pattern % 15) + 1; // Pattern should go from 0001 to 1111
  // Turn on green LEDs to match pattern
  for (int x = 0; x < 4; x++) {
    int mask = 1 << x;
    digitalWrite(LED(x), pattern & mask);
  }
}

void sleep() {
  curState = SLEEP;
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  turnOffLeds();
  digitalWrite(R_LED, LOW);
  sleep_mode();
  // Wait for interrupt, then return to default state
  waitForInput();  
}

// Empty interrupt function, used to wake up the microcontroller from sleep
void buttonInterrupt() {}

// Calling this method returns to the default state of waiting for a player input to start the game
void waitForInput() {
  curState = WAIT_FOR_INPUT;
  Serial.println("Welcome to the Catch the Light Pattern Game! Press Key T1 to Start.");
  timer = 10000 + millis(); // After 10 seconds switch to sleep mode
  pulse = 0; // Reset red LED pulse parameters
  pulseDirection = PULSE_SPEED;
}

// Print message, turn off red LED, set difficulty for the game and reset game variables
void startGame() {
  Serial.println("Go!");
  digitalWrite(R_LED, LOW);
  difficulty = (analogRead(POT) / 255) + 1;
  penalty = 0;
  level = 0;
  score = 0;
  t1 = 4000;
  t2 = 8000;
  waitForDisplay(); // Then switch to WAIT_FOR_DISPLAY state to start the game loop
}

// Wait a random time between 1 and 2 seconds before displaying pattern
void waitForDisplay() {
  turnOffLeds();
  guess = 0;
  curState = WAIT_FOR_DISPLAY;
  timer = millis() + 1000 + (random() % 1000);
}

// Generate random pattern, then switch to DISPLAY_PATTERN state for t1 milliseconds
void displayPattern() { 
  randomizeLeds();
  curState = DISPLAY_PATTERN;
  timer = millis() + t1;
}

// Turn off LEDS, switch to INPUT_PATTERN state for t2 milliseconds
void inputPattern() { 
  turnOffLeds();
  curState = INPUT_PATTERN;
  timer = millis() + t2;
}

// When ending the game wait 10 seconds before returning to WAIT_FOR_INPUT state
void gameOver() {
  digitalWrite(R_LED, LOW);
  turnOffLeds();
  curState = GAME_OVER;
  timer =  millis() + 10000;
}

// Add penalty count, print, turn on red LED and turn off the others, then switch to PENALTY state
void givePenalty() { 
  penalty++;
  Serial.println("Penalty!");
  digitalWrite(R_LED, HIGH);
  turnOffLeds();
  curState = PENALTY;
  timer = millis() + 1000; // Penalty state should last 1 second
}

// Turn off the leds
void turnOffLeds() {
  for (int x = 0; x < 4; x++) {
    digitalWrite(LED(x), LOW);
  }
}


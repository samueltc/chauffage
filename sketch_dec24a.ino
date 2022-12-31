#include <SimpleCLI.h>

#include <stdarg.h>

#define SERIAL_PRINTF_MAX_BUFF      256

#define RELAY_1_PIN 7
#define RELAY_2_PIN 6
#define RELAY_3_PIN 5
#define RELAY_4_PIN 4

#define THERMOSTAT_1_PIN 9
#define THERMOSTAT_2_PIN 8

void serialPrintf(const char *fmt, ...);
void serialPrintf(const char *fmt, ...) {
  /* Buffer for storing the formatted data */
  char buff[SERIAL_PRINTF_MAX_BUFF];
  /* pointer to the variable arguments list */
  va_list pargs;
  /* Initialise pargs to point to the first optional argument */
  va_start(pargs, fmt);
  /* create the formatted data and store in buff */
  vsnprintf(buff, SERIAL_PRINTF_MAX_BUFF, fmt, pargs);
  va_end(pargs);
  Serial.print(buff);
}

struct Thermostat {
   int id;
   int state = LOW;
   int previous_state = LOW;
};

class Relay {
  private:
    byte pin;
    unsigned long delay;
    unsigned long time_marker;
    int next_state;
  public:
    Relay(byte pin) {
      this->pin = pin;
      init();
    }
    int state() {
      return digitalRead(pin);
    }
    void init() {
      pinMode(pin, OUTPUT);
      off();
      next_state = 0;
    }
    void on() {
      digitalWrite(pin, HIGH);
      serialPrintf("on!\n");
    }
    void off() {
      digitalWrite(pin, LOW);
      serialPrintf("off!\n");
    }
    void start(unsigned long delay) {
      if (next_state == 1) {
        serialPrintf("already scheduled to start\n");
        return;
      }
      time_marker = millis();
      this->delay = delay;
      next_state = 1;
      serialPrintf("will start in %lu ms\n", delay);
    }
    void stop(unsigned long delay) {
      if (next_state == 0) {
        serialPrintf("already scheduled to stop\n");
        return;
      }
      time_marker = millis();
      this->delay = delay;
      next_state = 0;
      serialPrintf("will stop in %lu ms\n", delay);
    }
    void update() {
      //int state = digitalRead(pin);

      if (next_state == state()) {
        return;
      }

      unsigned long now = millis();
      unsigned long elapsed = now - time_marker;
      if (next_state == 1 && elapsed >= delay) {
        on();
      } else if (next_state == 0 && elapsed >= delay) {
        off();
      }
    }
};

Relay circulator1(RELAY_1_PIN);
Relay circulator2(RELAY_2_PIN);
Relay boiler(RELAY_3_PIN);
Relay relay4(RELAY_4_PIN);

Thermostat T1;
Thermostat T2;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(19200);
  delay(100);
  serialPrintf("ok\n");

  T1.id = 1;
  T2.id = 2;

  pinMode(THERMOSTAT_1_PIN, INPUT);
  //pinMode(THERMOSTAT_2_PIN, INPUT);
}

void loop() {
  T1.state = digitalRead(THERMOSTAT_1_PIN);
  T2.state = digitalRead(THERMOSTAT_2_PIN);

  circulator1.update();
  circulator2.update();
  boiler.update();
  relay4.update();

  if (T1.state != T1.previous_state) {
    serialPrintf("state of %i changed from %i -> %i\n", T1.id, T1.previous_state, T1.state);
    T1.previous_state = T1.state;
    if (T1.state == HIGH) {
      circulator1.start(1*1000);
    } else {
      circulator1.stop(5*1000);
    }
  }

  if (T2.state != T2.previous_state) {
    serialPrintf("state of %i changed from %i -> %i\n", T2.id, T2.previous_state, T2.state);
    T2.previous_state = T2.state;
    if (T2.state == HIGH) {
      circulator2.start(1*1000);
    } else {
      circulator2.stop(5*1000);
    }
  }
  if ((T1.state == HIGH || T2.state == HIGH) && boiler.state() == LOW) {
    boiler.start(1*5000);
  } else if ((T1.state == LOW && T2.state == LOW) && boiler.state() == HIGH) {
    boiler.stop(1*1000);
  }
}

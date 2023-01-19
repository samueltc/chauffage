#include <SimpleCLI.h>
#include <stdarg.h>
#include <EEPROM.h>

#define SERIAL_PRINTF_MAX_BUFF 256
#define DAY 86400
#define HOUR 3600
#define MINUTE 60

#define EEPROM_CONFIG_ADDRESS 0
#define RELAY_1_PIN 7
#define RELAY_2_PIN 6
#define RELAY_3_PIN 5
#define RELAY_4_PIN 4

#define THERMOSTAT_1_PIN 8
#define THERMOSTAT_2_PIN 9
#define THERMOSTAT_3_PIN 10

#define DEFAULT_CIRCULATOR_START 1
#define DEFAULT_CIRCULATOR_STOP 5
#define DEFAULT_BOILER_START 2
#define DEFAULT_BOILER_STOP 1

SimpleCLI cli;

Command set;

unsigned long uptime_time_ref = 0;
unsigned long uptime = 0;

struct Config {
  int circulator_start = 5;
  int circulator_stop = 5;
  int boiler_start = 10;
  int boiler_stop = 1;
};

Config config;

void info(const char *fmt, ...) {
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

void(* resetFunc) (void) = 0;

void helpCallback(cmd* c) {
  info("Help:");
  info(cli.toString().c_str());
}

void resetCallback(cmd* c) {
  Serial.println("Resetting now");
  resetFunc();
}

void uptimeCallback(cmd* c) {
  //char buffer[40];
  int days = uptime / DAY;
  int hours = (uptime % DAY) / HOUR;
  int minutes = ((uptime % DAY) % HOUR) / MINUTE;
  int seconds = ((uptime % DAY) % HOUR) % MINUTE;

  //sprintf(buffer, );
  info("%id %ih %im %is", days, hours, minutes, seconds);
}

void getCallback(cmd* c) {
  Serial.print("circulator_start = ");
  Serial.println(config.circulator_start);
  Serial.print("circulator_stop = ");
  Serial.println(config.circulator_stop);
  Serial.print("boiler_start = ");
  Serial.println(config.boiler_start);
  Serial.print("boiler_stop = ");
  Serial.println(config.boiler_stop);
}

// Callback function to set boiler/circ. start/stop delay
void setCallback(cmd* c) {
  Command cmd(c); // Create wrapper object

  // Get arguments
  Argument variableArg = cmd.getArgument("variable");
  Argument valueArg = cmd.getArgument("value");

  // Get values
  String variable = variableArg.getValue();
  int value = valueArg.getValue().toInt();

  //serialPrintf("%s, %i, %i\n", variable, count, scale);
  if (strcmp(variable.c_str(), "circulator_start") == 0) {
    config.circulator_start = value;
  } else if (strcmp(variable.c_str(), "circulator_stop") == 0) {
    config.circulator_stop = value;
  } else if (strcmp(variable.c_str(), "boiler_start") == 0) {
    config.boiler_start = value;
  } else if (strcmp(variable.c_str(), "boiler_stop") == 0) {
    config.boiler_stop = value;
  } else {
    info("noop\n");
  }
  info("Writing config to EEPROM\n");
  EEPROM.put(EEPROM_CONFIG_ADDRESS, config);
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
    int next_state = 0;
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
    }
    void on() {
      digitalWrite(pin, HIGH);
      info("relay %i is on!\n", this->pin);
    }
    void off() {
      digitalWrite(pin, LOW);
      info("relay %i is off!\n", this->pin);
    }

    void change(int state, unsigned long delay) {
        if(state == this->next_state) {
          return;
        }
        this->delay = delay * 1000;
        this->time_marker = millis(); 
        this->next_state = state;
        if (state == this->state()) {
          info("relay %i bounce detected, state (%i) won't change\n", this->pin, this->state());
          return;
        }
        info("relay %i state (%i -> %i) will change in %lu secs \n", this->pin, this->state(), state, delay);
    }

    void start(unsigned long delay) {
      this->change(HIGH, delay);
    }

    void stop(unsigned long delay) {
      this->change(LOW, delay);
    }

    void update() {
      if (this->next_state == state()) {
        return;
      }

      unsigned long now = millis();
      unsigned long elapsed = now - time_marker;

      if (next_state == HIGH && elapsed >= delay) {
        on();
      } else if (next_state == LOW && elapsed >= delay) {
        off();
      }
    }
};

Relay circulator1(RELAY_1_PIN);
Relay circulator2(RELAY_2_PIN);
Relay boiler(RELAY_3_PIN);

Thermostat T1;
Thermostat T2;
Thermostat T3;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  info("booting...\n");

  circulator1.off();
  circulator2.off();
  boiler.off();

  EEPROM.get(EEPROM_CONFIG_ADDRESS, config);
  if (config.circulator_start == -1) {
    config.circulator_start = DEFAULT_CIRCULATOR_START;
    config.circulator_stop = DEFAULT_CIRCULATOR_STOP;
    config.boiler_start = DEFAULT_BOILER_START;
    config.boiler_stop = DEFAULT_BOILER_STOP;
  }

  T1.id = 1;
  T2.id = 2;
  T3.id = 3;

  pinMode(THERMOSTAT_1_PIN, INPUT);
  pinMode(THERMOSTAT_2_PIN, INPUT);
  pinMode(THERMOSTAT_3_PIN, INPUT);

  set = cli.addCommand("set", setCallback);
  set.addPositionalArgument("variable");
  set.addPositionalArgument("value");
  cli.addCommand("get", getCallback);
  cli.addCommand("help", helpCallback);
  cli.addCommand("reset", resetCallback);
  cli.addCommand("uptime", uptimeCallback);
}

void uptime_tick() {
  if(millis() - uptime_time_ref > 1000){
    uptime_time_ref = millis();
    uptime += 1;
  }  
}

void loop() {
  control_loop();
  uptime_tick();

  if (Serial.available()) {
    // Read out string from the serial monitor
    String input = Serial.readStringUntil('\n');

    // Echo the user input
    Serial.print("# ");
    Serial.println(input);

    // Parse the user input into the CLI
    cli.parse(input);
  }

  if (cli.errored()) {
    CommandError cmdError = cli.getError();
    Serial.print("ERROR: ");
    Serial.println(cmdError.toString());

    if (cmdError.hasCommand()) {
      Serial.print("Did you mean \"");
      Serial.print(cmdError.getCommand().toString());
      Serial.println("\"?");
    }
  }
}

void control_loop() {
  T1.state = digitalRead(THERMOSTAT_1_PIN);
  T2.state = digitalRead(THERMOSTAT_2_PIN);

  circulator1.update();
  circulator2.update();
  boiler.update();

  if (T1.state == HIGH) {
    circulator1.start(config.circulator_start);
  } else {
    circulator1.stop(config.circulator_stop);
  }

  if (T2.state == HIGH) {
    circulator2.start(config.circulator_start);
  } else {
    circulator2.stop(config.circulator_stop);
  }

  if ((T1.state == HIGH || T2.state == HIGH)) {
    boiler.start(config.boiler_start);
  } else if ((T1.state == LOW && T2.state == LOW)) {
    boiler.stop(config.boiler_stop);
  }
}

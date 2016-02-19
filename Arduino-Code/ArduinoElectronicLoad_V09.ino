//**************************************************
/*
  Author: Habid Rascon-Ramos
  Project: Arduino Electronic Load
  Date: February 18, 2016
  Version: 9
  Description: This project is a simple electronic load using the Arduino
*/
//***************************************************

// Include libraries
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <Adafruit_INA219.h>

Adafruit_MCP4725 dac; // Declare DAC
Adafruit_INA219 ina219; // Declare INA219 current sensor

// Define all pins
#define SELPIN 10 // Selection pin 
#define DATAOUTPIN 11 // MOSI pin 
#define DATAINPIN  12 // MISO pin
#define SPICLOCKPIN  13 // Clock pin 
#define FANCNTRLPIN 5 // Fan control pin
#define OVRLDCNTRLPIN 6 // Overload control pin
#define LEDPIN 3 // LED pin
#define THERMISTORPIN A0 // Thermistor pin 

// Set integer values
int readAdcValue; // Read value from ADC
int dacValueBuffer; // Temporary DAC value
int intdacValue; // DAC value

// Set float values
float readVoltage; // Read voltage value from source
float dacValue; // DAC value
float readCurrent; // Sense current value from current sensor
float temperatureCelsius; // Read temperature value in Celsius
float temperatureFahrenheit; // Read temperature value in Farenheit
float errorSetpoint; // Temporary setpoint current value
float bufferSetpointCurrent; // Temporary setpoint current value
float tempSetpointCurrent; // Temporary setpoint current value
float setPointCurrent; // Setpoint current value
float CURRENT_THRESHOLD = 2000.0; // Current threshold value
float VOLTAGE_THRESHOLD = 25.0; // Voltage threshold value
float TEMPERATURE_THRESHOLD = 65.0; // Temperature threshold value
float CURRENT_DIFFERENCE = 30.0; // Difference between sepoint and sense current values
float CURRENT_MINIMUM = 3.0; // Minimum current value in mA
float seconds = 0; // Seconds
float milliseconds = 0; // Milliseconds
float countMilliseconds = 0; // Milliseconds counter

// Set constant float values for calculating temperature using thermistor
const float THERMISTOR_B = 3380;
const float THERMISTOR_T0 = 298.15;
const float THERMISTOR_R0 = 10000;
const float THERMISTOR_RS = 10000;

// Set timer values
typedef unsigned char byte;
unsigned long BASE_INTERVAL = 3000; // Default timer set at 3s
unsigned long baseLastIntervalStart = 1000; // Timer interval tracker
unsigned long load_timer_last_interval_start = 0; // Timer interval tracker
unsigned long timerInterval = 5000; // Timer interval
unsigned long userShowRate = 5000; // Display rate in milliseconds
unsigned long logShowRate = 1000; // Log rate in milliseconds

// Set boolean values
boolean mismatchFlag = true;
boolean cmdFlag = false;

// Set string values
String userInput = "";
String cmdType = "";
String cmdSubType = "";
String cmdBody = "";

enum {
  // States
  STANDBY = 1,
  DUMMY_STATE,
  MEASUREMENT_SETUP,
  DATA_LOG,
  VOLTAGE_OVERLOAD,
  CURRENT_ERROR,
  COOL_DOWN

} STATES;

enum {
  // Events
  START = 0,
  HELP,
  SEND,
  LOG,
  TEMP,
  OVERTEMP,
  VOLT,
  OVERVOLTAGE,
  OVERCURRENT,
  DIFFERENCE,
  TEST,
  SHOW,
  DISPLAYRATE,
  LOGRATE,
  END,
  NONE
} EVENTS;

enum {
  // Error states
  NO_ERROR = 1,
  DUMMY_ERROR,
  CURRENT_OVERLOAD,
  CURRENT_MISMATCH
} CURRENT_ERROR_STATES;

// Set byte values
byte state = STANDBY; // Tracks current state
byte prevState = DUMMY_STATE; // Dummy state
byte event = NONE; // Triggers a particular action within current state
byte error = NO_ERROR; // Tracks current error state
byte prevError = DUMMY_ERROR; // Dummy error state

// Updates logging timer
void updatePeriodicTimers () {
  milliseconds = millis() - baseLastIntervalStart;
  countMilliseconds = countMilliseconds + milliseconds;
  seconds = countMilliseconds / 1000.0;

}

// Updates triggered timers
void updateIntervalTimers () {
  if ((millis() - load_timer_last_interval_start > timerInterval) && state != STANDBY) {
    load_timer_last_interval_start = millis();
    event = SHOW;
  }
}

// State Machine
void updateState() {
  switch (state) {

    case STANDBY:
      if (prevState != state) {
        //Entry event
        setPointCurrent = 0;
        tempSetpointCurrent = 0;
        intdacValue = 0;
        dacValueBuffer = 0;
        dac.setVoltage(intdacValue, false);
        Serial.println(F(""));
        Serial.println(F("Starting STANDBY state"));
        event = NONE;
        prevError = DUMMY_ERROR;
        prevState = state;
      }

      if (event == SEND) {
        Serial.println(F(""));
        Serial.print(F("The setpoint current is now: "));
        Serial.print(setPointCurrent);
        Serial.println(F(" mA"));
        event = NONE;
      }

      if (event == OVERVOLTAGE) {
        state = VOLTAGE_OVERLOAD;
        event = NONE;
      }

      if (event == START) {
        state = MEASUREMENT_SETUP;
        event = NONE;
      }
      break;

    case MEASUREMENT_SETUP:
      intdacValue = dacValueBuffer;
      dac.setVoltage(intdacValue, false);
      mismatchFlag = true;

      if (prevState != state) {
        Serial.println(F(""));
        Serial.print(F("The setpoint current is: "));
        Serial.print(setPointCurrent);
        Serial.println(F(" mA"));
        if (setPointCurrent < CURRENT_MINIMUM) {
          Serial.println(F("The load is receiving current that is less than minimum measured amount"));
          Serial.print(F("The minimum measured amount is: "));
          Serial.print(CURRENT_MINIMUM);
          Serial.println(F(" mA"));
        }
        Serial.println(F(""));
        Serial.println(F("Starting Measurement."));
        Serial.print(F("The current display rate is: "));
        Serial.print(userShowRate);
        Serial.println(F(" milliseconds"));
        timerInterval = userShowRate;
        event = NONE;
        prevState = state;
      }

      if (event == SEND) {
        Serial.println(F(""));
        Serial.print(F("The setpoint current is now: "));
        Serial.println(setPointCurrent);
        if (setPointCurrent < CURRENT_MINIMUM) {
          Serial.println(F(""));
          Serial.println(F("The load is receiving current that is less than minimum measured amount"));
          Serial.print(F("The minimum measured amount is: "));
          Serial.print(CURRENT_MINIMUM);
          Serial.println(F(" mA"));
        }
        event = NONE;
      }

      if (event == OVERVOLTAGE) {
        state = VOLTAGE_OVERLOAD;
        event = NONE;
      }

      if (event == OVERCURRENT) {
        error = CURRENT_OVERLOAD;
        state = CURRENT_ERROR;
        event = NONE;
      }

      if (event == DIFFERENCE) {
        error = CURRENT_MISMATCH;
        state = CURRENT_ERROR;
        event = NONE;
      }

      if (event == OVERTEMP) {
        Serial.println(F(""));
        Serial.println(F("ERROR!"));
        Serial.println(F("The temperature is out of range!"));
        state = COOL_DOWN;
        event = NONE;
      }

      if (event == LOG) {
        state = DATA_LOG;
        event = NONE;
      }

      if (event == END) {
        Serial.println(F(""));
        Serial.println(F("Ending measurement."));
        Serial.println(F("Returning to STANDBY state."));
        state = STANDBY;
        event = NONE;
      }

      if (event == SHOW) {
        Serial.println(F(""));
        Serial.print(F("Load Current:   "));
        if (readCurrent < CURRENT_MINIMUM) {
          Serial.print(F("Below minimum measured amount"));
        }
        else {
          Serial.print(readCurrent);
        }
        Serial.println(F(" mA"));
        Serial.print(F("Source Voltage: "));
        Serial.print(readVoltage);
        Serial.println(F(" V"));
        event = NONE;
      }
      break;

    case DATA_LOG:
      if (prevState != state) {
        Serial.println(F(""));
        Serial.println(F("Starting logging process."));
        Serial.print(F("The current log rate is: "));
        Serial.print(logShowRate);
        Serial.println(" milliseconds");
        Serial.println(F(""));
        Serial.print(F("Time(seconds): ")); Serial.print(F(" Load Current(mA):")); Serial.println(F("  Source Voltage(V):"));
        timerInterval = logShowRate;
        seconds = 0;
        milliseconds = 0;
        countMilliseconds = 0;
        event = NONE;
        prevState = state;
      }

      if (event == OVERVOLTAGE) {
        state = VOLTAGE_OVERLOAD;
        event = NONE;
      }

      if (event == OVERCURRENT) {
        error = CURRENT_OVERLOAD;
        state = CURRENT_ERROR;
        event = NONE;
      }

      if (event == DIFFERENCE) {
        error = CURRENT_MISMATCH;
        state = CURRENT_ERROR;
        event = NONE;
      }

      if (event == OVERTEMP) {
        Serial.println(F(""));
        Serial.println(F("ERROR!"));
        Serial.println(F("The temperature is out of range!"));
        state = COOL_DOWN;
        event = NONE;
      }

      if (event == END) {
        seconds = 0;
        milliseconds = 0;
        countMilliseconds = 0;
        Serial.println(F(""));
        Serial.println(F("Ending measurement."));
        Serial.println(F("Returning to STANDBY state."));
        state = STANDBY;
        event = NONE;
      }


      if (event == SHOW) {
        Serial.print(seconds);
        Serial.print(F(", "));
        if (readCurrent < CURRENT_MINIMUM) {
          Serial.println(F("< 3"));
        }
        else {
          Serial.print(readCurrent);
        }
        Serial.print(F(", "));
        Serial.println(readVoltage);
        event = NONE;
      }
      break;

    case VOLTAGE_OVERLOAD:
      if (prevState != state) {
        dac.setVoltage(0, false);
        Serial.println(F(""));
        Serial.println(F("ERROR!"));
        Serial.println(F("Overload Protection is ON for safety."));
        digitalWrite(LEDPIN, HIGH);
        Serial.println(F(""));
        Serial.println(F("The voltage is too high!"));
        Serial.println(F("Please lower voltage."));
        Serial.println(F("The voltage should be at or below 25V."));
        Serial.println(F(""));
        Serial.print(F("Voltage: "));
        Serial.print(readVoltage);
        Serial.println(" V");
        timerInterval = userShowRate;
        event = NONE;
        prevState = state;
      }
      if (event == SHOW) {
        Serial.println(F(""));
        Serial.print(F("Voltage: "));
        Serial.print(readVoltage);
        Serial.println(" V");
        event = NONE;
      }
      if (readVoltage <= VOLTAGE_THRESHOLD) {
        Serial.println(F(""));
        Serial.println(F("Overload Protection is OFF."));
        digitalWrite(LEDPIN, LOW);
        Serial.println(F(""));
        Serial.println(F("The voltage is in suitable range."));
        Serial.println(F("Returning to STANDBY state."));
        state = STANDBY;
        event = NONE;
      }
      break;

    case CURRENT_ERROR:
      if (prevState != state) {
        digitalWrite(LEDPIN, HIGH);
        if (error == CURRENT_OVERLOAD) {
          dac.setVoltage(0, false);
          Serial.println(F(""));
          Serial.println(F("Error!"));
          Serial.println(F("Overload Protection is ON for safety."));
          Serial.println(F("The current is too high!"));
          Serial.println(F("The sense current should be at or below 2A"));
          Serial.println(F(""));
          Serial.print(F("The sense current before the error occured was: "));
          Serial.print(readCurrent);
          Serial.println(F(" mA"));
        }
        if (error == CURRENT_MISMATCH) {
          Serial.println(F(""));
          Serial.println(F("Error!"));
          Serial.println(F("There is a difference between setpoint and sense currents."));
          Serial.println(F("Please check connections."));
          if (setPointCurrent < CURRENT_MINIMUM) {
            Serial.println(F(""));
            Serial.println(F("The setpoint current is currently below the minimum measured amount."));
            Serial.print(F("The minimum measured amount is: "));
            Serial.print(CURRENT_MINIMUM);
            Serial.println(F(" mA"));
            Serial.print(F("The setpoint current is now: "));
            Serial.print(CURRENT_MINIMUM);
            Serial.println(F(" mA"));
            setPointCurrent = CURRENT_MINIMUM;
            dacValue = (setPointCurrent / 5000.0) * 4095.0 * 1.290;
            dacValueBuffer = int(dacValue);
            dac.setVoltage(dacValueBuffer, false);
            readCurrent = ina219.getCurrent_mA();
          }
        }
        timerInterval = userShowRate;
        event = NONE;
        prevState = state;
      }
      switch (error) {
        case CURRENT_OVERLOAD:
          if (prevError != error) {
            Serial.println(F(""));
            Serial.println(F("Please lower the setpoint current and then test it."));
            Serial.print(F("The setpoint current is currently "));
            Serial.print(setPointCurrent);
            Serial.println(F(" mA"));
            errorSetpoint = setPointCurrent;
            event = NONE;
            prevError = error;
          }
          if (event == TEST) {
            if (setPointCurrent < CURRENT_MINIMUM) {
              Serial.println(F(""));
              Serial.print(F("Please enter a setpoint current at or above "));
              Serial.print(CURRENT_MINIMUM);
              Serial.println(" mA.");
              event = NONE;
              break;
            }

            if (errorSetpoint <= setPointCurrent) {
              Serial.println(F(""));
              Serial.print(F("Please lower the setpoint current below "));
              Serial.print(errorSetpoint);
              Serial.println(F(" mA."));
              event = NONE;
              break;
            }
            else {
              intdacValue = dacValueBuffer;
              dac.setVoltage(intdacValue, false);
              Serial.println(F(""));
              Serial.println(F("Overload Protection is OFF."));
              readCurrent = ina219.getCurrent_mA();
              Serial.println(F(""));
              Serial.print(F("The sense current is: "));
              Serial.print(readCurrent);
              Serial.println(" mA");
            }

            if (readCurrent <= CURRENT_THRESHOLD) {
              Serial.println(F("The sense current is in suitable range."));
              Serial.println(F("Returning to STANDBY state."));
              digitalWrite(LEDPIN, LOW);
              state = STANDBY;
              error = NO_ERROR;
            }
            else {
              dac.setVoltage(0, false);
              Serial.println(F(""));
              Serial.println(F("Overload Protection is ON for safety."));
              Serial.println(F("Current is still too high!"));
              Serial.println(F("Please lower setpoint current and test it again."));
              errorSetpoint = setPointCurrent;
            }
            event = NONE;
          }

          if (event == SEND) {
            Serial.println(F(""));
            Serial.print(F("The setpoint current is now: "));
            Serial.print(setPointCurrent);
            Serial.println(F(" mA"));
            event = NONE;
          }

          if (event == SHOW) {
            Serial.println(F(""));
            Serial.print(F("Sense current: "));
            Serial.print(readCurrent);
            Serial.println(F(" mA"));
            event = NONE;
          }
          break;

        case CURRENT_MISMATCH:
          if (setPointCurrent < CURRENT_MINIMUM) {
            intdacValue = bufferSetpointCurrent;
            dac.setVoltage(intdacValue, false);
          }
          else {
            intdacValue = dacValueBuffer;
            dac.setVoltage(intdacValue, false);
          }

          if (prevError != error) {
            Serial.println(F(""));
            Serial.println(F("Try lowering setpoint current to a small value e.g. 10mA"));
            Serial.println(F(""));
            Serial.print(F("Setpoint Current: "));
            Serial.print(setPointCurrent);
            Serial.println(F(" mA"));
            Serial.print(F("Sense Current: "));
            Serial.print(readCurrent);
            Serial.println(F(" mA"));
            bufferSetpointCurrent = setPointCurrent;
            event = NONE;
            prevError = error;
          }

          if (abs(setPointCurrent - readCurrent) <= CURRENT_DIFFERENCE) {
            Serial.println(F("Both setpoint and sense current values match."));
            Serial.println(F("Returning to STANDBY state."));
            digitalWrite(LEDPIN, LOW);
            event = NONE;
            state = STANDBY;
            error = NO_ERROR;
          }

          if (event == SEND) {
            if (setPointCurrent < CURRENT_MINIMUM) {
              Serial.println(F(""));
              Serial.print(F("Please enter a setpoint current at or above "));
              Serial.print(CURRENT_MINIMUM);
              Serial.println(F(" mA"));
              setPointCurrent = bufferSetpointCurrent;
              dacValue = (setPointCurrent / 5000.0) * 4095.0 * 1.290;
              dacValueBuffer = int(dacValue);
            }
            else {
              bufferSetpointCurrent = setPointCurrent;
              Serial.println(F(""));
              Serial.print(F("The setpoint current is now: "));
              Serial.print(setPointCurrent);
              Serial.println(F( "mA"));
            }
            event = NONE;
          }

          if (event == SHOW) {
            Serial.println(F(""));
            Serial.print(F("Setpoint Current: "));
            Serial.print(setPointCurrent);
            Serial.println(F(" mA"));
            Serial.print(F("Sense Current: "));
            Serial.print(readCurrent);
            Serial.println(F(" mA"));
            event = NONE;
          }
          break;

      }
      break;

    case COOL_DOWN:
      if (prevState != state) {
        dac.setVoltage(0, false);
        Serial.println(F(""));
        Serial.println(F("Overload Protection is ON for safety."));
        digitalWrite(LEDPIN, HIGH);
        digitalWrite(FANCNTRLPIN, HIGH);
        Serial.println(F("Fan is ON to cool down heat sink."));
        Serial.println(F(""));
        Serial.print(F("Cooling Down until temperature reaches "));
        Serial.print((TEMPERATURE_THRESHOLD * 9.0) / 5.0 + 32.0);
        Serial.print(F(" Fahrenheit"));
        Serial.print(F(" and "));
        Serial.print(TEMPERATURE_THRESHOLD);
        Serial.print(F(" Celsius."));
        Serial.println(F("Please wait."));
        Serial.println(F(""));
        Serial.print(F("The current temperature is: "));
        Serial.print(temperatureFahrenheit);
        Serial.print(F(" Fahrenheit"));
        Serial.print(F(" and "));
        Serial.print(temperatureCelsius);
        Serial.println(F(" Celsius"));
        timerInterval = userShowRate;
        event = NONE;
        prevState = state;
      }

      if (event == SHOW) {
        Serial.println(F(""));
        Serial.print(F("The current temperature is: "));
        Serial.print(temperatureFahrenheit);
        Serial.print(F(" Fahrenheit"));
        Serial.print(F(" and "));
        Serial.print(temperatureCelsius);
        Serial.println(F(" Celsius"));
        event = NONE;
      }

      if (temperatureCelsius < TEMPERATURE_THRESHOLD) {
        digitalWrite(LEDPIN, LOW);
        digitalWrite(FANCNTRLPIN, LOW);
        Serial.println(F(""));
        Serial.println(F("Fan is OFF"));
        Serial.println(F("Overload Protection is OFF"));
        Serial.println(F("Temperature is in suitable range."));
        Serial.println(F("Returning to STANDBY state."));
        state = STANDBY;
        event = NONE;
      }
      break;

  }

}

// Function to determine if string input is a float
boolean isFloat(String numberInput) {
  boolean decimalPoint = false;

  for (int i = 0; i < numberInput.length(); i++) {
    if (numberInput.charAt(i) == '/')
      return false;
    if (numberInput.charAt(i) == '.')  {
      if (decimalPoint)
        return false;
      else
        decimalPoint = true;
    }
  }
  return true;
}

// Handles parsing command from the command line
void parseCommand (String cmd) {
  String rate;
  float lograte;
  float displayRate;
  int location;
  int index;
  boolean intFlag;
  String number;
  byte section = 0;
  char delimeter = ' ';

  for (index = 0; index < cmd.length(); index++) {
    if (cmd.charAt(index) == delimeter) section++;
    else {
      switch (section) {
        case 0:
          cmdType.concat(cmd.charAt(index));
          break;
        case 1:
          cmdSubType.concat(cmd.charAt(index));
          break;
        case 2:
          cmdBody.concat(cmd.charAt(index));
          break;
      }
    }
  }

  if (cmdType.equals("help")) {
    event = HELP;
  }

  if (cmdType.equals("start")) {
    if (readVoltage <= 0.0 && readCurrent < CURRENT_MINIMUM && readCurrent != setPointCurrent && event != OVERVOLTAGE) {
      Serial.println(F(""));
      Serial.println(F("Source is not connected!"));
      Serial.println(F("Please connect source before starting."));
    }
    else
      event = START;
  }

  if (cmdType.equals("end")) {
    event = END;
  }

  if (cmdType.equals("read")) {
    if (cmdSubType.equals("temperature")) {
      event = TEMP;
    }
    if (cmdSubType.equals("voltage")) {
      event = VOLT;
    }
  }

  if (cmdType.equals("show")) {
    if (cmdSubType.equals("displayrate")) {
      event = DISPLAYRATE;
    }
    if (cmdSubType.equals("lograte")) {
      event = LOGRATE;
    }
  }

  if (cmdType.equals("log")) {
    if (cmdSubType.equals("data")) {
      event = LOG;
    }
  }

  if (cmdType.equals("test")) {
    if (cmdSubType.equals("current")) {
      event = TEST;
    }
  }

  if (cmdType.equals("set")) {
    if (cmdSubType.equals("lograte")) {
      for (int i = 0; i < cmdBody.length(); i++) {
        if (cmdBody.charAt(i) >= 48 && cmdBody.charAt(i) <= 57) {
          intFlag = true;
        }
        else {
          intFlag = false;
          break;
        }
      }
      if (intFlag && (state == STANDBY | state == MEASUREMENT_SETUP | state == DATA_LOG)) {
        rate = cmdBody.substring(0, cmdBody.length());
        lograte = atof(rate.c_str());
        if ((lograte < 100)) {
          Serial.println(F(""));
          Serial.println(F("Log rate is too low!"));
          Serial.println(F("Please enter a log rate greater than or equal to 100 milliseconds."));
          Serial.println(F("Value must be a whole number."));
        }
        else {
          logShowRate = lograte;
          if (state == DATA_LOG) {
            timerInterval = logShowRate;
          }
          Serial.println(F(""));
          Serial.print(F("The log rate is now: "));
          Serial.print((long)logShowRate);
          Serial.println(F(" milliseconds"));
        }
      }
      else if (!intFlag && (state == STANDBY | state == MEASUREMENT_SETUP | state == DATA_LOG)) {
        Serial.println(F(""));
        Serial.println(F("Invalid Input!"));
        Serial.println(F("Please Enter a whole number."));
        Serial.print(F("Value must be greater than or equal to 100 milliseconds."));
      }
      else {
        Serial.println(F(""));
        Serial.println(F("Cannot change log rate in this state!"));
      }
    }

    if (cmdSubType.equals("displayrate")) {
      for (int i = 0; i < cmdBody.length(); i++) {
        if (cmdBody.charAt(i) >= 48 && cmdBody.charAt(i) <= 57) {
          intFlag = true;
        }
        else {
          intFlag = false;
          break;
        }
      }
      if (intFlag && state != DATA_LOG) {
        rate = cmdBody.substring(0, cmdBody.length());
        displayRate = atof(rate.c_str());
        if (displayRate < 100) {
          Serial.println(F(""));
          Serial.println(F("Display rate is too low!"));
          Serial.println(F("Please enter a display rate greater than or equal to 100 milliseconds."));
          Serial.println(F("Value must be a whole number."));
        }
        else {
          userShowRate = displayRate;
          timerInterval = userShowRate;
          Serial.println(F(""));
          Serial.print(F("The display rate is now: "));
          Serial.print((long)userShowRate);
          Serial.println(F(" milliseconds"));
        }
      }
      else if (!intFlag && state != DATA_LOG) {
        Serial.println(F(""));
        Serial.println(F("Invalid Input!"));
        Serial.println(F("Please Enter a whole number."));
        Serial.print(F("Value must be greater than or equal to 100 milliseconds."));
      }
      else {
        Serial.println(F(""));
        Serial.println(F("Cannot change display rate in this state!"));
      }
    }

    if ((cmdSubType.equals("current")) && (state == STANDBY | state == MEASUREMENT_SETUP | state == CURRENT_ERROR)) {
      for (int i = 0; i < cmdBody.length(); i++) {
        if (cmdBody.charAt(i) >= 46 && cmdBody.charAt(i) <= 57) {
        }
        else {
          location = i;
          break;
        }
      }
      if (cmdBody.charAt(location) == 'm' || cmdBody.charAt(location) == 'A') {
        if (cmdBody.substring(location) == "mA") {
          number = cmdBody.substring(0, location);
          if (isFloat(number)) {
            tempSetpointCurrent = atof(number.c_str());
            if (tempSetpointCurrent > 2000) {
              Serial.println(F("Setpoint current is too high!"));
              Serial.println(F("Please enter a current value below 2A."));
            }
            else {
              setPointCurrent = tempSetpointCurrent;
              dacValue = (setPointCurrent / 5000.0) * 4095.0 * 1.290;
              dacValueBuffer = int(dacValue);
              mismatchFlag = false;
            }
            event = SEND;
          }
          else {
            Serial.println(F(""));
            Serial.println(F("Invalid Input!"));
            Serial.println(F("Please Try Again"));
          }
        }
        else if (cmdBody.substring(location) == "A") {
          number = cmdBody.substring(0, location);
          if (isFloat(number)) {
            tempSetpointCurrent = atof(number.c_str());
            tempSetpointCurrent = tempSetpointCurrent * 1000;
            if (tempSetpointCurrent > 2000) {
              Serial.println(F("Setpoint current is too high!"));
              Serial.println(F("Please enter a current value below 2A."));
            }
            else {
              setPointCurrent = tempSetpointCurrent;
              dacValue = (setPointCurrent / 5000.0) * 4095.0 * 1.290;
              dacValueBuffer = int(dacValue);
              mismatchFlag = false;
            }
            event = SEND;
          }
          else {
            Serial.println(F(""));
            Serial.println(F("Invalid Input!"));
            Serial.println(F("Please Try Again"));
          }
        }
        else {
          Serial.println(F(""));
          Serial.println(F("Invalid Input!"));
          Serial.println(F("Please Try Again"));
        }
      }
      else {
        Serial.println(F(""));
        Serial.println(F("Invalid Input!"));
        Serial.println(F("Please Try Again"));
      }
    }
  }

  cmdType = "";
  cmdBody = "";
  cmdSubType = "";
}

// Program startup
void setup() {

  // set pin modes
  pinMode(SELPIN, OUTPUT);
  pinMode(DATAOUTPIN, OUTPUT);
  pinMode(DATAINPIN, INPUT);
  pinMode(SPICLOCKPIN, OUTPUT);
  pinMode(FANCNTRLPIN, OUTPUT);
  pinMode(OVRLDCNTRLPIN, OUTPUT);
  pinMode(LEDPIN, OUTPUT);

  // Set Outputs
  digitalWrite(SELPIN, HIGH);
  digitalWrite(DATAOUTPIN, LOW);
  digitalWrite(SPICLOCKPIN, LOW);
  digitalWrite(FANCNTRLPIN, LOW);
  digitalWrite(OVRLDCNTRLPIN, LOW);
  digitalWrite(LEDPIN, LOW);

  // Display Information to user
  Serial.begin(9600);
  Serial.println(F("Welcome!"));
  Serial.println(F("This is the Arduino Electronic Load Program."));
  Serial.println(F("Electronic Load works in constant current mode."));
  Serial.println(F("Please enter a suitable setpoint current below 2A."));
  Serial.println(F("Enter 'start' command to begin."));
  Serial.println(F("Use the 'help' command for further assistance."));

  dac.begin(0x60); // The I2C Address
  ina219.begin(); // Initialize DAC
  int dacInput = 0; // From 0 to 4095 is from 0V to 5V
  dac.setVoltage(dacInput, false); // Set dac to default value of 0

}

// Read ADC function
int read_adc(int channel) {
  int adcvalue = 0;
  byte commandbits = B11000000; // Command bits: start, mode, chn (3), dont care (3)

  // Channel selection
  commandbits |= ((channel - 1) << 3);

  digitalWrite(SELPIN, LOW); // Select pin for ADC is set to low

  // Setup bits to be written
  for (int i = 7; i >= 3; i--) {
    digitalWrite(DATAOUTPIN, commandbits & 1 << i);

    //Cycle clock
    digitalWrite(SPICLOCKPIN, HIGH);
    digitalWrite(SPICLOCKPIN, LOW);
  }

  // Ignore 2 null bits by cycling clock HIGH and LOW twice
  digitalWrite(SPICLOCKPIN, HIGH);
  digitalWrite(SPICLOCKPIN, LOW);
  digitalWrite(SPICLOCKPIN, HIGH);
  digitalWrite(SPICLOCKPIN, LOW);

  // Read bits from adc
  for (int i = 11; i >= 0; i--) {
    adcvalue += digitalRead(DATAINPIN) << i;

    // Cycle clock
    digitalWrite(SPICLOCKPIN, HIGH);
    digitalWrite(SPICLOCKPIN, LOW);
  }
  digitalWrite(SELPIN, HIGH); // Turn off ADC
  return adcvalue;
}

// Read temperature function
float temperature() {
  int adc = analogRead(THERMISTORPIN);
  temperatureCelsius = 1 / ((log(THERMISTOR_RS * adc / (1024 - adc)) - log(THERMISTOR_R0)) / THERMISTOR_B + 1 / THERMISTOR_T0) - 273.15;
  temperatureFahrenheit = (temperatureCelsius * 9.0) / 5.0 + 32.0;

  if (event == TEMP) {
    Serial.println();
    Serial.print(F("The current temperature is: "));
    Serial.print(temperatureFahrenheit);
    Serial.print(F(" Fahrenheit"));
    Serial.print(F(" and "));
    Serial.print(temperatureCelsius);
    Serial.println(F(" Celsius"));
    delay(3000);
    event = NONE;
  }

  if (temperatureCelsius >= TEMPERATURE_THRESHOLD && state != COOL_DOWN)
    event = OVERTEMP;
  return temperatureCelsius;
}

// Display voltage value from source function
void displayVoltage() {
  if (event == VOLT) {
    Serial.println();
    Serial.print(F("The current voltage is: "));
    Serial.print(readVoltage);
    Serial.println(F(" V"));
    Serial.println();
    delay(3000);
    event = NONE;
  }
}

// Show display rate function
void showDisplayRate() {
  if (event == DISPLAYRATE) {
    Serial.println();
    Serial.print(F("The current display rate is: "));
    Serial.print(userShowRate);
    Serial.println(F(" milliseconds"));
    Serial.println();
    delay(3000);
    event = NONE;
  }
}

// Show log rate function
void showLogRate() {
  if (event == LOGRATE) {
    Serial.println();
    Serial.print(F("The current log rate is: "));
    Serial.print(logShowRate);
    Serial.println(F(" milliseconds"));
    Serial.println();
    delay(3000);
    event = NONE;
  }
}

// Help screen function
void helpscreen() {
  if (event == HELP) {
    Serial.println(F(""));
    Serial.println(F("Displaying Commands:"));
    if (state == STANDBY) {
      Serial.println(F("start: Starts the measurement of current and voltage."));
    }
    if (state == STANDBY | state == MEASUREMENT_SETUP | state == CURRENT_ERROR) {
      Serial.println(F("set current: Set the setpoint current in A or mA e.g. 'set current 1000mA' or 'set current 1A'"));
    }
    if (state == MEASUREMENT_SETUP) {
      Serial.println(F("log data: Logs the current and voltage with a timestamp using a log rate."));
    }
    Serial.println(F("read temperature: Reads the current temperature of the heatsink."));
    if (state != MEASUREMENT_SETUP | state != DATA_LOG) {
      Serial.println(F("read voltage: Reads the current voltage of the source."));
    }
    if (state != DATA_LOG) {
      Serial.println(F("set displayrate: Set the rate at which the current and voltage are displayed on the screen during the measurement e.g. 'set displayrate 1000'."));
    }
    Serial.println(F("show displayrate: Displays the current display rate."));
    if (state == STANDBY | state == MEASUREMENT_SETUP | state == DATA_LOG) {
      Serial.println(F("set lograte: Set the log rate when logging current and voltage with a time stamp e.g. 'set lograte 1000'."));
    }
    Serial.println(F("show lograte: Displays the current log rate."));
    if (state == MEASUREMENT_SETUP | state == DATA_LOG) {
      Serial.println(F("end: Ends the measurement"));
    }
    delay(3000);
    event = NONE;
  }
}

// Main loop
void loop() {
  readAdcValue = read_adc(1);
  readVoltage = float(readAdcValue);
  readVoltage = (readVoltage / 4095) * 2.048 * 13.2;

  if (readVoltage > VOLTAGE_THRESHOLD && state != VOLTAGE_OVERLOAD) {
    event = OVERVOLTAGE;
  }

  readCurrent = ina219.getCurrent_mA();

  if (readCurrent > CURRENT_THRESHOLD && state != CURRENT_ERROR) {
    event = OVERCURRENT;
  }

  if ((abs(setPointCurrent - readCurrent) >= CURRENT_DIFFERENCE) && (state == MEASUREMENT_SETUP || state == DATA_LOG) && mismatchFlag ) {
    event = DIFFERENCE;
  }

  if (readVoltage <= 0.0 && state != STANDBY) {
    Serial.println(F(""));
    Serial.println(F("Source has been disconnected!"));
    Serial.println(F("Returning to STANDBY state."));
    state = STANDBY;
    event = NONE;
  }

  if ((userShowRate < 1000 | userShowRate > 5000) && (state != STANDBY && state != MEASUREMENT_SETUP && state != DATA_LOG)) {
    timerInterval = BASE_INTERVAL;
    userShowRate = timerInterval;
    Serial.println(F(""));
    Serial.println(F("The Display rate is not in a desirable range!"));
    Serial.println(F("The Display rate will be changed to 3000 milliseconds to make debugging easier."));
    Serial.println("The display rate can still be change between 1000 and 5000 milliseconds.");
  }

  if ((millis() - baseLastIntervalStart > logShowRate) && state == DATA_LOG) {
    updatePeriodicTimers ();
    baseLastIntervalStart = millis();
  }

  displayVoltage();
  temperature();
  helpscreen();
  showDisplayRate();
  showLogRate();
  updateState();
  updateIntervalTimers();

  if (Serial.available() > 0) {
    int inByte = Serial.read();
    if (inByte == 13) {  // carriage return
      cmdFlag = true;
      Serial.println(F(""));
      Serial.print(F("Command sent: "));  Serial.println(userInput);
      inByte = 0;
    }
    else {
      userInput.concat(char(inByte));
    }
  }
  if (cmdFlag) {
    parseCommand(userInput);
    userInput = "";
    cmdFlag = false;
  }

}

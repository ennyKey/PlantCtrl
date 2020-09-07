/**
 * @file main.cpp
 * @author Ollo
 * @brief PlantControl
 * @version 0.1
 * @date 2020-05-01
 * 
 * @copyright Copyright (c) 2020
 * 
 */
#include "PlantCtrl.h"
#include "ControllerConfiguration.h"
#include "DS18B20.h"
#include <Homie.h>

const unsigned long TEMPREADCYCLE = 30000; /**< Check temperature all half minutes */

#define AMOUNT_SENOR_QUERYS   8
#define SENSOR_QUERY_SHIFTS   3
#define SOLAR4SENSORS         6.0f
#define TEMP_INIT_VALUE       -999.0f
#define TEMP_MAX_VALUE        85.0f

bool mLoopInited = false;
bool mDeepSleep = false;

bool mPumpIsRunning=false;

int plantSensor1 = 0;

int lipoSenor = -1;
int lipoSensorValues = 0;
int solarSensor = -1;
int solarSensorValues = 0;

int mWaterAtEmptyLevel = 0;
#ifndef HC_SR04
int mWaterLow   = 0;
#else
int mWaterGone = -1;  /**< Amount of centimeter, where no water is seen */
#endif
int mOverflow = 0;

int readCounter = 0;
int mButtonClicks = 0;


#if (MAX_PLANTS >= 1)
HomieNode plant1("plant1", "Plant 1", "Plant");
#endif
#if (MAX_PLANTS >= 2)
HomieNode plant2("plant2", "Plant 2", "Plant");
#endif
#if (MAX_PLANTS >= 3)
HomieNode plant3("plant3", "Plant 3", "Plant");
#endif
#if (MAX_PLANTS >= 4)
HomieNode plant4("plant4", "Plant 4", "Plant");
#endif
#if (MAX_PLANTS >= 5)
HomieNode plant5("plant5", "Plant 5", "Plant");
#endif
#if (MAX_PLANTS >= 6)
HomieNode plant6("plant6", "Plant 6", "Plant");
#endif

HomieNode sensorLipo("lipo", "Battery Status", "Lipo");
HomieNode sensorSolar("solar", "Solar Status", "Solarpanel");
HomieNode sensorWater("water", "WaterSensor", "Water");
HomieNode sensorTemp("temperature", "Temperature", "temperature");

HomieSetting<long> deepSleepTime("deepsleep", "time in milliseconds to sleep (0 deactivats it)");
HomieSetting<long> deepSleepNightTime("nightsleep", "time in milliseconds to sleep (0 usese same setting: deepsleep at night, too)");
HomieSetting<long> wateringTime("watering", "time seconds the pump is running (60 is the default)");
HomieSetting<long> plantCnt("plants", "amout of plants to control (1 ... 6)");

#ifdef  HC_SR04
HomieSetting<long> waterLevel("watermaxlevel", "Water maximum level in centimeter (50 cm default)");
#endif


#if (MAX_PLANTS >= 1)
HomieSetting<long> plant1SensorTrigger("moist1", "Moist1 sensor value, when pump activates");
#endif
#if (MAX_PLANTS >= 2)
HomieSetting<long> plant2SensorTrigger("moist2", "Moist2 sensor value, when pump activates");
#endif
#if (MAX_PLANTS >= 3)
HomieSetting<long> plant3SensorTrigger("moist3", "Moist3 sensor value, when pump activates");
#endif
#if (MAX_PLANTS >= 4)
HomieSetting<long> plant4SensorTrigger("moist4", "Moist4 sensor value, when pump activates");
#endif
#if (MAX_PLANTS >= 5)
HomieSetting<long> plant5SensorTrigger("moist5", "Moist5 sensor value, when pump activates");
#endif
#if (MAX_PLANTS >= 6)
HomieSetting<long> plant6SensorTrigger("moist6", "Moist6 sensor value, when pump activates");
#endif

Ds18B20 dallas(SENSOR_DS18B20);

Plant mPlants[MAX_PLANTS] = { 
#if (MAX_PLANTS >= 1)
        Plant(SENSOR_PLANT1, OUTPUT_PUMP1), 
#endif
#if (MAX_PLANTS >= 2)
        Plant(SENSOR_PLANT2, OUTPUT_PUMP2), 
#endif
#if (MAX_PLANTS >= 3)
        Plant(SENSOR_PLANT3, OUTPUT_PUMP3), 
#endif
#if (MAX_PLANTS >= 4)
        Plant(SENSOR_PLANT4, OUTPUT_PUMP4), 
#endif
#if (MAX_PLANTS >= 5)
        Plant(SENSOR_PLANT5, OUTPUT_PUMP5), 
#endif
#if (MAX_PLANTS >= 6)
        Plant(SENSOR_PLANT6, OUTPUT_PUMP6) 
#endif
      };

void readAnalogValues() {
  if (readCounter < AMOUNT_SENOR_QUERYS) {
    lipoSensorValues += analogRead(SENSOR_LIPO);
    solarSensorValues += analogRead(SENSOR_SOLAR);
    readCounter++;
  } else {
    lipoSenor = (lipoSensorValues >> SENSOR_QUERY_SHIFTS);
    lipoSensorValues = 0;
    solarSensor = (solarSensorValues >> SENSOR_QUERY_SHIFTS);
    solarSensorValues = 0;
    
    readCounter = 0;
  }
}

/**
 * @brief cyclic Homie callback
 * All logic, to be done by the controller cyclically
 */
void loopHandler() {

  /* Move from Setup to this position, because the Settings are only here available */
  if (!mLoopInited) {
    // Configure Deep Sleep:
    if (deepSleepTime.get()) {
      Serial << "HOMIE | Setup sleeping for " << deepSleepTime.get() << " ms" << endl;
    }
    if (wateringTime.get()) {
      Serial << "HOMIE | Setup watering for " << abs(wateringTime.get()) << " s" << endl;
    }
    /* Publish default values */
    plant1.setProperty("switch").send(String("OFF"));            
    plant2.setProperty("switch").send(String("OFF"));            
    plant3.setProperty("switch").send(String("OFF"));         
    
#if (MAX_PLANTS >= 4)
    plant4.setProperty("switch").send(String("OFF"));            
    plant5.setProperty("switch").send(String("OFF"));
    plant6.setProperty("switch").send(String("OFF"));
#endif   

    for(int i=0; i < plantCnt.get(); i++) {
      mPlants[i].calculateSensorValue(AMOUNT_SENOR_QUERYS);
      int boundary4MoistSensor=-1;
      switch (i)
      {
      case 0:
        boundary4MoistSensor = plant1SensorTrigger.get();
        plant1.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095 ));
        break;
      case 1:
        boundary4MoistSensor = plant2SensorTrigger.get();
        plant2.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095));
        break;
      case 2:
        boundary4MoistSensor = plant3SensorTrigger.get();
        plant3.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095));
        break;
#if (MAX_PLANTS >= 4)
      case 3:
        boundary4MoistSensor = plant4SensorTrigger.get();
        plant4.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095));
        break;
      case 4:
        boundary4MoistSensor = plant5SensorTrigger.get();
        plant5.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095));
        break;
      case 5:
        boundary4MoistSensor = plant6SensorTrigger.get();
        plant6.setProperty("moist").send(String(100 * mPlants[i].getSensorValue() / 4095));
        break;
#endif
      }

#ifndef HC_SR04
      if (SOLAR_VOLT(solarSensor) > SOLAR4SENSORS) {
        if (mWaterLow && mWaterAtEmptyLevel) {
          sensorWater.setProperty("remaining").send("50");
        } else if (!mWaterLow && mWaterAtEmptyLevel) {
          sensorWater.setProperty("remaining").send("10");
        } else if (!mWaterLow && !mWaterAtEmptyLevel) {
          sensorWater.setProperty("remaining").send("0");
        } else if (!mWaterLow && !mWaterAtEmptyLevel) {
          sensorWater.setProperty("remaining").send("-1");
        }
      } else {
        Serial << "Sun not strong enough for sensors (" << String(SOLAR_VOLT(solarSensor)) << "V )" << endl;
      }
#else
      mWaterAtEmptyLevel = (mWaterGone <= waterLevel.get());
      int waterLevelPercent = (100 * mWaterGone) / waterLevel.get();
      sensorWater.setProperty("remaining").send(String(waterLevelPercent));
      Serial << "Water : " << mWaterGone << " cm (" << waterLevelPercent << "%)" << endl;
#endif
      mPumpIsRunning=false;
      /* Check if a plant needs water */
      if (mPlants[i].isPumpRequired(boundary4MoistSensor) && 
          (mWaterAtEmptyLevel) && 
          (!mPumpIsRunning)) {
        if (digitalRead(mPlants[i].getPumpPin()) == LOW) {
          Serial << "Plant" << (i+1) << " needs water" << endl;
          switch (i)
          {
          case 0:
            plant1.setProperty("switch").send(String("ON"));
            break;
          case 1:
            plant2.setProperty("switch").send(String("ON"));
            break;
          case 2:
            plant3.setProperty("switch").send(String("ON"));
            break;
#if (MAX_PLANTS >= 4)
          case 3:
            plant4.setProperty("switch").send(String("ON"));
            break;
          case 4:
            plant5.setProperty("switch").send(String("ON"));
            break;
          case 5:
            plant6.setProperty("switch").send(String("ON"));
            break;
#endif
          }
        }
        digitalWrite(mPlants[i].getPumpPin(), HIGH);
        mPumpIsRunning=true;
      }
    }
  }
  mLoopInited = true;

  readAnalogValues();

  if ((millis() % 1500) == 0) {
    sensorLipo.setProperty("percent").send( String(100 * lipoSenor / 4095) );
    sensorLipo.setProperty("volt").send( String(ADC_5V_TO_3V3(lipoSenor)) );
    sensorSolar.setProperty("percent").send(String((100 * solarSensor ) / 4095));
    sensorSolar.setProperty("volt").send( String(SOLAR_VOLT(solarSensor)) );
  } else if ((millis() % 1000) == 0) {
    float temp[2] = { TEMP_INIT_VALUE, TEMP_INIT_VALUE };
    float* pFloat = temp;
    int devices = dallas.readAllTemperatures(pFloat, 2);
    if (devices < 2) {
      if ((pFloat[0] > TEMP_INIT_VALUE) && (pFloat[0] < TEMP_MAX_VALUE) ) {
        sensorTemp.setProperty("control").send( String(pFloat[0]));
      }
    } else if (devices >= 2) {      
      if ((pFloat[0] > TEMP_INIT_VALUE) && (pFloat[0] < TEMP_MAX_VALUE) ) {
        sensorTemp.setProperty("temp").send( String(pFloat[0]));
      }
      if ((pFloat[1] > TEMP_INIT_VALUE) && (pFloat[1] < TEMP_MAX_VALUE) ) {
        sensorTemp.setProperty("control").send( String(pFloat[1]));
      }
    }
  }

  /* Main Loop functionality */
  if ((!mPumpIsRunning) || (!mWaterAtEmptyLevel) ) {
      /* let the ESP sleep qickly, as nothing must be done */
      if ((millis() >= (MIN_TIME_RUNNING * MS_TO_S)) && (deepSleepTime.get() > 0)) {
        mDeepSleep = true; 
        Serial << "No Water or Pump" << endl;
      }
  }

  /* Always check, that after 5 minutes the device is sleeping */
  /* Pump is running, go to sleep after defined time */
  if ((millis() >= (((MIN_TIME_RUNNING + abs(wateringTime.get())) * MS_TO_S) + 5)) && 
            (deepSleepTime.get() > 0)) {
    Serial << "No sleeping activated (maximum)" << endl;
    Serial << "Pump was running:" << mPumpIsRunning << "Water level is empty: " << mWaterAtEmptyLevel << endl;
    mDeepSleep = true;
  } else if ((millis() >= (((MIN_TIME_RUNNING + abs(wateringTime.get())) * MS_TO_S) + 0)) &&
      (deepSleepTime.get() > 0)) {
    Serial << "Maximum time reached: " << endl;
    Serial <<  (mPumpIsRunning ? "Pump was running " : "No Pump ") << (mWaterAtEmptyLevel ? "Water level is empty" : "Water available") << endl;
    mDeepSleep = true;
  }
}

bool switchGeneralPumpHandler(const int pump, const HomieRange& range, const String& value) {
  if (range.isRange) return false;  // only one switch is present
  switch (pump)
  {
#if MAX_PLANTS >= 1
  case 0:
#endif
#if MAX_PLANTS >= 2
  case 1:
#endif
  #if MAX_PLANTS >= 3
#endif
  case 2:
#if MAX_PLANTS >= 4
  case 3:
#endif
#if MAX_PLANTS >= 5
  case 4:
#endif
#if MAX_PLANTS >= 6
  case 5:
#endif

    if ((value.equals("ON")) || (value.equals("On")) || (value.equals("on")) || (value.equals("true"))) {
      digitalWrite(mPlants[pump].getPumpPin(), HIGH);
      return true;
    } else if ((value.equals("OFF")) || (value.equals("Off")) || (value.equals("off")) || (value.equals("false")) ) {
      digitalWrite(mPlants[pump].getPumpPin(), LOW);
      return true;
    } else {
      return false;
    }
    break;
  default:
    return false;
  }
}

/**
 * @brief Handle Mqtt commands for the pumpe, responsible for the first plant
 * 
 * @param range multiple transmitted values (not used for this function)
 * @param value single value
 * @return true when the command was parsed and executed succuessfully
 * @return false on errors when parsing the request
 */
bool switch1Handler(const HomieRange& range, const String& value) {
  return switchGeneralPumpHandler(0, range, value);
}


/**
 * @brief Handle Mqtt commands for the pumpe, responsible for the second plant
 * 
 * @param range multiple transmitted values (not used for this function)
 * @param value single value
 * @return true when the command was parsed and executed succuessfully
 * @return false on errors when parsing the request
 */
bool switch2Handler(const HomieRange& range, const String& value) {
  return switchGeneralPumpHandler(1, range, value);
}

/**
 * @brief Handle Mqtt commands for the pumpe, responsible for the third plant
 * 
 * @param range multiple transmitted values (not used for this function)
 * @param value single value
 * @return true when the command was parsed and executed succuessfully
 * @return false on errors when parsing the request
 */
bool switch3Handler(const HomieRange& range, const String& value) {
  return switchGeneralPumpHandler(2, range, value);
}

/**
 * @brief Sensors, that are connected to GPIOs, mandatory for WIFI.
 * These sensors (ADC2) can only be read when no Wifi is used.
 */
void readSensors() {
  /* activate all sensors */
  pinMode(OUTPUT_SENSOR, OUTPUT);
  digitalWrite(OUTPUT_SENSOR, HIGH);
  /* Use Pump 4 to activate and deactivate the Sensors */
#if (MAX_PLANTS < 4)
  pinMode(OUTPUT_PUMP4, OUTPUT);
  digitalWrite(OUTPUT_PUMP4, HIGH);
#endif

  delay(100);
  /* wait before reading something */
  for (int readCnt=0;readCnt < AMOUNT_SENOR_QUERYS; readCnt++) {
    for(int i=0; i < MAX_PLANTS; i++) {
      mPlants[i].addSenseValue(analogRead(mPlants[i].getSensorPin()));
    }
  }

#ifndef HC_SR04
  mWaterAtEmptyLevel = digitalRead(INPUT_WATER_EMPTY);
  mWaterLow = digitalRead(INPUT_WATER_LOW);
  mOverflow = digitalRead(INPUT_WATER_OVERFLOW);
#else
  /* Use the Ultrasonic sensor to measure waterLevel */
  
  /* deactivate all sensors and measure the pulse */
  digitalWrite(INPUT_WATER_EMPTY, LOW);
  delayMicroseconds(2);
  digitalWrite(INPUT_WATER_EMPTY, HIGH);
  delayMicroseconds(10);
  digitalWrite(INPUT_WATER_EMPTY, LOW);
  float duration = pulseIn(INPUT_WATER_LOW, HIGH);
  float distance = (duration*.0343)/2;
  mWaterGone = (int) distance;
  Serial << "HC_SR04 | Distance : " << String(distance) << " cm" << endl;
#endif
  /* deactivate the sensors */
  digitalWrite(OUTPUT_SENSOR, LOW);
#if (MAX_PLANTS < 4)
  digitalWrite(OUTPUT_PUMP4, LOW);
#endif
}

/**
 * @brief Startup function
 * Is called once, the controller is started
 */
void setup() {
  /* Required to read the temperature once */
  float temp[2] = {0, 0};
  float* pFloat = temp;

  /* read button */
  pinMode(BUTTON, INPUT);
  /* Prepare Water sensors */
  pinMode(INPUT_WATER_EMPTY, INPUT);
  pinMode(INPUT_WATER_LOW, INPUT);
  pinMode(INPUT_WATER_OVERFLOW, INPUT);

  Serial.begin(115200);
  Serial.setTimeout(1000); // Set timeout of 1 second
  Serial << endl << endl;
  Serial << "Read analog sensors..." << endl;
  /* Disable Wifi and bluetooth */
  WiFi.mode(WIFI_OFF);
  /* now ADC2 can be used */
  readSensors();
  /* activate Wifi again */
  WiFi.mode(WIFI_STA);


  Homie_setFirmware("PlantControl", FIRMWARE_VERSION);
  Homie.setLoopFunction(loopHandler);

  // Load the settings
  deepSleepTime.setDefaultValue(0);
  deepSleepNightTime.setDefaultValue(0);
  wateringTime.setDefaultValue(60);
  plantCnt.setDefaultValue(0).setValidator([] (long candidate) {
    return ((candidate >= 0) && (candidate <= 6) );
  });
  plant1SensorTrigger.setDefaultValue(0);
  plant2SensorTrigger.setDefaultValue(0);
  plant3SensorTrigger.setDefaultValue(0);
#if (MAX_PLANTS >= 4)
  plant4SensorTrigger.setDefaultValue(0);
  plant5SensorTrigger.setDefaultValue(0);
  plant6SensorTrigger.setDefaultValue(0);
#endif

#ifdef HC_SR04
  waterLevel.setDefaultValue(50);
#endif

  // Advertise topics
  plant1.advertise("switch").setName("Pump 1")
                            .setDatatype("boolean")
                            .settable(switch1Handler);
  plant1.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
  plant2.advertise("switch").setName("Pump 2")
                            .setDatatype("boolean")
                            .settable(switch2Handler);
  plant2.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
  plant3.advertise("switch").setName("Pump 3")
                            .setDatatype("boolean")
                            .settable(switch3Handler);
  plant3.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
#if (MAX_PLANTS >= 4)
  plant4.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
  plant5.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
  plant6.advertise("moist").setName("Percent")
                            .setDatatype("number")
                            .setUnit("%");
#endif
  sensorTemp.advertise("control")
              .setName("Temperature")
              .setDatatype("number")
              .setUnit("°C");
  sensorTemp.advertise("temp")
              .setName("Temperature")
              .setDatatype("number")
              .setUnit("°C");

  sensorLipo.advertise("percent")
              .setName("Percent")
              .setDatatype("number")
              .setUnit("%");
  sensorLipo.advertise("volt")
              .setName("Volt")
              .setDatatype("number")
              .setUnit("V");

  sensorSolar.advertise("percent")
              .setName("Percent")
              .setDatatype("number")
              .setUnit("%");
  sensorSolar.advertise("volt")
              .setName("Volt")
              .setDatatype("number")
              .setUnit("V");
  sensorWater.advertise("remaining").setDatatype("number").setUnit("%");

  Homie.setup();

  /* Intialize inputs and outputs */
  for(int i=0; i < plantCnt.get(); i++) {
    pinMode(mPlants[i].getPumpPin(), OUTPUT);
    pinMode(mPlants[i].getSensorPin(), ANALOG);
    digitalWrite(mPlants[i].getPumpPin(), LOW);
  }
  /* Setup Solar and Lipo measurement */
  pinMode(SENSOR_LIPO, ANALOG);
  pinMode(SENSOR_SOLAR, ANALOG);
  /* Read analog values at the start */
  do {
    readAnalogValues();
  } while (readCounter != 0);


  // Configure Deep Sleep:
  if ((deepSleepNightTime.get() > 0) &&
      ( SOLAR_VOLT(solarSensor) < MINIMUM_SOLAR_VOLT)) {
    Serial << "HOMIE | Setup sleeping for " << deepSleepNightTime.get() << " ms as sun is at " << SOLAR_VOLT(solarSensor) << "V"  << endl;
    uint64_t usSleepTime = deepSleepNightTime.get() * 1000U;
    esp_sleep_enable_timer_wakeup(usSleepTime);
  }else if (deepSleepTime.get()) {
    Serial << "HOMIE | Setup sleeping for " << deepSleepTime.get() << " ms" << endl;
    uint64_t usSleepTime = deepSleepTime.get() * 1000U;
    esp_sleep_enable_timer_wakeup(usSleepTime);
  }

  if ( (ADC_5V_TO_3V3(lipoSenor) < MINIMUM_LIPO_VOLT) && (deepSleepTime.get()) ) {
    long sleepEmptyLipo = (deepSleepTime.get() * EMPTY_LIPO_MULTIPL);
    Serial << "HOMIE | Change sleeping to " << sleepEmptyLipo << " ms as lipo is at " << ADC_5V_TO_3V3(lipoSenor) << "V" << endl;
    esp_sleep_enable_timer_wakeup(sleepEmptyLipo * 1000U);
    mDeepSleep = true;
  }

  /* Read the temperature sensors once, as first time 85 degree is returned */
  Serial << "DS18B20 | sensors: " << String(dallas.readDevices()) << endl;
  delay(200);
  if (dallas.readAllTemperatures(pFloat, 2) > 0) {
      Serial << "DS18B20 | Temperature 1: " << String(temp[0]) << endl;
      Serial << "DS18B20 | Temperature 2: " << String(temp[1]) << endl;
  }
  delay(200);
  if (dallas.readAllTemperatures(pFloat, 2) > 0) {
      Serial << "Temperature 1: " << String(temp[0]) << endl;
      Serial << "Temperature 2: " << String(temp[1]) << endl;
  }
}

/**
 * @brief Cyclic call
 * Executs the Homie base functionallity or triggers sleeping, if requested.
 */
void loop() {
  if (!mDeepSleep) {
    if (Serial.available() > 0) {
      // read the incoming byte:
      int incomingByte = Serial.read();

      switch ((char) incomingByte)
      {
      case 'P':
          Serial << "Activate Sensor OUTPUT " << endl;
          pinMode(OUTPUT_SENSOR, OUTPUT);
          digitalWrite(OUTPUT_SENSOR, HIGH);
        break;
      case 'p':
          Serial << "Deactivate Sensor OUTPUT " << endl;
          pinMode(OUTPUT_SENSOR, OUTPUT);
          digitalWrite(OUTPUT_SENSOR, LOW);
        break;
      default:
        break;
      }
    }

    if ((digitalRead(BUTTON) == LOW) && (mButtonClicks % 2) == 0) {
      float temp[2] = {0, 0};
      float* pFloat = temp;
      mButtonClicks++;

      Serial << "SELF TEST (clicks: " << String(mButtonClicks) << ")" << endl;
      
      Serial << "DS18B20 sensors: " << String(dallas.readDevices()) << endl;
      delay(200);
      if (dallas.readAllTemperatures(pFloat, 2) > 0) {
          Serial << "Temperature 1: " << String(temp[0]) << endl;
          Serial << "Temperature 2: " << String(temp[1]) << endl;
      }
      
      switch(mButtonClicks) {
        case 1:
        case 3:
        case 5:
          if (mButtonClicks > 1) {
          Serial << "Read analog sensors..." << endl;
            /* Disable Wifi and bluetooth */
            WiFi.mode(WIFI_OFF);
            delay(50);
            /* now ADC2 can be used */
            readSensors();
          }
#ifndef HC_SR04
          Serial << "Water Low:     " << String(mWaterLow) << endl;
          Serial << "Water Empty:    " << String(mWaterAtEmptyLevel) << endl;
          Serial << "Water Overflow: " << String(mOverflow) << endl;
#else
          Serial << "Water gone:     " << String(mWaterGone) << " cm" << endl;
#endif
          for(int i=0; i < MAX_PLANTS; i++) {
            mPlants[i].calculateSensorValue(AMOUNT_SENOR_QUERYS);

            Serial << "Moist Sensor " << (i+1) << ": " << String(mPlants[i].getSensorValue()) << " Volt: " << String(ADC_5V_TO_3V3(mPlants[i].getSensorValue())) << endl;
          }
          /* Read enough values */
          do {
            readAnalogValues();
            Serial << "Read Analog (" << String(readCounter) << ")" << endl;;
          } while (readCounter != 0);

          Serial << "Lipo Sensor  - Raw: " << String(lipoSenor) << " Volt: " << String(ADC_5V_TO_3V3(lipoSenor)) << endl;
          Serial << "Solar Sensor - Raw: " << String(solarSensor) << " Volt: " << String(SOLAR_VOLT(solarSensor)) << endl;
          break;
          case 7:
            Serial << "Activate Sensor OUTPUT " << endl;
            pinMode(OUTPUT_SENSOR, OUTPUT);
            digitalWrite(OUTPUT_SENSOR, HIGH);
            break;
          case 9:
            Serial << "Activate Pump1 GPIO" << String(mPlants[0].getPumpPin()) << endl;
            digitalWrite(mPlants[0].getPumpPin(), HIGH);
            break;
          case 11:
            Serial << "Activate Pump2 GPIO" << String(mPlants[1].getPumpPin()) << endl;
            digitalWrite(mPlants[1].getPumpPin(), HIGH);
            break;
          case 13:
            Serial << "Activate Pump3 GPIO" << String(mPlants[2].getPumpPin()) << endl;
            digitalWrite(mPlants[2].getPumpPin(), HIGH);
            break;
          case 15:
            Serial << "Activate Pump4/Sensor GPIO" << String(OUTPUT_PUMP4) << endl;
            digitalWrite(OUTPUT_PUMP4, HIGH);
            break;
          default:
            Serial << "No further tests! Please reboot" << endl;
      }
      Serial.flush();
    }else if (mButtonClicks > 0 && (digitalRead(BUTTON) == HIGH) && (mButtonClicks % 2) == 1) {
      Serial << "Self Test Ended" << endl;
      mButtonClicks++;
      /* Always reset all outputs */
      digitalWrite(OUTPUT_SENSOR, LOW);
      for(int i=0; i < MAX_PLANTS; i++) {
        digitalWrite(mPlants[i].getPumpPin(), LOW);
      }
      digitalWrite(OUTPUT_PUMP4, LOW);
    } else if (mButtonClicks == 0) {
      Homie.loop();
    }

  } else {
    Serial << (millis()/ 1000) << "s running; sleeeping ..." << endl;
    Serial.flush();
    esp_deep_sleep_start();
  }
}
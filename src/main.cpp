#include <Arduino.h>

constexpr uint8_t PIN_PUMP = 2;
constexpr uint8_t PIN_COMPRESSOR = 3;
constexpr uint8_t PIN_IN_FAN_LOW = 4;
constexpr uint8_t PIN_IN_FAN_MEDIUM = 5;
constexpr uint8_t PIN_IN_FAN_HIGH = 6;
constexpr uint8_t PIN_OUT_FAN_HIGH = 7;
constexpr uint8_t PIN_STEPER_1 = 8;
constexpr uint8_t PIN_STEPER_2 = 9;
constexpr uint8_t PIN_STEPER_3 = 10;
constexpr uint8_t PIN_STEPER_4 = 11;
constexpr uint8_t PIN_OUT_FAN_LOW = 12;

constexpr uint8_t PIN_TEMP_SENSOR_COOLER = A0;
constexpr uint8_t PIN_TEMP_SENSOR_AIR = A1;
constexpr uint8_t PIN_WATER_SENSOR_NO = A2;
constexpr uint8_t PIN_WATER_SENSOR_NC = A3;

constexpr float OFF_TEMP_THRESHOLD = 3.0;
constexpr float STANDBY_TEMP_THRESHOLD = 1.0;
constexpr float UNFROST_START_TEMP_THRESHOLD = 5.0;
constexpr float UNFROST_STOP_TEMP_THRESHOLD = 10.0;

struct InFanSpeed
{
  enum Enum : char
  {
    Off,
    Low,
    Medium,
    High  
  };
};

struct OutFanSpeed
{
  enum Enum : char
  {
    Off,
    Low,
    High  
  };
};

struct ACState
{
  enum Enum : char
  {
    Off,
    StandBy,
    Cooling,
    Unfrosting,
  };
};


float target;
ACState::Enum state;
bool pumpStatus;

bool isWaterError;
bool isWaterFull;
float airTemp;
float coolerTemp;

unsigned long lastPumpDisabledTime = 0;

void SetInFanSpeed(InFanSpeed::Enum speed)
{
  // Turn off everything to clean previous state
  digitalWrite(PIN_IN_FAN_LOW, LOW);
  digitalWrite(PIN_IN_FAN_MEDIUM, LOW);
  digitalWrite(PIN_IN_FAN_HIGH, LOW);

  // Turn on required pin to get the right speed
  switch (speed)
  {
  case InFanSpeed::Low:
    digitalWrite(PIN_IN_FAN_LOW, HIGH);
    break;
  case InFanSpeed::Medium:
    digitalWrite(PIN_IN_FAN_MEDIUM, HIGH);
    break;
  case InFanSpeed::High:
    digitalWrite(PIN_IN_FAN_HIGH, HIGH);
    break;
  case InFanSpeed::Off:
    break;
  }
}

void SetOutFanSpeed(OutFanSpeed::Enum speed)
{
  // Turn off everything to clean previous state
  digitalWrite(PIN_OUT_FAN_LOW, LOW);
  digitalWrite(PIN_OUT_FAN_HIGH, LOW);

  // Turn on required pin to get the right speed
  switch (speed)
  {
  case OutFanSpeed::Low:
    digitalWrite(PIN_OUT_FAN_LOW, HIGH);
    break;
  case OutFanSpeed::High:
    digitalWrite(PIN_OUT_FAN_HIGH, HIGH);
    break;
  case OutFanSpeed::Off:
    break;
  }
}

void SetCompressorStatus(bool isEnabled)
{
  digitalWrite(PIN_COMPRESSOR, isEnabled ? HIGH : LOW);
}

void SetPumpStatus(bool isEnabled)
{
  if(!isEnabled)
  {
    lastPumpDisabledTime = millis();
  }
  digitalWrite(PIN_PUMP, isEnabled ? HIGH : LOW);
}

bool IsWaterFull(bool& hasError)
{
  const bool noIsFull = digitalRead(PIN_WATER_SENSOR_NO) == LOW;
  const bool ncIsFull = digitalRead(PIN_WATER_SENSOR_NC) == LOW;

  Serial.print("NoFull:      "); // for debug 
  Serial.println(noIsFull);
  Serial.print("NcFull:      "); // for debug 
  Serial.println(ncIsFull);

  if(noIsFull == ncIsFull)
  {
    hasError = false;
    return noIsFull;
  }
  else
  {
    hasError = true;
    return false;
  }
}

float ReadTempAir()
{
  constexpr float valAt26 = 517.f;
  constexpr float valAt39 = 717.f;
  // pulldown resistor: 15k
  // We don't need a wide range of temperature, let's assume lineare transition in that range
  float in = analogRead(PIN_TEMP_SENSOR_AIR);
  float temp = (in - valAt26) * 13.0f / (valAt39 - valAt26) + 26.0f;
  Serial.print("airTempRaw:  "); // for debug and calibration
  Serial.println(in);
  return temp;
}

float ReadTempCooler()
{
  constexpr float valAt26 = 515.f;
  constexpr float valAt39 = 717.f;
  // pulldown resistor: 15k
  // We don't need a wide range of temperature, let's assume lineare transition in that range
  float in = analogRead(PIN_TEMP_SENSOR_COOLER);
  float temp = (in - valAt26) * 13.0f / (valAt39 - valAt26) + 26.0f;
  Serial.print("coolerTempRaw: "); // for debug and calibration
  Serial.println(in);
  return temp;
}

void processState()
{
  switch (state)
  {
  case ACState::Unfrosting:
    SetOutFanSpeed(OutFanSpeed::Off);
    SetInFanSpeed(InFanSpeed::High);
    SetCompressorStatus(false);
    break;
  case ACState::Cooling:
    SetOutFanSpeed(OutFanSpeed::High);
    SetInFanSpeed(InFanSpeed::High);
    SetCompressorStatus(true);
    break;
  case ACState::StandBy:
    SetOutFanSpeed(OutFanSpeed::Off);
    SetInFanSpeed(InFanSpeed::Low);
    SetCompressorStatus(false);
    break;
  case ACState::Off:
    SetOutFanSpeed(OutFanSpeed::Off);
    SetInFanSpeed(InFanSpeed::Off);
    SetCompressorStatus(false);
    break;
  }

  SetPumpStatus(pumpStatus);
}

void UpdateState()
{
  const float delta_temp = airTemp - target;

  switch (state)
  {
  case ACState::Unfrosting:
    if(coolerTemp > UNFROST_STOP_TEMP_THRESHOLD)
    {
      if(delta_temp < -STANDBY_TEMP_THRESHOLD)
      {
        state = ACState::StandBy;
      }
      else
      {
        state = ACState::Cooling;
      }
    }
  case ACState::Cooling:
    if(delta_temp < -STANDBY_TEMP_THRESHOLD)
    {
      state = ACState::StandBy;
    }
    else if(coolerTemp < UNFROST_START_TEMP_THRESHOLD)
    {
      state = ACState::Unfrosting;
    }
    break;
  case ACState::StandBy:
    if(delta_temp > 0.0)
    {
      state = ACState::Cooling;
    }
    else if(delta_temp < -OFF_TEMP_THRESHOLD)
    {
      state = ACState::Off;
    }
    break;
  case ACState::Off:
    if(delta_temp > -OFF_TEMP_THRESHOLD + 0.5)
    {
      state = ACState::StandBy;
    }
    break;
  }
}

void updateSensor()
{
  isWaterFull = IsWaterFull(isWaterError);
  airTemp = ReadTempAir();
  coolerTemp = ReadTempCooler();
}

void updatePump()
{
  if(pumpStatus)
  {
    if(!isWaterFull && millis() - lastPumpDisabledTime > 1000 * 15)
    {
      // run for at least 15 seconds
      pumpStatus = false;
    }
    else if((millis() - lastPumpDisabledTime) > (1000 * 60 * 1))
    {
      // if we are not able to get the water out for 1 minute, stop the AC
      state = ACState::Off;
    }
  }
  else
  {
    if(isWaterFull)
    {
      pumpStatus = true;
    }
  }

  if(isWaterError)
  {
    state = ACState::Off;
  }
}

void printLogs()
{
  Serial.print("isError:     ");
  Serial.println(isWaterError);
  Serial.print("isWaterFull: ");
  Serial.println(isWaterFull);
  Serial.print("airTemp:     ");
  Serial.println(airTemp);
  Serial.print("coolerTemp:  ");
  Serial.println(coolerTemp);
  Serial.print("State:       ");
  switch (state)
  {
  case ACState::Unfrosting:
    Serial.println("Unfrosting");
    break;
  case ACState::Cooling:
    Serial.println("Cooling");
    break;
  case ACState::StandBy:
    Serial.println("StandBy");
    break;
  case ACState::Off:
    Serial.println("Off");
    break;
  }
  Serial.print("Pump:        ");
  Serial.println(pumpStatus);
}

void setup() 
{
  SetCompressorStatus(false);
  SetInFanSpeed(InFanSpeed::Off);  
  SetOutFanSpeed(OutFanSpeed::Off);
  SetPumpStatus(false);

  // Disable stepper
  digitalWrite(PIN_STEPER_1, LOW);
  digitalWrite(PIN_STEPER_2, LOW);
  digitalWrite(PIN_STEPER_3, LOW);
  digitalWrite(PIN_STEPER_4, LOW);

  target = 28.0;
  state = ACState::Off;

  Serial.begin(9600);
}

void loop() 
{
  updateSensor();
  printLogs();
  UpdateState();
  updatePump(); // update pump post state as it may reset it
  processState();  
  
  delay(1000);
}

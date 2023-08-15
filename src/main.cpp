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
  digitalWrite(PIN_PUMP, isEnabled ? HIGH : LOW);
}

bool IsWaterFull(bool& hasError)
{
  const bool noIsFull = digitalRead(PIN_WATER_SENSOR_NO) == HIGH;
  const bool ncIsFull = digitalRead(PIN_WATER_SENSOR_NC) == LOW;

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
  constexpr float valAt27 = 437.;
  constexpr int valAt39 = 717.;
  // pulldown resistor: 15k
  // We don't need a wide range of temperature, let's assume lineare transition in that range
  float in = analogRead(PIN_TEMP_SENSOR_AIR);
  float temp = (in - valAt27) * 12.0 / (valAt39 - valAt27) + 27.0;
  Serial.print("airTempRaw:  ");
  Serial.println(in);
  return temp;
}

float ReadTempCooler()
{
  constexpr int valAt27 = 444.;
  constexpr int valAt39 = 717.;
  // pulldown resistor: 15k
  // We don't need a wide range of temperature, let's assume lineare transition in that range
  float in = analogRead(PIN_TEMP_SENSOR_AIR);
  float temp = (in - valAt27) * 12.0 / (valAt39 - valAt27) + 27.0;
  Serial.print("coolerTempRaw: ");
  Serial.println(in);
  return temp;
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

  target = 27.0;
  state = ACState::Off;

  Serial.begin(9600);
}

void loop() 
{
  bool isError;
  const bool isWaterFull = IsWaterFull(isError);

  const float airTemp = ReadTempAir();
  const float coolerTemp = ReadTempCooler();

  Serial.print("isError:     ");
  Serial.println(isError);
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

  delay(1000);
}

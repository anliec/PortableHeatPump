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

int ReadTemperatureDeciCelcius(uint8_t pin)
{
  return analogRead(pin);
}

int ReadTempAir()
{
  return ReadTemperatureDeciCelcius(PIN_TEMP_SENSOR_AIR);
}

int ReadTempCooler()
{
  return ReadTemperatureDeciCelcius(PIN_TEMP_SENSOR_COOLER);
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

  Serial.begin(9600);
}

void loop() 
{
  bool isError;
  const bool isWaterFull = IsWaterFull(isError);

  const int airTemp = ReadTempAir();
  const int coolerTemp = ReadTempCooler();

  Serial.print("isError:     ");
  Serial.println(isError);
  Serial.print("isWaterFull: ");
  Serial.println(isWaterFull);
  Serial.print("airTemp:     ");
  Serial.println(airTemp);
  Serial.print("coolerTemp:  ");
  Serial.println(coolerTemp);

  delay(1000);
}

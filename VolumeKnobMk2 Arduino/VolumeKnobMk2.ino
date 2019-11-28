#include <EEPROM.h>

#include "FastLED.h"
#include "Rotary.h"
#include "HID-Project.h"

//#define ANIMATION

#define PIN_ENCODER_A         0
#define PIN_ENCODER_B         2
#define PIN_ENCODER_SWITCH    1
#define PIN_LED               8

#define LED_COUNT             3
#define COLORS_COUNT          11
#define WAIT_TIME_MS          2000
#define PUSH_COUNT            3
#define ANIMATION_FRAME_MS    25

struct SettingsStorage
{
  uint8_t SelectedColor[LED_COUNT] = {0};
  uint8_t Brightness = 64;
};

const uint8_t Colors[COLORS_COUNT][3] = {{255, 64, 0}, {255, 0, 0}, {255, 0, 255}, {255, 0, 64}, {64, 0, 255}, {0, 0, 255}, {0, 64, 255}, {0, 255, 64}, {0, 255, 0}, {255, 255, 255}, {0, 0, 0}};
const uint8_t EEPROMVer = 4;
const uint16_t eeprom_addr_EEPROMVer = 0x00;
const uint16_t eeprom_addr_Settings = eeprom_addr_EEPROMVer + sizeof(SettingsStorage);

CRGB LEDs[LED_COUNT];
Rotary Rotary1 = Rotary(PIN_ENCODER_A, PIN_ENCODER_B);
SettingsStorage Settings;

bool SwitchWasPressed = false;
// 0 - not in setup
// 1 to LED_COUNT - set led color
// LED_COUNT + 1 - set all leds colors
// LED_COUNT + 2 - set brightness
uint8_t SetupState = 0;
int8_t VolPresses = 0;
uint8_t MutePresses = 0;
unsigned long LastPushTime = 0;
uint8_t SeqPressesCounter = 0;
bool UpdateLedNeeded = false;
uint8_t CurrentBrightness = 0;
#ifdef ANIMATION
uint8_t TargetBrightness = 0;
unsigned long LastAnimationTime = 0;
#endif

void setup()
{
    //pinMode(PIN_ENCODER_A, INPUT_PULLUP);
    //pinMode(PIN_ENCODER_B, INPUT_PULLUP);
    pinMode(PIN_ENCODER_SWITCH, INPUT_PULLUP);
    
    attachInterrupt(3, OnMutePress, FALLING);
    
    FastLED.addLeds<WS2812, PIN_LED, GRB>(LEDs, LED_COUNT);

    byte eepromVer = EEPROM.read(eeprom_addr_EEPROMVer);
    if (eepromVer != EEPROMVer)
    {
      EEPROM.write(eeprom_addr_EEPROMVer, EEPROMVer);
      SaveSettings();
    }
    else
    {
      EEPROM.get(eeprom_addr_Settings, Settings);
    }

    #ifndef ANIMATION
      CurrentBrightness = Settings.Brightness;
    #endif

    UpdateLed();
  
    Consumer.begin();
}

void loop()
{
  UpdateEncoder();

  if(UpdateLedNeeded)
  {
    UpdateLed();
  }

  while (VolPresses > 0)
  {
    Consumer.write(MEDIA_VOLUME_UP); // MMKEY_VOL_UP
    --VolPresses;
  }

  while (VolPresses < 0)
  {
    Consumer.write(MEDIA_VOLUME_DOWN); // MMKEY_VOL_DOWN_VOL_UP
    ++VolPresses;
  }

  while (MutePresses > 0)
  {
    Consumer.write(MEDIA_VOLUME_MUTE); // MMKEY_MUTE
    --MutePresses;
  }

#ifdef ANIMATION
  //if(TrinketHidCombo.isConnected())
  //{
    TargetBrightness = Settings.Brightness;
  //}
  //else
  //{
  //  TargetBrightness = 0;
  //}

  if((uint16_t)(millis() - LastAnimationTime) >= ANIMATION_FRAME_MS)
  {
    if(AnimationBrightness > TargetBrightness)
    {
      UpdateLedNeeded = true;
      --CurrentBrightness;
    }
    
    if(AnimationBrightness < TargetBrightness)
    {
      UpdateLedNeeded = true;
      ++CurrentBrightness;
    }
    
    LastAnimationTime = millis();
  }
#endif
}

void SaveSettings()
{
    EEPROM.put(eeprom_addr_Settings, Settings);
}

void OnMutePress()
{
  if(SetupState == 0)
  {
    if(LastPushTime != 0 && (millis() - LastPushTime < WAIT_TIME_MS))
    {
      if(++SeqPressesCounter == PUSH_COUNT)
      {
        SetupState = 1;
        UpdateLedNeeded = true;
        SeqPressesCounter = 0;
      }
    }
    else
    {
      SeqPressesCounter = 0;
    }
    ++MutePresses;
  }
  else
  {
    ++SetupState;
    if(SetupState == LED_COUNT + 3)
    {
      SetupState = 0;
      SaveSettings();
    }
    UpdateLedNeeded = true;
  }

  LastPushTime = millis();
  delay(5);
}

void UpdateEncoder()
{
  uint8_t result = Rotary1.process();
  if (result)
  {
    SeqPressesCounter = 0;

    if(SetupState == 0)
    {
      if (result == DIR_CW)
      {
        ++VolPresses;
      }
      else
      {
        --VolPresses;
      }
    }
    else if(SetupState < LED_COUNT + 2)
    {
      uint8_t colId = SetupState - 1;
      if(SetupState == LED_COUNT + 1)
      {
        colId = 0;
      }
      uint8_t& sel = Settings.SelectedColor[colId];
      if (result == DIR_CW)
      {
        if(sel == COLORS_COUNT - 1)
        {
          sel = 0;
        }
        else
        {
          ++sel;
        }
      }
      else
      {
        if(sel == 0)
        {
          sel = COLORS_COUNT - 1;
        }
        else
        {
          --sel;
        }
      }

      if(SetupState == LED_COUNT + 1)
      {
        for(uint8_t i = 1; i < LED_COUNT; ++i)
        {
          Settings.SelectedColor[i] = Settings.SelectedColor[0];
        }
      }

      UpdateLedNeeded = true;
    }
    else if(SetupState == LED_COUNT + 2)
    {
      if (result == DIR_CW)
      {
        if(Settings.Brightness < 255)
        {
          ++Settings.Brightness;
        }
      }
      else
      {
        if(Settings.Brightness > 0)
        {
          --Settings.Brightness;
        }
      }
      
      CurrentBrightness = Settings.Brightness;
      UpdateLedNeeded = true;
    }
  }
}

void UpdateLed()
{
  for(uint8_t i = 0; i < LED_COUNT; ++i)
  {
    if(SetupState == 0 || i == SetupState - 1 || SetupState > LED_COUNT)
    {
      const uint8_t* col = Colors[Settings.SelectedColor[i]];
      LEDs[i] = CRGB(col[0], col[1], col[2]);
    }
    else
    {
      LEDs[i] = CRGB::Black;
    }
  }

  FastLED.setBrightness(CurrentBrightness);
  
  UpdateLedNeeded = false;
    
  FastLED.show();
}

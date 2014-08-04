// C5D v1.01 MODIFIED [PRODUCTION] FIRMWARE
// ----------------------------------------------------------------------------
// Code Version:  1.2.0 Custom V2 [1.2.0 Base Code]
// Code Date:     April 10, 2014 [December 14, 2013]
// Authors:       Michael Woo [John and Nick @ JDS Labs, Inc.]
// Requires:      Arduino Bootloader, Arduino 1.0.1
// License:       Creative Commons Attribution-ShareAlike 3.0 Unported
//                http://creativecommons.org/licenses/by-sa/3.0/deed.en_US

#include <Wire.h>

// Pinouts (Note: Pinout of centerButton differs between C5 (centerButton = 0) and C5D (centerButton = 3))
// Power Rails
int PosRail = 6;                             // Pin to control +7V Low-DropOut regulator rail
int NegRail = 7;                             // Pin to control -7V Low-DropOut regulator rail

// LED
int LED = 4;                                 // Pin to control Bi-color LED (LED1 on the PCB): Green LED is wired active-low

// Digital Potentiometer
int PotPower = A2;                           // Pin to control 5V power supply to DS1882 IC logic
int RightButton = 5;                         // Pin to detect digital potentiometer's right pushbutton/volume up
int CenterButton = 3;                        // Pin to detect digital potentiometer's center pushbutton/gain
int LeftButton = 2;                          // Pin to detect digital potentiometer's left pushbutton/volume down
int Gain = A1;                               // Pin to control gain

// DAC
int DAC5V = 9;                               // Pin to control 5 V Low-DropOut regulator power supply to DAC circuit (activate before DAC33V)
int DAC33V = 8;                              // Pin to control all 3.3 V Low-DropOut regulator power supplies to DAC circuit (activate after DAC5V)
int FilterType = A0;                         // Pin to control Low Latency Filter of PCM5102 DAC

// Battery
int Battery = A7;                            // Pin to detect USB or battery voltage, 2.7-5.5 V DC





// Temporary Variables
int Mode = 0;                                // Determines the mode to run

// LED
int LEDState = LOW;                          // LED state: On = LOW, Off = HIGH
unsigned long LEDTimer = 0;                  // Sets the timer for the LED to flash during modes 1 and 2

// Digital Potentiometer
int Right = 0;                               // Right pushbutton is low
int Center = 0;                              // Center pushbutton is low
int Left = 0;                                // Left pushbutton is low
byte GainState = 0;                          // Current gain: Low-Gain = 0, High-Gain = 1
byte Attenuation = 62;                       // Attenuation value of volume potentiometer gangs
byte TempAttenuation = 0;                    // Holds attenuation value for second pot bitwise operation
byte AttenuationHold = 63;
int VolumeDelay = 375;                       // Delay (milliseconds) before volume control quickly transitions through steps
int StepTime = 85;                           // Delay (milliseconds) between each volume step in seek mode. Minimum value is 55ms, due to write time of DS1882

// DAC
int FilterState = HIGH;                      // State of PCM5102A's Low Latency Filter: Minimum-Phase Filter = HIGH (default), Linear-Phase Filter = LOW

// Battery
float BattVoltage = 5;                       // Temp variable to read battery voltage
int PowCheck = 500;                          // Delay (milliseconds) between periodically checking battery volume
unsigned long LastMillis = 0;                // Time of last battery check
boolean LowBatt = false;                     // Low battery indicator
float LowVThreshold = 3.51;                  // 'Low' hysteresis threshold (voltage at which low battery flashing begins). 3.50 V should yield 45-90 minute warning.
float HighVThreshold = 3.55;                 // 'High' hysteresis threshold for low battery detection







void setup()
{
  delay(50);                                 // Wait for power to stabilize, then setup I/O pins

  // Setup Power Rails
  pinMode(PosRail, OUTPUT);
  pinMode(NegRail, OUTPUT);
  digitalWrite(PosRail, HIGH);               // Enable +7V Low-DropOut regulator rail
  digitalWrite(NegRail, HIGH);               // Enable -7V Low-DropOut regulator rail
  delay(200);                                // Wait for power rails to stabilise

  // Setup LED to Indicate C5D is On
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LEDState);               // Turn on Green LED

  // Setup Digital Potentiometer
  pinMode(PotPower, OUTPUT);
  pinMode(RightButton, INPUT);               // Declare pushbuttons as inputs
  pinMode(CenterButton, INPUT);
  pinMode(LeftButton, INPUT);
  pinMode(Gain, OUTPUT);
  digitalWrite(PotPower, HIGH);              // Enable DS1882 IC digital potentiometer
  delay(50);                                 // Wait for power to stabilize
  digitalWrite(Gain, GainState);             // Set gain to low

  // Setup DAC
  pinMode(DAC5V, OUTPUT);
  pinMode(DAC33V, OUTPUT);
  pinMode(FilterType, OUTPUT);
  digitalWrite(DAC5V, HIGH);
  delay(25);                                 // Wait for 5V power to stabilize before enabling 3.3V regulators
  digitalWrite(DAC33V, HIGH);
  delay(500);
  digitalWrite(FilterType, FilterState);  

  // Setup Serial
  Wire.begin();                              // Join the I2C bus as master device

  // Sets DS1882 registers to nonvolatile memory, zero-crossing enabled, and 64 tap positions (pg 10 of datasheet)
  // FUTURE: This only needs to be performed once--read register and only write if necessary!
  TempAttenuation = B10000110;
  Wire.beginTransmission(40);                // Set parameters of volume potentiometers
  Wire.write(TempAttenuation);
  Wire.endTransmission();
  delay(25);

  ChangeVolume();
}







void loop()
{
  Right = digitalRead(RightButton);            // Read the up pushbutton
  Center = digitalRead(CenterButton);          // Read the center pushbutton
  Left = digitalRead(LeftButton);              // Read the down pushbutton

  if (Center == HIGH)
  {
    delay(50);                                 // Simple debounce: wait and see if user actually pushed the button
    if (Center == HIGH)                        // If user pressed center pushbutton, toggle mode
    {
      if (Mode < 4)                             // 5 modes available: 0, 1, 2, 3, 4
      {
        Mode++;
      }
      else if (Mode == 4)
      {
        Mode = 2;                              // After Mode 4, revert back to Mode 3
      }
      delay(333);                              // Wait for 1/3 of a second to prevent additional button presses
    }
  }


  // --------------Volume Control---------------
  // First IF statement increases volume. DS1882 has range of 0-63. So:
  //       attenuation = 0 = max volume
  //       attenuation = 62 = minimum volume
  //       attenuation = 63 = mute
  switch(Mode)
  {
  case 0:                                          // Low-Gain mode
    GainState = 0;
    digitalWrite(Gain, GainState);

    digitalWrite(DAC5V, LOW);
    delay(25);                                     // Wait for 5V power to stabilize before enabling 3.3V regulators
    digitalWrite(DAC33V, LOW);

    FilterState = HIGH;
    digitalWrite(FilterType, FilterState);

    if ((Right == HIGH) || (Left == HIGH))
    {
      delay(StepTime);                             // Simple debounce: wait and see if user actually pushed the button
      Right = digitalRead(RightButton);            // Refreshes/Updates the digitalRead variables
      Left = digitalRead(LeftButton);
      if ((Right == HIGH) && (Attenuation > 0))    // Check if button was really pressed and volume isn't already at maximum
      {
        Attenuation--;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Right = digitalRead(RightButton);          // Refreshes/Updates the digitalRead variable
        if ((Right == HIGH) && (Attenuation > 0))
        {
          do                                       // Increase volume faster
          {
            Attenuation--;
            ChangeVolume();
            delay(StepTime);
            Right = digitalRead(RightButton);
          } 
          while ((Right == HIGH) && (Attenuation > 0));
        }
      }

      if ((Left == HIGH) && (Attenuation < 63))    // Check if button was really pressed and volume isn't already at minimum
      {
        Attenuation++;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Left = digitalRead(LeftButton);
        if ((Left == HIGH) && (Attenuation < 63))
        {
          do                                       // Decrease volume faster
          {
            delay(StepTime);
            Attenuation++;
            ChangeVolume();
            Left = digitalRead(LeftButton);
          } 
          while((Left == HIGH) && (Attenuation < 63));
        }
      }
    }
    break;


  case 1:                                          // High-Gain mode
    if ((millis() - LEDTimer) > 125)               // Fast-blinking LED indicator
    {
      LEDState == HIGH;
      FastLED();
      LEDTimer = millis();
    }

    GainState = 1;
    digitalWrite(Gain, GainState);

    digitalWrite(DAC5V, LOW);
    delay(25);                                     // Wait for 5V power to stabilize before enabling 3.3V regulators
    digitalWrite(DAC33V, LOW);

    FilterState = HIGH;
    digitalWrite(FilterType, FilterState);

    if ((Right == HIGH) || (Left == HIGH))
    {
      delay(StepTime);                             // Simple debounce: wait and see if user actually pushed the button
      Right = digitalRead(RightButton);            // Refreshes/Updates the digitalRead variables
      Left = digitalRead(LeftButton);
      if ((Right == HIGH) && (Attenuation > 0))    // Check if button was really pressed and volume isn't already at maximum
      {
        Attenuation--;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Right = digitalRead(RightButton);          // Refreshes/Updates the digitalRead variable
        if ((Right == HIGH) && (Attenuation > 0))
        {
          do                                       // Increase volume faster
          {
            Attenuation--;
            ChangeVolume();
            delay(StepTime);
            Right = digitalRead(RightButton);
          } 
          while ((Right == HIGH) && (Attenuation > 0));
        }
      }

      if ((Left == HIGH) && (Attenuation < 63))    // Check if button was really pressed and volume isn't already at minimum
      {
        Attenuation++;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Left = digitalRead(LeftButton);
        if ((Left == HIGH) && (Attenuation < 63))
        {
          do                                       // Decrease volume faster
          {
            delay(StepTime);
            Attenuation++;
            ChangeVolume();
            Left = digitalRead(LeftButton);
          } 
          while((Left == HIGH) && (Attenuation < 63));
        }
      }
    }
    break;

  case 2:                                          // Low-Gain DAC mode
    if ((millis() - LEDTimer) > 95)                // Fast-blinking LED indicator
    {
      LEDState == HIGH;
      FastLED();
      LEDTimer = millis();
    }

    GainState = 0;
    digitalWrite(Gain, GainState);

    digitalWrite(DAC5V, HIGH);
    delay(25);                                     // Wait for 5V power to stabilize before enabling 3.3V regulators
    digitalWrite(DAC33V, HIGH);

    FilterState = HIGH;
    digitalWrite(FilterType, FilterState);

    if ((Right == HIGH) || (Left == HIGH))
    {
      delay(StepTime);                             // Simple debounce: wait and see if user actually pushed the button
      Right = digitalRead(RightButton);            // Refreshes/Updates the digitalRead variables
      Left = digitalRead(LeftButton);
      if ((Right == HIGH) && (Attenuation > 0))    // Check if button was really pressed and volume isn't already at maximum
      {
        Attenuation--;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Right = digitalRead(RightButton);          // Refreshes/Updates the digitalRead variable
        if ((Right == HIGH) && (Attenuation > 0))
        {
          do                                       // Increase volume faster
          {
            Attenuation--;
            ChangeVolume();
            delay(StepTime);
            Right = digitalRead(RightButton);
          } 
          while ((Right == HIGH) && (Attenuation > 0));
        }
      }

      if ((Left == HIGH) && (Attenuation < 63))    // Check if button was really pressed and volume isn't already at minimum
      {
        Attenuation++;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Left = digitalRead(LeftButton);
        if ((Left == HIGH) && (Attenuation < 63))
        {
          do                                       // Decrease volume faster
          {
            delay(StepTime);
            Attenuation++;
            ChangeVolume();
            Left = digitalRead(LeftButton);
          } 
          while((Left == HIGH) && (Attenuation < 63));
        }
      }
    }
    break;

  case 3:                                          // High-Gain DAC mode
    if ((millis() - LEDTimer) > 65)                // Fast-blinking LED indicator
    {
      LEDState == HIGH;
      FastLED();
      LEDTimer = millis();
    }

    GainState = 1;
    digitalWrite(Gain, GainState);

    digitalWrite(DAC5V, HIGH);
    delay(25);                                     // Wait for 5V power to stabilize before enabling 3.3V regulators
    digitalWrite(DAC33V, HIGH);

    FilterState = HIGH;
    digitalWrite(FilterType, FilterState);

    if ((Right == HIGH) || (Left == HIGH))
    {
      delay(StepTime);                             // Simple debounce: wait and see if user actually pushed the button
      Right = digitalRead(RightButton);            // Refreshes/Updates the digitalRead variables
      Left = digitalRead(LeftButton);
      if ((Right == HIGH) && (Attenuation > 0))    // Check if button was really pressed and volume isn't already at maximum
      {
        Attenuation--;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Right = digitalRead(RightButton);          // Refreshes/Updates the digitalRead variable
        if ((Right == HIGH) && (Attenuation > 0))
        {
          do                                       // Increase volume faster
          {
            Attenuation--;
            ChangeVolume();
            delay(StepTime);
            Right = digitalRead(RightButton);
          } 
          while ((Right == HIGH) && (Attenuation > 0));
        }
      }

      if ((Left == HIGH) && (Attenuation < 63))    // Check if button was really pressed and volume isn't already at minimum
      {
        Attenuation++;
        ChangeVolume();
        delay(VolumeDelay);                        // Long delay before entering fast-volume change period
        Left = digitalRead(LeftButton);
        if ((Left == HIGH) && (Attenuation < 63))
        {
          do                                       // Decrease volume faster
          {
            delay(StepTime);
            Attenuation++;
            ChangeVolume();
            Left = digitalRead(LeftButton);
          } 
          while((Left == HIGH) && (Attenuation < 63));
        }
      }
    }
    break;

  case 4:                                          // DAC filter mode (same volume settings as Case 1)
    if ((millis() - LEDTimer) > 35)                // Faster-blinking LED indicator
    {
      LEDState == HIGH;
      FastLED();
      LEDTimer = millis();
    }

    digitalWrite(DAC5V, HIGH);
    delay(25);                                     // Wait for 5V power to stabilize before enabling 3.3V regulators
    digitalWrite(DAC33V, HIGH);

    if (Right == HIGH)                             // Change filter to Minimum-Phase Filter
    {
      delay(StepTime);
      if (Right == HIGH)
      {
        FilterState = HIGH;
        digitalWrite(FilterType, FilterState);
        FastLED();
      }
    }

    if (Left == HIGH)                              // Change filter to Linear-Phase Filter
    {
      delay(StepTime);
      if (Left == HIGH)
      {
        FilterState = LOW;
        digitalWrite(FilterType, FilterState);
        FastLED();
      }
    }
    break;
  }
}







void ChangeVolume()
{
  // Update first potentiometer gang, where potentiometer address is first two bits of attenuation: 00 ######
  Wire.beginTransmission(40);                      // Transmit to DS1808, device #40 (0x28) = 0101 000, LSB is auto sent by Arduino Wire
  Wire.write(Attenuation);                         // Write wiper value
  Wire.endTransmission();                          // Stop transmitting    

  // Update second potentiometer gang. Same process, where pot address is now: 01 ######.
  // (each I2C transmission should be started stopped individually)
  TempAttenuation = B01000000 | Attenuation;       // Attenuation value is bitwise OR'ed with address byte fpr second pot gang
  Wire.beginTransmission(40);
  Wire.write(TempAttenuation);
  Wire.endTransmission();
}







void FastLED()                                     // Separate block of code for simple LED logic
{
  if(LEDState == HIGH)
  {
    digitalWrite(LED, LEDState);
    LEDState = LOW;
  }

  else if (LEDState == LOW)
  {
    digitalWrite(LED, LEDState);
    LEDState = HIGH;
  }
}







void CheckBattery()
{
  BattVoltage = (float)analogRead(Battery) * 4.95 / 1023;   // Note: 4.95V is imperical value of c5 5V rail

  if(BattVoltage > HighVThreshold)                          // Use of hysteresis to avoid erratic LED toggling
  {
    LEDState = LOW;                                         // flashState toggles if voltage is below the hysteresis threshold,
    LowBatt = false;                                        // lowBatt prevents flashState from toggling when above high threshold
  }
  else if(BattVoltage < LowVThreshold)                      // lowbatt toggles 
  {
    LowBatt = true;
  }

  if((LowBatt == true) && (LEDState == LOW))                // If the battery is low, determine next state of LED
  {
    LEDState = HIGH;
  }
  else
  {
    LEDState = LOW;
  }

  BatteryLED();                                             // Calls the LED function to perform toggles based on battery performance
}







void BatteryLED()
{
  if((LEDState == LOW) && (LowBatt == true))                // If the battery is low, toggle the LED according to flashState
  {
    digitalWrite(LED, LEDState); 
  }

  else if ((LEDState == HIGH) && (LowBatt == true))
  {
    digitalWrite(LED, LEDState); 
  }
  else                                                      // Activate LED as usual if the battery isn't low
  {
    digitalWrite(LED, LOW); 
  }
}


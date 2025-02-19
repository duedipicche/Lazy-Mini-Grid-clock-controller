/* -[ClockSketch v7.2]----------------------------------------------------------------------------------------
   https://www.instructables.com/ClockSketch-V7-Part-I/
   
   pre-configured for:
   Lazy Mini Grid v1 - 17x5
   https://www.instructables.com/Lazy-Mini-Grid/
   
   Arduino UNO/Nano/Pro Mini (AtMega328, 5V, 16 MHz), DS3231 RTC

   January 2023 - Daniel Cikic

   Serial Baud Rates:
   Arduino: 57600
   nodeMCU: 74880
-------------------------------------------------------------------------------------------------------------- */


// comment below to disable serial in-/output and free some RAM
#define DEBUG

// nodeMCU - uncomment to compile this sketch for nodeMCU 1.0 / ESP8266, make sure to select the proper board
// type inside the IDE! This mode is NOT supported and only experimental!
 #define NODEMCU

// useWiFi - enable WiFi support, WPS setup only! If no WPS support is available on a router check settings
// further down, set useWPS to false and enter ssid/password there
 #define USEWIFI

// useNTP - enable NTPClient, requires NODEMCU and USEWIFI. This will also enforce AUTODST.
// Configure a ntp server further down below!
#define USENTP

// RTC selection - uncomment the one you're using, comment all others and make sure pin assignemts for
//DS1302 are correct in the parameters section further down!
//#define RTC_DS1302
//#define RTC_DS1307
//#define RTC_DS3231

// autoDST - uncomment to enable automatic DST switching, check Time Change Rules below!
// #define AUTODST

// FADING - uncomment to enable fading effects for dots/digits, other parameters further down below
#define FADING

// autoBrightness - uncomment to enable automatic brightness adjustments by using a photoresistor/LDR
// #define AUTOBRIGHTNESS

// FastForward will speed up things and advance time, this is only for testing purposes!
// Disables AUTODST, USENTP and USERTC.
// #define FASTFORWARD

/* ----------------------------------------------------------------------------------------------------- */


#include <TimeLib.h>                                 // "Time" by Michael Margolis, used in all configs
#include <EEPROM.h>                                  // required for reading/saving settings to eeprom


/* Start RTC config/parameters-------------------------------------------------------------------------- 
   Check pin assignments for DS1302 (SPI), others are I2C (A4/A5 on Arduino by default)                  
   Currently all types are using the "Rtc by Makuna" library                                             */
#ifdef RTC_DS1302
  #include <ThreeWire.h>
  #include <RtcDS1302.h>
  ThreeWire myWire(7, 6, 8);                                     // IO/DAT, SCLK, CE/RST
  RtcDS1302<ThreeWire> Rtc(myWire);
  #define RTCTYPE "DS1302"
  #define USERTC
#endif

#ifdef RTC_DS1307
  #include <Wire.h>
  #include <RtcDS1307.h>
  RtcDS1307<TwoWire> Rtc(Wire);
  #define RTCTYPE "DS1307"
  #define USERTC
#endif

#ifdef RTC_DS3231
  #include <Wire.h>
  #include <RtcDS3231.h>
  RtcDS3231<TwoWire> Rtc(Wire);
  #define RTCTYPE "DS3231"
  //#define USERTC
#endif

#if !defined ( USERTC )
  #pragma message "No RTC selected, check definitions on top of the sketch!"
#endif
/* End RTC config/parameters---------------------------------------------------------------------------- */


/* Start WiFi config/parameters------------------------------------------------------------------------- */ 
#ifdef USEWIFI
  const bool useWPS = false;          // set to false to disable WPS and use credentials below
  const char* wifiSSID = "SSID";
  const char* wifiPWD = "password";
#endif
/* End WiFi config/parameters--------------------------------------------------------------------------- */


/* Start NTP config/parameters-------------------------------------------------------------------------- 
   Using NTP will enforce autoDST, so check autoDST/time zone settings below!                            */
#ifdef USENTP
  /* I recommend using a local ntp service (many routers offer them), don't spam public ones with dozens 
     of requests a day, get a rtc! ^^                                                                    */
  #define NTPHOST "europe.pool.ntp.org"
  //#define NTPHOST "192.168.2.1"
  #ifndef AUTODST
    #define AUTODST
  #endif
#endif
/* End NTP config/parameters---------------------------------------------------------------------------- */


/* Start autoDST config/parameters ---------------------------------------------------------------------- 
   Comment/uncomment/add TimeChangeRules as needed, only use 2 (tcr1, tcr2), comment out unused ones!     
   Enabling/disabling autoDST will require to set time again, clock will be running in UTC time if autoDST
   is enabled, only display times are adjusted (check serial monitor with DEBUG defined!)                
   This will also add options for setting the date (Year/Month/Day) when setting time on the clock!      */
#ifdef AUTODST
  #include <Timezone.h>                                          // "Timezone" by Jack Christensen
  TimeChangeRule *tcr;
  //-----------------------------------------------
  /* US */
  // TimeChangeRule tcr1 = {"tcr1", First, Sun, Nov, 2, -360};   // utc -6h, valid from first sunday of november at 2am
  // TimeChangeRule tcr2 = {"tcr2", Second, Sun, Mar, 2, -300};  // utc -5h, valid from second sunday of march at 2am
  //-----------------------------------------------
  /* Europe */
  TimeChangeRule tcr1 = {"tcr1", Last, Sun, Oct, 3, 60};         // standard/winter time, valid from last sunday of october at 3am, UTC + 1 hour (+60 minutes) (negative value like -300 for utc -5h)
  TimeChangeRule tcr2 = {"tcr2", Last, Sun, Mar, 2, 120};        // daylight/summer time, valid from last sunday of march at 2am, UTC + 2 hours (+120 minutes)
  //-----------------------------------------------
  Timezone myTimeZone(tcr1, tcr2);
#endif
/* End autoDST config/parameters ----------------------------------------------------------------------- */


/* Start autoBrightness config/parameters -------------------------------------------------------------- */
uint8_t upperLimitLDR = 180;                      // everything above this value will cause max brightness (according to current level) to be used (if it's higher than this)
uint8_t lowerLimitLDR = 50;                       // everything below this value will cause minBrightness to be used
uint8_t minBrightness = 30;                       // anything below this avgLDR value will be ignored
const bool nightMode = false;                     // nightmode true -> if minBrightness is used, colorizeOutput() will use a single color for everything, using HSV
const uint8_t nightColor[2] = { 0, 70 };          // hue 0 = red, fixed brightness of 70, https://github.com/FastLED/FastLED/wiki/FastLED-HSV-Colors
float factorLDR = 1.0;                            // try 0.5 - 2.0, compensation value for avgLDR. Set dbgLDR true & define DEBUG and watch the serial monitor. Looking...
const bool dbgLDR = false;                        // ...for values roughly in the range of 120-160 (medium room light), 40-80 (low light) and 0 - 20 in the dark
#ifdef NODEMCU
  uint8_t pinLDR = 0;                             // LDR connected to A0 (nodeMCU only offers this one)
#else
  uint8_t pinLDR = 1;                             // LDR connected to A1 (in case somebody flashes this sketch on arduino and already has an ldr connected to A1)
#endif
uint8_t intervalLDR = 75;                         // read value from LDR every 75ms (most LDRs have a minimum of about 30ms - 50ms)
uint16_t avgLDR = 0;                              // we will average this value somehow somewhere in readLDR();
uint16_t lastAvgLDR = 0;                          // last average LDR value we got
/* End autoBrightness config/parameters ---------------------------------------------------------------- */


#define SKETCHNAME "ClockSketch v7.2"
#define CLOCKNAME "Lazy Mini Grid v1 - 17x5 resolution"


/* Start button config/pins----------------------------------------------------------------------------- */
#ifdef NODEMCU
  const uint8_t buttonA = 13;                                    // momentary push button, 1 pin to gnd, 1 pin to d7 / GPIO_13
  const uint8_t buttonB = 14;                                    // momentary push button, 1 pin to gnd, 1 pin to d5 / GPIO_14
#else
  const uint8_t buttonA = 3;                                     // momentary push button, 1 pin to gnd, 1 pin to d3
  const uint8_t buttonB = 4;                                     // momentary push button, 1 pin to gnd, 1 pin to d4
#endif
/* End button config/pins------------------------------------------------------------------------------- */


/* Start basic appearance config------------------------------------------------------------------------ */
const bool dotsBlinking = true;                                  // true = only light up dots on even seconds, false = always on
const bool leadingZero = false;                                  // true = enable a leading zero, 9:00 -> 09:00, 1:30 -> 01:30...
uint8_t displayMode = 0;                                         // 0 = 24h mode, 1 = 12h mode ("1" will also override setting that might be written to EEPROM!)
uint8_t colorMode = 0;                                           // different color modes, setting this to anything else than zero will overwrite values written to eeprom, as above
uint16_t colorSpeed = 750;                                       // controls how fast colors change, smaller = faster (interval in ms at which color moves inside colorizeOutput();)
const bool colorPreview = true;                                  // true = preview selected palette/colorMode using "8" on all positions for 3 seconds
const uint8_t colorPreviewDuration = 3;                          // duration in seconds for previewing palettes/colorModes if colorPreview is enabled/true
const bool reverseColorCycling = false;                          // true = reverse color movements
const uint8_t brightnessLevels[3] {100, 140, 210};               // 0 - 255, brightness Levels (min, med, max) - index (0-2) will be saved to eeprom
uint8_t brightness = brightnessLevels[0]; // default brightness if none saved to eeprom yet / first run
bool DATE=false;
#ifdef FADING
  uint8_t fadePixels = 2;                                        // fade pixels, 0 = disabled, 1 = only fade out pixels turned off, 2 = fade old out and fade new in
  uint8_t fadeDelay = 20;                                        // milliseconds between each fading step, 5-25 should work okay-ish
#endif
/* End basic appearance config-------------------------------------------------------------------------- */


/* End of basic config/parameters section */


/* End of feature/parameter section, unless changing advanced things/modifying the sketch there's absolutely nothing to do further down! */


/* library, wifi and ntp stuff depending on above config/parameters */
#ifdef NODEMCU
  #if defined ( USENTP ) && !defined ( USEWIFI )                 // enforce USEWIFI when USENTP is defined
    #define USEWIFI
    #pragma warning "USENTP without USEWIFI, enabling WiFi"
  #endif
  #ifdef USEWIFI
    #include <WiFi.h>
    #include <WiFiUdp.h>
  #endif
#endif

#ifdef USENTP
  #include <NTPClient.h>
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, NTPHOST, 0, 60000);
#endif
/* end library stuff */


/* setting feature combinations/options */
#if defined ( FASTFORWARD )
  bool firstLoop = true;
  #ifdef USERTC
    #undef USERTC
  #endif
  #ifdef USEWIFI
    #undef USEWIFI
  #endif
  #ifdef USENTP
    #undef USENTP
  #endif
  #ifdef AUTODST
    #undef AUTODST
  #endif
#endif
/* setting feature combinations/options */


/* Start of FastLED/clock stuff */
#define LEDSTUFF
#ifdef LEDSTUFF
  #ifdef NODEMCU
    #define FASTLED_ESP8266_RAW_PIN_ORDER                        // this means we'll be using the raw esp8266 pin order -> GPIO_12, which is d6 on nodeMCU
    #define LED_PIN 17                                             // led data in connected to GPIO_12 (d6/nodeMCU)
  #else
    #define FASTLED_ALLOW_INTERRUPTS 0                           // AVR + WS2812 + IRQ = https://github.com/FastLED/FastLED/wiki/Interrupt-problems
    #define LED_PIN 6                                            // led data in connected to d6 (arduino)
  #endif
  
  #define LED_PWR_LIMIT 500                                      // 500mA - Power limit in mA (voltage is set in setup() to 5v) 
  #define LED_DIGITS 4                                           // 4 or 6 digits, HH:MM or HH:MM:SS (unsupported)
  #define LED_COUNT 90                                           // Total number of leds, 90 on LMGv1 (17x5)
  #if ( LED_DIGITS == 6 )
    #define LED_COUNT 90                                         // leds on the 6 digit version (unsupported)
  #endif
  #if ( LED_DIGITS == 6 )
    #define RES_X 17
  #else
    #define RES_X 17
  #endif
  #define RES_Y 5
  #define CHAR_X 3
  #define CHAR_Y 5
  
  #include <FastLED.h>
  
  uint8_t markerHSV[3] = { 0, 127, 20 };                         // this color will be used to "flag" leds for coloring later on while updating the leds
  CRGB leds[LED_COUNT];
  CRGBPalette16 currentPalette;
#endif


// start clock specific config/parameters
#if ( LED_DIGITS == 4 )
  const uint8_t digitPositions[4] = { 0, 4, 10, 14 };            // x coordinates of HH:MM
#endif

#if ( LED_DIGITS == 6 )
  const uint8_t digitPositions[6] = { 0, 4, 10, 14, 20, 24 };    // x coordinates of HH:MM:SS
#endif

const uint8_t digitYPosition = 0;

const uint8_t characters[20][CHAR_X * CHAR_Y] PROGMEM = {
  { 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 1, 1 },        // 0
  { 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 1
  { 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1 },        // 2
  { 1, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 1, 1 },        // 3
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 1 },        // 4
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // 5
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 6
  { 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1 },        // 7
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // 8
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // 9
  { 1, 1, 1, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0 },        // T - some letters from here on (index 10, so won't interfere with digits 0-9)
  { 0, 0, 0, 1, 1, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0 },        // r
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1 },        // y
  { 0, 0, 1, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1 },        // d
  { 1, 1, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 1 },        // C
  { 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0 },        // F
  { 1, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1 },        // m1 - will be drawn 2 times when used with an offset of +2 on x
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0 },        // °
  { 1, 0, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 1 },        // H
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }         // "blank"
};

uint8_t clockStatus = 1;                  // Used for various things, don't mess around with it! 1 = startup
                                          // 0 = regular mode, 1 = startup, 9x = setup modes (90, 91, 92, 93...)

/* these values will be saved to EEPROM:
  0 = index for selected palette
  1 = index for selected brightness level
  2 = displayMode, 12h/24h mode
  3 = colorMode */

/* End of FastLED/clock stuff */
// End clock specific configs/parameters

/* other variables */
uint8_t btnRepeatCounter = 0;         // keeps track of how often a button press has been repeated
/* */


/* -- this is where the fun parts start -------------------------------------------------------------------------------------------------------- */


void setup() {
  #ifdef DEBUG
    while ( millis() < 300 ) {  // safety delay for serial output
      #ifdef NODEMCU
        yield();
      #endif
    }
    #ifdef NODEMCU
      Serial.begin(115200); Serial.println(F("  "));
    #else
      Serial.begin(57600);  Serial.println(F("  "));
    #endif
    #ifdef SKETCHNAME
      Serial.print(SKETCHNAME); Serial.println(F(" starting up..."));
    #endif
    #ifdef CLOCKNAME
      Serial.print("Clock Type: "); Serial.println(CLOCKNAME);
    #endif
    #ifdef RTCTYPE
      Serial.print(F("Configured RTC: ")); Serial.println(RTCTYPE);
    #endif
    #ifdef LEDSTUFF
      Serial.print(F("LED power limit: ")); Serial.print(LED_PWR_LIMIT); Serial.println(F(" mA"));
      Serial.print(F("Total LED count: ")); Serial.println(LED_COUNT);
      Serial.print(F("LED digits: ")); Serial.println(LED_DIGITS);
    #endif
    #ifdef AUTODST
      Serial.println(F("autoDST enabled"));
    #endif
    #ifdef NODEMCU
      Serial.println(F("Configured for nodeMCU"));
      #ifdef USEWIFI
        Serial.println(F("WiFi enabled"));
      #endif
      #ifdef USENTP
        Serial.print(F("NTP enabled, NTPHOST: ")); Serial.println(NTPHOST);
      #endif
    #else
      Serial.println(F("Configured for Arduino"));
    #endif
    #ifdef FASTFORWARD
      Serial.println(F("!! FASTFORWARD defined !!"));
    #endif
    while ( millis() < 600 ) {  // safety delay for serial output
      #ifdef NODEMCU
        yield();
      #endif
    }
  #endif
  
  #ifdef AUTOBRIGHTNESS
    #ifdef DEBUG
      Serial.print(F("autoBrightness enabled, LDR using pin: ")); Serial.println(pinLDR);
    #endif
    pinMode(pinLDR, INPUT);
  #endif
  
  pinMode(buttonA, INPUT_PULLUP);
  pinMode(buttonB, INPUT_PULLUP);

  #ifdef DEBUG
    if ( digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW ) {
      if ( digitalRead(buttonA) == LOW ) {
        Serial.println(F("buttonA is LOW / pressed - check wiring!"));
      }
      if ( digitalRead(buttonB) == LOW ) {
        Serial.println(F("buttonB is LOW / pressed - check wiring!"));
      }
    }
  #endif

  #ifdef LEDSTUFF
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT).setCorrection(TypicalSMD5050).setTemperature(DirectSunlight).setDither(1);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, LED_PWR_LIMIT);
    FastLED.clear();
    FastLED.show();
    #ifdef DEBUG
      Serial.println(F("setup(): Lighting up some leds..."));
    #endif
    for ( uint8_t i = 0; i < LED_DIGITS; i++ ) {
      setPixel(0, i);
    }
    FastLED.show();
  #endif

  #ifdef NODEMCU                                                                              // if building for nodeMCU...
    #ifdef USEWIFI                                                                            // ...and if using WiFi.....
      #ifdef DEBUG
        Serial.println(F("Starting up WiFi..."));
      #endif
      WiFi.mode(WIFI_STA);                                                                    // set WiFi mode to STA...
      if ( useWPS ) {
        WiFi.begin(WiFi.SSID().c_str(),WiFi.psk().c_str());                                   // ...and start connecting using saved credentials...
        #ifdef DEBUG
          Serial.println(F("Using WPS setup / saved credentials"));
        #endif
      } else {
        WiFi.begin(wifiSSID, wifiPWD);                                                        // ...or credentials defined in the USEWIFI config section
        #ifdef DEBUG
          Serial.println(F("Using credentials from sketch"));
        #endif
      }
      unsigned long startTimer = millis();
      uint8_t wlStatus = 0;
      uint8_t counter = 6;
      #ifdef DEBUG
        Serial.print(F("Waiting for WiFi connection... "));
      #endif
      while ( wlStatus == 0 ) {
        if ( WiFi.status() != WL_CONNECTED ) wlStatus = 0; else wlStatus = 1;
        #ifdef LEDSTUFF
          if ( millis() - startTimer >= 1000 ) {
            FastLED.clear();
            showChar(counter, digitPositions[3], digitYPosition);
            FastLED.show();
            if ( counter > 0 ) counter--; else wlStatus = 2;
            startTimer = millis();
            #ifdef DEBUG
                Serial.print(F("."));
            #endif
          }
        #endif
        #ifdef NODEMCU
          yield();
        #endif
      }
      if ( WiFi.status() == WL_CONNECTED ) {                                                  // if status is connected...
        #ifdef USENTP                                                                         // ...and USENTP defined...
          timeClient.begin();                                                                 // ...start timeClient
        #endif
      }
      #ifdef DEBUG
        Serial.println();
        if ( WiFi.status() != 0 ) {
          Serial.print(F("setup(): Connected to SSID: ")); Serial.println(WiFi.SSID());
        } else Serial.println(F("setup(): WiFi connection failed."));
      #endif
    #endif
    EEPROM.begin(512);
  #endif

  #ifdef USERTC
    Rtc.Begin();
    #ifdef DEBUG
      Serial.println(F("setup(): RTC.begin(), 2 second safety delay before"));
      Serial.println(F("         doing any read/write actions!"));
    #endif
    unsigned long tmp_time = millis();
    while ( millis() - tmp_time < 2000 ) {
      #ifdef NODEMCU
        yield();
      #endif
    }
    #ifdef DEBUG
      Serial.println(F("setup(): RTC initialized"));
    #endif
  #else
    #ifdef DEBUG
      Serial.println(F("setup(): No RTC defined!"));
    #endif
  #endif

  #ifdef LEDSTUFF
    FastLED.clear();
    FastLED.show();
    /* eeprom settings */
    #ifdef nodeMCU
      EEPROM.begin(512);
    #endif
    paletteSwitcher();
    brightnessSwitcher();
    colorModeSwitcher();
    displayModeSwitcher();
  #endif

  #ifdef FASTFORWARD
    setTime(21, 59, 50, 30, 6, 2021);            // h, m, s, d, m, y to set the clock to when using FASTFORWARD
  #endif

  #ifdef USENTP
    syncHelper(); 
  #endif

  clockStatus = 0;       // change from 1 (startup) to 0 (running mode)

  #ifdef DEBUG
    printTime();
    Serial.println(F("setup() done"));
    Serial.println(F("------------------------------------------------------"));
  #endif
}


/* MAIN LOOP */

void loop() {
  static uint8_t lastInput = 0;                     // != 0 if any button press has been detected
  static uint8_t lastSecondDisplayed = 0;           // This keeps track of the last second when the display was updated (HH:MM and HH:MM:SS)
  static unsigned long lastCheckRTC = millis();     // This will be used to read system time in case no RTC is defined (not supported!)
  static bool doUpdate = false;                     // Update led content whenever something sets this to true. Coloring will always happen at fixed intervals!
  #ifdef USERTC
     static RtcDateTime rtcTime = Rtc.GetDateTime().Epoch32Time();  // Get time from rtc (epoch)
  #else
    static time_t sysTime = now();                  // if no rtc is defined, get local system time
  #endif
  #ifdef LEDSTUFF
    static uint8_t refreshDelay = 1;                // refresh leds every 5ms
    static long lastRefresh = millis();             // Keeps track of the last led update/FastLED.show() inside the loop
    #ifdef AUTOBRIGHTNESS
    static long lastReadLDR = millis();
    #endif
  #endif
  #ifdef FASTFORWARD
    static unsigned long lastFFStep = millis();     // Keeps track of last time increment if FASTFORWARD is defined
  #endif
  
  if ( lastInput != 0 ) {                                                                  // If any button press is detected...
    if ( btnRepeatCounter < 1 ) {                                                          // execute short/single press function(s)
      #ifdef DEBUG
        Serial.print(F("loop(): ")); Serial.print(lastInput); Serial.println(F(" (short press)"));
      #endif
      if ( lastInput == 1 ) {                                                              // short press button A
        #ifdef LEDSTUFF
          brightnessSwitcher();
        #endif
      }
      if ( lastInput == 2 ) {                                                              // short press button B
        #ifdef LEDSTUFF
          paletteSwitcher();
        #endif
      }
      if ( lastInput == 3 ) {                                                              // short press button A + button B
      }
    } else if ( btnRepeatCounter > 8 ) {                                                   // execute long press function(s)...
      btnRepeatCounter = 1;                                                                // ..reset btnRepeatCounter to stop this from repeating
      #ifdef DEBUG
        Serial.print(F("loop(): ")); Serial.print(lastInput); Serial.println(F(" (long press)"));
      #endif
      if ( lastInput == 1 ) {                                                              // long press button A
        #ifdef LEDSTUFF
          colorModeSwitcher();
        #endif
      }
      if ( lastInput == 2 ) {                                                              // long press button B
        #ifdef LEDSTUFF
          displayModeSwitcher();
        #endif
      }
      if ( lastInput == 3) {                                                               // long press button A + button B
        #ifdef USEWIFI                                                                     // if USEWIFI is defined and...
          if ( useWPS ) {                                                                  // ...if useWPS is true...
            connectWPS();                                                                  // connect WiFi using WPS
          }
        #else                                                                              // if USEWIFI is not defined...
          #ifdef LEDSTUFF
            FastLED.clear();
            FastLED.show();
            setupClock();                                                                  // start date/time setup
          #endif
        #endif
      }
      while ( digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW ) {               // wait until buttons are released again
        #ifdef LEDSTUFF
          if ( millis() % 50 == 0 ) {                                                      // Refresh leds every 50ms to give optical feedback
            colorizeOutput(colorMode);
            FastLED.show();
          }
        #endif
        #ifdef NODEMCU
          yield();
        #endif
      }
    }
  }
  
  #ifdef FASTFORWARD                                                                       // if FASTFORWARD is defined...
    if ( millis() - lastFFStep >= 250 ) {                                                  // ...and 250ms have passed...
      adjustTime(5);                                                                       // ...add 5 seconds to current time
      lastFFStep = millis();
    }
  #endif

  if ( millis() - lastCheckRTC >= 50 ) {                                                   // check rtc/system time every 50ms
    #ifdef USERTC
      rtcTime = Rtc.GetDateTime().Epoch32Time();
      if ( lastSecondDisplayed != second(rtcTime) ) doUpdate = true;
    #else
      sysTime = now();
      if ( lastSecondDisplayed != second(sysTime) ) doUpdate = true;
    #endif
    lastCheckRTC = millis();
  }

  if ( doUpdate ) {                                      // this will update the led array if doUpdate is true because of a new second from the rtc
    #ifdef USERTC
      setTime(rtcTime);                                  // sync system time to rtc every second
      #ifdef LEDSTUFF
        FastLED.clear();                                 // 1A - clear all leds...
        displayTime(rtcTime);                            // 2A - output rtcTime to the led array..
      #endif
      lastSecondDisplayed = second(rtcTime);
    #else
      #ifdef LEDSTUFF
        FastLED.clear();                                 // 1B - clear all leds...
        displayTime(sysTime);                            // 2B - output sysTime to the led array...
      #endif
      lastSecondDisplayed = second(sysTime); 
    #endif
    #ifdef CUSTOMDISPLAY
      displayMyStuff();                                  // 3AB - if customDisplay is defined this will clear the led array again to display custom values...
    #endif
    doUpdate = false;
    #ifdef DEBUG
      if ( second() % 20 == 0 ) {
        printTime();
      }
    #endif
    #ifdef USENTP                                        // if NTP is enabled, resync to ntp server at 0:00:00 utc
      if ( hour() == 0 && minute() == 0 and second() == 0 ) {
        syncHelper();
      }
    #endif
  }

  #ifdef LEDSTUFF
    colorizeOutput(colorMode);                           // 1C, 2C, 3C...colorize the data inside the led array right now...
    #ifdef AUTOBRIGHTNESS
      if ( millis() - lastReadLDR >= intervalLDR ) {     // if LDR is enabled and sample interval has been reached...
        readLDR();                                       // ...call readLDR();
        if ( abs(avgLDR - lastAvgLDR) >= 5 ) {           // if avgLDR has changed for more than +/- 5 update lastAvgLDR
          lastAvgLDR = avgLDR;
          FastLED.setBrightness(avgLDR);
        }
        lastReadLDR = millis();
      }
    #endif
    #ifdef FADING
      pixelFader();
    #endif
    if ( millis() - lastRefresh >= refreshDelay ) {
      FastLED.show();
      lastRefresh = millis();
    }
  #endif

  lastInput = inputButtons();
}


/* */


#ifdef LEDSTUFF

#ifdef CUSTOMDISPLAY
  void displayMyStuff() {
  /* One way to display custom sensor data/other things. displayMyStuff() is then called inside the doUpdate if statement inside
     void loop() - after updating the leds but before calling colorizeOutput() and FastLED.show()                              */
    if ( second() >= 30 && second() < 40 ) {                   // only do something if current second is 30-39
      #ifdef RTC_DS3231                                        // if DS3231 is used we can read the temperature from that for demo purposes here
        float rtcTemp = Rtc.GetTemperature().AsFloatDegC();    // get temperature in °C as float (25.75°C)....
        uint8_t tmp = round(rtcTemp);                          // ...and round (26°C)
      #else
        uint8_t tmp = 99;                                      // get whatever value from whatever sensor into tmp    
      #endif
        FastLED.clear();
        if ( LED_DIGITS == 4 ) {                                  // if 4 digits, display following content:
          showChar(tmp / 10, digitPositions[0], digitYPosition);  // tmp (26°C) / 10 = 2 on position 1 of HH
          showChar(tmp % 10, digitPositions[1], digitYPosition);  // tmp (26°C) % 10 = 6 on position 2 of HH
          showChar(17, digitPositions[2], digitYPosition);        // ° symbol from array digits[][] on position 1 of MM
          showChar(14, digitPositions[3], digitYPosition);        // C from array digits[][] on position 2 of MM
        }
        if ( LED_DIGITS == 6 ) {                                  // if 6 digits....
          showChar(tmp / 10, digitPositions[2], digitYPosition);  // ...do the above using MM:SS positions instead of HH:MM
          showChar(tmp % 10, digitPositions[3], digitYPosition);
          showChar(17, digitPositions[4], digitYPosition);
          showChar(14, digitPositions[5], digitYPosition);
        }
    }
  }
#endif


#ifdef FADING
void fadePixel(uint8_t x, uint8_t y, uint8_t amount, uint8_t fadeType) {
  /* this will check if the first led of a given segment is lit and if it is, will fade by  
     amount using fadeType. fadeType is important because when fading things in that where
     off previously we must avoid setting them black at first - hence fadeLightBy instead
     of fadeToBlack.  */
  uint8_t pixel = calcPixel(x, y);
  if ( leds[pixel] ) {
    if ( fadeType == 0 ) {
      leds[pixel].fadeToBlackBy(amount);
    } else {
      leds[pixel].fadeLightBy(amount);
    }
  }
}


void pixelFader() {
  if ( fadePixels == 0 ) return;
  static unsigned long firstRun = 0;                                                                // time when a change has been detected and fading starts
  static unsigned long lastRun = 0;                                                                 // used to store time when this function was executed the last time
  static boolean active = false;                                                                    // will be used as a flag when to do something / fade pixels
  static uint8_t previousPixels[LED_COUNT] = { 0 };                                                 // all the pixels lit after the last run
  static uint8_t currentPixels[LED_COUNT] = { 0 };                                                  // all the pixels lit right now
  static uint8_t changedPixels[LED_COUNT] = { 0 };                                                  // used to store the differences -> 1 = led has been turned off, fade out, 2 = was off, fade in
  static uint8_t fadeSteps = 15;                                                                    // steps used to fade in or out
  lastRun = millis();
  if ( !active ) {                                                                                  // this will check if....
    firstRun = millis();
    for ( uint8_t x = 0; x < RES_X; x++ ) {                                                         // ...any of the pixels are on....
      for ( uint8_t y = 0; y < RES_Y; y++ ) {
        if ( leds[calcPixel(x, y)] ) {
          currentPixels[calcPixel(x, y)] = 1;
        } else {
          currentPixels[calcPixel(x, y)] = 0;
        }
        if ( currentPixels[calcPixel(x, y)] != previousPixels[calcPixel(x, y)] ) {                  // ...and compare them to the previous displayed pixels.
          active = true;                                                                            // if a change has been detected, set active = true so fading gets executed
          #ifdef DEBUG
            Serial.print(F("pixel at: ")); Serial.print(x);
            Serial.print(F(" / ")); Serial.print(y);
            Serial.print(F(" was "));
          #endif
          if ( currentPixels[calcPixel(x, y)] == 0 ) {
            changedPixels[calcPixel(x, y)] = 1;
            #ifdef DEBUG
              Serial.println(F("ON, is now OFF"));
            #endif
          } else {
            changedPixels[calcPixel(x, y)] = 2;
            #ifdef DEBUG
              Serial.println(F("OFF, is now ON"));
            #endif
          }
        }
      }
    }
  }
  if ( active ) {                                                                                   // this part is executed once a change has been detected....
    static uint8_t counter = 1;
    static unsigned long lastFadeStep = millis();
    for ( uint8_t x = 0; x < RES_X; x++ ) {                                                         // redraw pixels that have turned off, so we can fade them out...
      for ( uint8_t y = 0; y < RES_Y; y++ ) {
        uint8_t pixel = calcPixel(x, y);
        if ( changedPixels[pixel] == 1 ) {
          setPixel(x, y);
        }
      }
    }
    colorizeOutput(colorMode);                                                                      // colorize again after redraw, so colors keep consistent
    for ( uint8_t x = 0; x < RES_X; x++ ) {
      for ( uint8_t y = 0; y < RES_Y; y++ ) {
        uint8_t pixel = calcPixel(x, y);
        if ( changedPixels[pixel] == 1 ) {                                                          // 1 - pixel has turned on, this one has to be faded in
          fadePixel(x, y, counter * ( 255.0 / fadeSteps ), 0);                                      // fadeToBlackBy, segments supposed to be off/fading out
        }
        if ( changedPixels[pixel] == 2 ) {                                                          // 2 - pixel has turned off, this one has to be faded out
          if ( fadePixels == 2 ) {
            fadePixel(x, y, 255 - counter * ( 255.0 / fadeSteps ), 1 );                             // fadeLightBy, pixels supposed to be on/fading in
          }
        }
      }
    }
    if ( millis() - lastFadeStep >= fadeDelay ) {
      counter++;
      lastFadeStep = millis();
    }
    if ( counter > fadeSteps ) {                                                                    // done with fading, reset variables...
      counter = 1;
      active = false;
      for ( uint8_t x = 0; x < RES_X; x++ ) {                                                       // and save current pixels to previousPixels
        for ( uint8_t y = 0; y < RES_Y; y++ ) {
          uint8_t pixel = calcPixel(x, y);
          if ( leds[pixel] ) {
            previousPixels[pixel] = 1;
          } else {
            previousPixels[pixel] = 0;
          }
          changedPixels[pixel] = 0;
        }
      }
      #ifdef DEBUG
        Serial.print(F("pixel fading sequence took "));                                             // for debugging/checking duration - fading should never take longer than 1000ms!
        Serial.print(millis() - firstRun);
        Serial.println(F(" ms"));
      #endif
    }
  }
}
#endif


#ifdef AUTOBRIGHTNESS
void readLDR() {                                                                                            // read LDR value 5 times and write average to avgLDR
  static uint8_t runCounter = 1;
  static uint16_t tmp = 0;
  uint8_t readOut = map(analogRead(pinLDR), 0, 1023, 0, 250);
  tmp += readOut;
  if (runCounter == 5) {
    avgLDR = ( tmp / 5 )  * factorLDR;
    tmp = 0; runCounter = 0;
    #ifdef DEBUG
      if ( dbgLDR ) {
        Serial.print(F("readLDR(): avgLDR value: "));
        Serial.print(avgLDR);
      }
    #endif
    if ( avgLDR < minBrightness ) avgLDR = minBrightness;
    if ( avgLDR > brightness ) avgLDR = brightness;
    if ( avgLDR >= upperLimitLDR && avgLDR < brightness ) avgLDR = brightness;                              // if avgLDR is above upperLimitLDR switch to max current brightness
    if ( avgLDR <= lowerLimitLDR ) avgLDR = minBrightness;                                                  // if avgLDR is below lowerLimitLDR switch to minBrightness
    #ifdef DEBUG
      if ( dbgLDR ) {
        Serial.print(F(" - adjusted to: "));
        Serial.println(avgLDR);
      }
    #endif
  }
  runCounter++;
}
#endif


void setupClock() {
/* This sets time and date (if AUTODST is defined) on the clock/rtc */
  clockStatus = 90;                                                                         // clockStatus 9x = setup, relevant for other functions/coloring
  while ( digitalRead(buttonA) == LOW || digitalRead(buttonB) == LOW ) {                    // do nothing until both buttons are released to avoid accidental inputs right away
    #ifdef NODEMCU
      yield();
    #endif
  }
  tmElements_t setupTime;                                                                   // Create a time element which will be used. Using the current time would
  setupTime.Hour = 12;                                                                      // give some problems (like time still running while setting hours/minutes)
  setupTime.Minute = 0;                                                                     // Setup starts at 12 (12 pm) (utc 12 if AUTODST is defined)
  setupTime.Second = 0;                                                                     // 
  setupTime.Day = 1;                                                                        // date settings only used when AUTODST is defined, but will set them anyways
  setupTime.Month = 1;                                                                      // see above
  setupTime.Year = 23;                                                                      // current year - 2000 (2023 - 2000 = 23)
  #ifdef USERTC
    RtcDateTime writeTime;
  #endif
  #ifdef AUTODST
    clockStatus = 91;                                                                       // 91 = y/m/d setup
    uint8_t y, m, d;
    y = getUserInput(12, 19, 23, 99);                                                       // show Y + blank, get value from 23 - 99 into y
    setupTime.Year = y + 30;                                                                // 2 digit year + 30 (epoch), so we get offset from 1970
    m = getUserInput(16, 19, 1, 12);                                                        // show M, get value from 1 - 12 into m
    setupTime.Month = m;
    if ( m == 2 ) {
      if ( leapYear(y + 2000) ) {                                                           // check for leap year...
        #ifdef DEBUG                                                                        // ...and get according day input ranges for each month
          Serial.println(F("setupClock(): Leap year detected"));
        #endif
        d = getUserInput(13, 19, 1, 29);
      } else {
        d = getUserInput(13, 19, 1, 28);
      }
    } 
    if ( m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12 ) {
      d = getUserInput(13, 19, 1, 31);
    }
    if ( m == 4 || m == 6 || m == 9 || m == 11 ) {
      d = getUserInput(13, 19, 1, 30);      
    }
    setupTime.Day = d;
    #ifdef USERTC
      writeTime = { 2000 + y, setupTime.Month, setupTime.Day, 
                    setupTime.Hour, setupTime.Minute, setupTime.Second };
      Rtc.SetDateTime(writeTime);
      setTime(makeTime(setupTime));
      #ifdef DEBUG
        Serial.println(now());
        Serial.print(F("setupClock(): RTC time/date set to: ")); Serial.println(writeTime);
      #endif
    #else
      setTime(makeTime(setupTime));
    #endif
  #else
    setupTime.Year = 51;
  #endif
  uint8_t lastInput = 0;
  // hours
  while ( lastInput != 2 ) {
    clockStatus = 92;                                                                      // 92 = HH setup
    if ( lastInput == 1 ) {
      if ( setupTime.Hour < 23 ) {
        setupTime.Hour++;
      } else {
        setupTime.Hour = 0;
      }
    }
    displayTime(makeTime(setupTime));
    lastInput = inputButtons();
  }
  lastInput = 0;
  // minutes
  while ( lastInput != 2 ) {
    clockStatus = 93;                                                                      // 93 = MM setup
    if ( lastInput == 1 ) {
      if ( setupTime.Minute < 59 ) {
        setupTime.Minute++;
      } else {
        setupTime.Minute = 0;
      }
    }
    displayTime(makeTime(setupTime));
    lastInput = inputButtons();
  }
  lastInput = 0;
  // seconds
  if ( LED_DIGITS == 6 ) {
    while ( lastInput != 2 ) {
      clockStatus = 94;                                                                    // 94 = SS setup
      if ( lastInput == 1 ) {
        if ( setupTime.Second < 59 ) {
          setupTime.Second++;
        } else {
          setupTime.Second = 0;
        }
      }
      displayTime(makeTime(setupTime));
      lastInput = inputButtons();
    }
    lastInput = 0;
  }
  #ifdef DEBUG
    #ifdef AUTODST
      Serial.print(F("setupClock(): "));
      Serial.print(F("Y/M/D -> "));
      Serial.print(1970 + setupTime.Year); Serial.print(F("/"));
      Serial.print(setupTime.Month); Serial.print(F("/"));
      Serial.println(setupTime.Day);
    #endif
    Serial.print(F("setupClock(): "));
    Serial.print(F("HH:MM:SS -> "));
    #ifdef AUTODST
      Serial.print(F("AUTODST enabled, setting LOCAL time -> "));
    #endif
    if ( setupTime.Hour < 10 ) Serial.print(F("0"));
    Serial.print(setupTime.Hour); Serial.print(F(":"));
    if ( setupTime.Minute < 10 ) Serial.print(F("0"));
    Serial.print(setupTime.Minute); Serial.print(F(":"));
    if ( setupTime.Second < 10 ) Serial.print(F("0"));
    Serial.println(setupTime.Second);
  #endif
  #ifdef USERTC
    writeTime = { 1970 + setupTime.Year, setupTime.Month, setupTime.Day,
                  setupTime.Hour, setupTime.Minute, setupTime.Second };
    #ifdef AUTODST
      time_t t = myTimeZone.toUTC(makeTime(setupTime)); // get UTC time from entered time
      writeTime = { 1970 + setupTime.Year, month(t), day(t),
                    hour(t), minute(t), second(t) };
    #endif
    Rtc.SetDateTime(writeTime);
    setTime(makeTime(setupTime));
    #ifdef DEBUG
      Serial.println(F("setupClock(): RTC time set"));
      Serial.println(makeTime(setupTime));
      printTime();
    #endif
  #else
    #ifdef AUTODST
      time_t t = myTimeZone.toUTC(makeTime(setupTime)); // get UTC time from entered time
      setTime(t);
    #else
      setTime(makeTime(setupTime));
    #endif
  #endif
  clockStatus = 0;
  #ifdef DEBUG
    Serial.println(F("setupClock() done"));
  #endif
}


uint16_t getUserInput(uint8_t sym1, uint8_t sym2, uint8_t startVal, uint8_t endVal) {
/* This will show two symbols on HH and allow to enter a 2 digit value using the buttons
   and display the value on MM.                                                                                        */
  static uint8_t lastInput = 0;
  static uint8_t currentVal = startVal;
  static bool newInput = true;
  if ( newInput ) {
    currentVal = startVal;
    newInput = false;
  }
  while ( lastInput != 2 ) {
    if ( lastInput == 1 ) {
      if ( currentVal < endVal ) {
        currentVal++;
      } else {
        currentVal = startVal;
      }
    }
    FastLED.clear();
    showChar(sym1, digitPositions[0], digitYPosition);
    if ( sym1 == 16 ) showChar(sym1, digitPositions[0] + 2, digitYPosition); // draw index 16 two times with an offset to get a "M"
    showChar(sym2, digitPositions[1], digitYPosition);
    showChar(currentVal / 10, digitPositions[2], digitYPosition);
    showChar(currentVal % 10, digitPositions[3], digitYPosition);
    if ( millis() % 30 == 0 ) {
      colorizeOutput(colorMode);
      FastLED.show();
      }
    lastInput = inputButtons();
  }
  #ifdef DEBUG
    Serial.print(F("getUserInput(): returned ")); Serial.println(currentVal);
  #endif
  lastInput = 0;
  newInput = true;
  return currentVal;
  #ifdef DEBUG
    Serial.print(F("getUserInput(): returned ")); Serial.println(currentVal);
  #endif
}


void colorizeOutput(uint8_t mode) {
/* So far showChar()/setPixel() only set some leds inside the array to values from "markerHSV" but we haven't updated
   the leds yet using FastLED.show(). This function does the coloring of the right now single colored but "insivible"
   output. This way color updates/cycles aren't tied to updating display contents                                      */
  static unsigned long lastRun = 0;
  static unsigned long lastColorChange = 0;
  static uint8_t startColor = 0;
  static uint8_t colorOffset = 0;              // different offsets result in quite different results, depending on the amount of leds inside each segment...
                                               // ...so it's set inside each color mode if required
  /* mode 0 = simply assign different colors with an offset of "colorOffset" to each led based on its x position -> each digit gets its own color */
  if ( mode == 0 ) {
    colorOffset = 24;
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      for ( uint8_t x = digitPositions[pos]; x < digitPositions[pos] + CHAR_X; x++ ) {
        for ( uint8_t y = 0; y < RES_Y; y++ ) {
          if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - pos * colorOffset, brightness, LINEARBLEND);
        }
      }
    }
    // following will color the dots in the same way
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - 8 * colorOffset, brightness, LINEARBLEND);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)] = ColorFromPalette(currentPalette, startColor - 8 * colorOffset, brightness, LINEARBLEND);
      }
    }
  }
  /* mode 1 = simply assign different colors with an offset of "colorOffset" to each led based on its y coordinate -> each digit with a top/down gradient */
  if ( mode == 1 ) {
    colorOffset = 16;
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      for ( uint8_t x = digitPositions[pos]; x < digitPositions[pos] + CHAR_X; x++ ) {
        for ( uint8_t y = 0; y < RES_Y; y++ ) {
          if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
        }
      }
    }
    // following will color the dots in the same way
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)] = ColorFromPalette(currentPalette, startColor - y * colorOffset, brightness, LINEARBLEND);
      }
    }
  }
  /* clockStatus >= 90 is used for coloring output while in setup mode */
  if ( clockStatus >= 90 ) {
    static boolean blinkFlag = true;
    static unsigned long lastBlink = millis();
    static uint8_t b = brightnessLevels[0];
    if ( millis() - lastBlink > 333 ) {                    // blink switch frequency, 3 times a second
      if ( blinkFlag ) {
        blinkFlag = false;
        b = brightnessLevels[1];
      } else {
        blinkFlag = true;
        b = brightnessLevels[0];
      }
      lastBlink = millis();
    }                                                      // unset values = red, set value = green, current value = yellow and blinkinkg 
    for ( uint8_t pos = 0; pos < LED_DIGITS; pos++ ) {
      if ( clockStatus == 91 ) {  // Y/M/D setup
        colorHelper(digitPositions[0], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[0] + 2, digitYPosition, 0, 255, brightness); // offset for double drawn "M"
        colorHelper(digitPositions[1], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[3], digitYPosition, 64, 255, b);
      }
      if ( clockStatus == 92 ) { // hours
        colorHelper(digitPositions[0], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[1], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[2], digitYPosition, 0, 255, brightness);
        colorHelper(digitPositions[3], digitYPosition, 0, 255, brightness);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 0, 255, brightness);
          colorHelper(digitPositions[5], digitYPosition, 0, 255, brightness);
        }
      }
      if ( clockStatus == 93 ) {  // minutes
        colorHelper(digitPositions[0], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[1], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 64, 255, b);
        colorHelper(digitPositions[3], digitYPosition, 64, 255, b);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 0, 255, brightness);
          colorHelper(digitPositions[5], digitYPosition, 0, 255, brightness);
        }
      }
      if ( clockStatus == 94 ) {  // seconds
        colorHelper(digitPositions[0], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[1], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[2], digitYPosition, 96, 255, brightness);
        colorHelper(digitPositions[3], digitYPosition, 96, 255, brightness);
        if ( LED_DIGITS == 6 ) {
          colorHelper(digitPositions[4], digitYPosition, 64, 255, b);
          colorHelper(digitPositions[5], digitYPosition, 64, 255, b);
        }
      }
    }
    uint8_t x = digitPositions[1] + CHAR_X + 1;
    uint8_t x2 = x;
    if ( LED_DIGITS == 6 ) {
      x2 = digitPositions[3] + CHAR_X + 1;
      x2 = 18;
    }
    for ( uint8_t y = 0; y < RES_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)].setHSV(24, 255, brightness);
      if ( LED_DIGITS == 6 ) {
        if ( leds[calcPixel(x2, y)] ) leds[calcPixel(x2, y)].setHSV(24, 255, brightness);
      }
    }
  }

  #ifdef FASTFORWARD
    if ( millis() - lastColorChange > 15 ) {
  #else
    if ( millis() - lastColorChange > colorSpeed ) {
  #endif
    if ( reverseColorCycling ) {
      startColor--;
    } else {
      startColor++;
    }
    lastColorChange = millis();
  }
  #ifdef AUTOBRIGHTNESS
  if ( nightMode && clockStatus == 0 ) {                           // nightmode will overwrite everything that has happened so far...
    for ( uint16_t i = 0; i < LED_COUNT; i++ ) {
      if ( leds[i] ) {
        if ( avgLDR == minBrightness ) {
          leds[i].setHSV(nightColor[0], 255, nightColor[1] );      // and assign nightColor to all lit leds. Default is a very dark red.
          FastLED.setDither(0);
        } else {
          FastLED.setDither(1);
        }
      }
    }
  }
  #endif

/*  // example for time based coloring
  // for coloring based on current times the following will get local display time into
  // checkTime if autoDST is defined as the clock is running in utc time then
  #ifdef AUTODST
    time_t checkTime = myTimeZone.toLocal(now());
  #else
    time_t checkTime = now();
  #endif

  // below if-loop simply checks for a given time and colors everything in green/blue accordingly
  if ( hour(checkTime) > 6 && hour(checkTime) <= 22 ) {           // if hour > 6 AND hour <= 22 ---> 07:00 - 22:59
    for ( uint16_t i = 0; i < LED_COUNT; i++ ) {                  // for each position...
      if ( leds[i] ) {                                            // ...check led and if it's lit...
        leds[i].setHSV(96, 255, brightness);                      // ...redraw with HSV color 96 -> green
      }
    }
  } else {                                                        // ---> 23:00 - 06:59
    for ( uint16_t i = 0; i < LED_COUNT; i++ ) {                  // for each position...
      if ( leds[i] ) {                                            // ...check led and if it's lit...
        leds[i].setHSV(160, 255, brightness);                     // ...redraw with HSV color 160 -> blue
      }
    }
  }
*/

  lastRun = millis();
}


void colorHelper(uint8_t xpos, uint8_t ypos, uint8_t hue, uint8_t sat, uint8_t bri) {
  for ( uint8_t x = xpos; x < xpos + CHAR_X; x++ ) {
    for ( uint8_t y = ypos; y < ypos + CHAR_Y; y++ ) {
      if ( leds[calcPixel(x, y)] ) leds[calcPixel(x, y)].setHSV(hue, sat, bri);
    }
  }
}


void displayTime(time_t t) {
  #ifdef AUTODST
    if ( clockStatus < 90 ) {                             // display adjusted times only while NOT in setup
      t = myTimeZone.toLocal(t);                          // convert display time to local time zone according to rules on top of the sketch
    }
  #endif
  if ( clockStatus >= 90 ) {
    FastLED.clear();
  }
  /* hours */
  if ( displayMode == 0 ) {
    if ( hour(t) < 10 ) {
      if ( leadingZero ) {
        showChar(0, digitPositions[0], digitYPosition);
      }
    } else {
      showChar(hour(t) / 10, digitPositions[0], digitYPosition);
    }
    showChar(hour(t) % 10, digitPositions[1], digitYPosition);
  } else if ( displayMode == 1 ) {
    if ( hourFormat12(t) < 10 ) {
      if ( leadingZero ) {
        showChar(0, digitPositions[0], digitYPosition);
      }
    } else {
      showChar(hourFormat12(t) / 10, digitPositions[0], digitYPosition);
    }
    showChar(hourFormat12(t) % 10, digitPositions[1], digitYPosition);
  }
  /* minutes */
  showChar(minute(t) / 10, digitPositions[2], digitYPosition);
  showChar(minute(t) % 10, digitPositions[3], digitYPosition);
  if ( LED_DIGITS == 6 ) {
    /* seconds */
    showChar(second(t) / 10, digitPositions[4], digitYPosition);
    showChar(second(t) % 10, digitPositions[5], digitYPosition);
  }
  if ( clockStatus >= 90 ) {                                      // in setup modes displayTime will also use colorizeOutput/FastLED.show!
    static unsigned long lastRefresh = millis();
    if ( isAM(t) && displayMode == 1 ) {                          // in 12h mode and if it's AM only light up the upper dots (while setting time)
      showDots(1);
    } else {
      showDots(2);
    }
    if ( millis() - lastRefresh >= 25 ) {
      colorizeOutput(colorMode);
      FastLED.show();
      lastRefresh = millis();
    }
    return;
  }
  /* dots */
  if ( dotsBlinking ) {
    if ( second(t) % 2 == 0 ) {
      showDots(2);
    }
  } else {
    showDots(2);
  }
}


void setPixel(uint8_t x, uint8_t y) {
  uint8_t pixel = 0;
  if ( x < RES_X && y < RES_Y ) {
    pixel = calcPixel(x, y);
    leds[pixel].setHSV(markerHSV[0], markerHSV[1], markerHSV[2]);
  }
}


uint16_t calcPixel(uint8_t x, uint8_t y) {                   // returns led # located at x/y
  uint8_t pixel = 0;
  if ( x % 2 == 0 ) {
    pixel = x / 2 + y * ( RES_X + 1 );
  } else {
    pixel = RES_X + 1 - x + x / 2 + y * ( RES_X + 1 );
  }
  return pixel;
}


void showDots(uint8_t dots) {
  // dots 0 = upper dots, dots 1 = lower dots, dots 2 = all dots
  if ( dots == 0  || dots == 2 ) {
    setPixel(digitPositions[1] + CHAR_X + 1, 3);
    if ( LED_DIGITS == 6 ) {
      setPixel(digitPositions[3] + CHAR_X + 1, 3);
    }
  }
  if ( dots == 1 || dots == 2 ) {
    setPixel(digitPositions[1] + CHAR_X + 1, 1);
    if ( LED_DIGITS == 6 ) {
      setPixel(digitPositions[3] + CHAR_X + 1, 1);
    }
  }
}


void showChar(uint8_t character, uint8_t x, uint8_t y) {        // show digit (0-9) with lower left corner at position x/y 
  for ( uint8_t i = 0; i < ( CHAR_X * CHAR_Y ); i++ ) {
    if ( pgm_read_byte_near(&characters[character][i]) == 1 ) {
      setPixel(x + ( i - ( ( i / CHAR_X ) * CHAR_X ) ), ( y + CHAR_Y - 1 ) - ( i / CHAR_X ) );
    }
  }
}


void paletteSwitcher() {    
/* As the name suggests this takes care of switching palettes. When adding palettes, make sure paletteCount increases
  accordingly.  A few examples of gradients/solid colors by using RGB values or HTML Color Codes  below               */
  static uint8_t paletteCount = 6;                                          
  static uint8_t currentIndex = 0;
  if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
    uint8_t tmp = EEPROM.read(0);
    if ( tmp >= 0 && tmp < paletteCount ) {
      currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
    } else {
      currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 6
    }
    #ifdef DEBUG
      Serial.print(F("paletteSwitcher(): loaded EEPROM value "));
      Serial.println(tmp);
    #endif
  }
  switch ( currentIndex ) {
    case 0: currentPalette = CRGBPalette16( CRGB( 224,   0,  32 ),
                                            CRGB(   0,   0, 244 ),
                                            CRGB( 128,   0, 128 ),
                                            CRGB( 224,   0,  64 ) ); break;
    case 1: currentPalette = CRGBPalette16( CRGB( 224,  16,   0 ),
                                            CRGB( 192,  64,   0 ),
                                            CRGB( 192, 128,   0 ),
                                            CRGB( 240,  40,   0 ) ); break;
    case 2: currentPalette = CRGBPalette16( CRGB::Aquamarine,
                                            CRGB::Turquoise,
                                            CRGB::Blue,
                                            CRGB::DeepSkyBlue   ); break;
    case 3: currentPalette = RainbowColors_p; break;
    case 4: currentPalette = PartyColors_p; break;
    case 5: currentPalette = CRGBPalette16( CRGB::LawnGreen ); break;
  }
  #ifdef DEBUG
    Serial.print(F("paletteSwitcher(): selected palette "));
    Serial.println(currentIndex);
  #endif
  if ( clockStatus == 0 ) {                             // only save selected palette to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(0, currentIndex);
    #ifdef NODEMCU
      EEPROM.commit();
    #endif
    #ifdef DEBUG
      Serial.print(F("paletteSwitcher(): saved index "));
      Serial.print(currentIndex);
      Serial.println(F(" to eeprom"));
    #endif
  }
  if ( currentIndex < paletteCount - 1 ) {
    currentIndex++;    
  } else {
    currentIndex = 0;
  }
  if ( colorPreview ) {
    previewMode();
  }
  #ifdef DEBUG
    Serial.println(F("paletteSwitcher() done"));
  #endif
}


void brightnessSwitcher() {
  static uint8_t currentIndex = 0;
  if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
    uint8_t tmp = EEPROM.read(1);
    if ( tmp >= 0 && tmp < 3 ) {
      currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
    } else {
      currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 2
    }
    #ifdef DEBUG
      Serial.print(F("brightnessSwitcher(): loaded EEPROM value "));
      Serial.println(tmp);
    #endif
  }
  switch ( currentIndex ) {
    case 0: brightness = brightnessLevels[currentIndex]; break;
    case 1: brightness = brightnessLevels[currentIndex]; break;
    case 2: brightness = brightnessLevels[currentIndex]; break;
  }
  #ifdef DEBUG
    Serial.print(F("brightnessSwitcher(): selected brightness index "));
    Serial.println(currentIndex);
  #endif
  if ( clockStatus == 0 ) {                             // only save selected brightness to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(1, currentIndex);
    #ifdef NODEMCU
      EEPROM.commit();
    #endif
    #ifdef DEBUG
      Serial.print(F("brightnessSwitcher(): saved index "));
      Serial.print(currentIndex);
      Serial.println(F(" to eeprom"));
    #endif
  }
  if ( currentIndex < 2 ) {
    currentIndex++;    
  } else {
    currentIndex = 0;
  }
  #ifdef DEBUG {
    Serial.println(F("brightnessSwitcher() done"));
  #endif
}


void colorModeSwitcher() {
  static uint8_t currentIndex = 0;
  if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
    if ( colorMode != 0 ) return;                                           // 0 is default, if it's different on startup the config is set differently, so exit here
    uint8_t tmp = EEPROM.read(3);
    if ( tmp >= 0 && tmp < 2 ) {                                            // make sure tmp < 2 is increased if color modes are added in colorizeOutput()!
      currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
    } else {
      currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 2
    }
    #ifdef DEBUG
      Serial.print(F("colorModeSwitcher(): loaded EEPROM value "));
      Serial.println(tmp);
    #endif
  }
  colorMode = currentIndex;
  #ifdef DEBUG
    Serial.print(F("colorModeSwitcher(): selected colorMode "));
    Serial.println(currentIndex);
  #endif
  if ( clockStatus == 0 ) {                             // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(3, currentIndex);
    #ifdef NODEMCU
      EEPROM.commit();
    #endif
    #ifdef DEBUG
      Serial.print(F("colorModeSwitcher(): saved index "));
      Serial.print(currentIndex);
      Serial.println(F(" to eeprom"));
    #endif
  }
  if ( currentIndex < 1 ) {
    currentIndex++;    
  } else {
    currentIndex = 0;
  }
  if ( colorPreview ) {
    previewMode();
  }
  #ifdef DEBUG {
    Serial.println(F("colorModeSwitcher() done"));
  #endif
}


void displayModeSwitcher() {
  static uint8_t currentIndex = 0;
  if ( clockStatus == 1 ) {                                                 // Clock is starting up, so load selected palette from eeprom...
    if ( displayMode != 0 ) return;                                         // 0 is default, if it's different on startup the config is set differently, so exit here
    uint8_t tmp = EEPROM.read(2);
    if ( tmp >= 0 && tmp < 2 ) {                                            // make sure tmp < 2 is increased if display modes are added
      currentIndex = tmp;                                                   // 255 from eeprom would mean there's nothing been written yet, so checking range...
    } else {
      currentIndex = 0;                                                     // ...and default to 0 if returned value from eeprom is not 0 - 1 (24h/12h mode)
    }
    #ifdef DEBUG
      Serial.print(F("displayModeSwitcher(): loaded EEPROM value "));
      Serial.println(tmp);
    #endif
  }
  displayMode = currentIndex;
  #ifdef DEBUG
    Serial.print(F("displayModeSwitcher(): selected displayMode "));
    Serial.println(currentIndex);
  #endif
  if ( clockStatus == 0 ) {                             // only save selected colorMode to eeprom if clock is in normal running mode, not while in startup/setup/whatever
    EEPROM.put(2, currentIndex);
    #ifdef NODEMCU
      EEPROM.commit();
    #endif
    #ifdef DEBUG
      Serial.print(F("displayModeSwitcher(): saved index "));
      Serial.print(currentIndex);
      Serial.println(F(" to eeprom"));
    #endif
  }
  if ( clockStatus == 0 ) {                             // show 12h/24h for 2 seconds after selected in normal run mode, don't show this on startup (status 1)
    FastLED.clear();
    unsigned long timer = millis();
    while ( millis() - timer <= 2000 ) {
      if ( currentIndex == 0 ) {
        showChar(2, digitPositions[0], digitYPosition);
        showChar(4, digitPositions[1], digitYPosition);
        showChar(18, digitPositions[3], digitYPosition);
      }
      if ( currentIndex == 1 ) {
        showChar(1, digitPositions[0], digitYPosition);
        showChar(2, digitPositions[1], digitYPosition);
        showChar(18, digitPositions[3], digitYPosition);
      }
      colorizeOutput(colorMode);
      if ( millis() % 50 == 0 ) {
        FastLED.show();
      }
      #ifdef NODEMCU
        yield();
      #endif
    }
  }
  if ( currentIndex < 1 ) {
    currentIndex++;    
  } else {
    currentIndex = 0;
  }
  #ifdef DEBUG {
    Serial.println(F("displayModeSwitcher() done"));
  #endif
}


void previewMode() {
/*  This will simply display "8" on all positions, speed up the color cyling and preview the
    selected palette or colorMode                                                            */
  if ( clockStatus == 1 ) return;                           // don't preview when starting up
  unsigned long previewStart = millis();
  uint16_t colorSpeedBak = colorSpeed;
  colorSpeed = 5;
  while ( millis() - previewStart <= uint16_t ( colorPreviewDuration * 1000L ) ) {
    for ( uint8_t i = 0; i < LED_DIGITS; i++ ) { 
      showChar(8, digitPositions[i], digitYPosition);
    }
    colorizeOutput(colorMode);
    FastLED.show();
    #ifdef NODEMCU
      yield();
    #endif
  }
  colorSpeed = colorSpeedBak;
  FastLED.clear();
}
#endif /* LEDSTUFF */


bool leapYear(uint16_t y) {
  boolean isLeapYear = false;
  if (y % 4 == 0) isLeapYear = true;
  if (y % 100 == 0 && y % 400 != 0) isLeapYear = false;
  if (y % 400 == 0) isLeapYear = true;
  if ( isLeapYear ) return true; else return false;
}


uint8_t inputButtons() {
  /* This scans for button presses and keeps track of delay/repeat for user inputs
     Short keypresses will only be returned when buttons are released before repeatDelay
     is reached. This is to avoid constantly sending 1 or 2 when executing a long button
     press and/or multiple buttons.
     Note: Buttons are using pinMode INPUT_PULLUP, so HIGH = not pressed, LOW = pressed! */
  static uint8_t scanInterval = 30;                                 // only check buttons every 30ms
  static uint16_t repeatDelay = 1000;                               // delay in milliseconds before repeating detected keypresses
  static uint8_t repeatRate = 1000 / 10;                            // 10 chars per 1000 milliseconds
  static uint8_t minTime = scanInterval * 2;                        // minimum time to register a button as pressed
  static unsigned long lastReadout = millis();                      // keeps track of when the last readout happened
  static unsigned long lastReturn = millis();                       // keeps track of when the last readout value was returned
  static uint8_t lastState = 0;                                     // button state from previous scan
  uint8_t currentState = 0;                                         // button state from current scan
  uint8_t retVal = 0;                                               // return value, will be 0 if no button is pressed
  static unsigned long eventStart = millis();                       // keep track of when button states are changing
  if ( millis() - lastReadout < scanInterval ) return 0;            // only scan for button presses every <scanInterval> ms
  if ( digitalRead(buttonA) == LOW ) currentState += 1;
  if ( digitalRead(buttonB) == LOW ) currentState += 2;
  if ( currentState == 0 && currentState == lastState ) {
    btnRepeatCounter = 0;
  }
  if ( currentState != 0 && currentState != lastState ) {           // if any button is pressed and different from the previous scan...
    eventStart = millis();                                          // ...reset eventStart to current time
    btnRepeatCounter = 0;                                           // ...and reset global variable btnRepeatCounter
  }
  if ( currentState != 0 && currentState == lastState ) {           // if same input has been detected at least twice (2x scanInterval)...
    if ( millis() - eventStart >= repeatDelay ) {                   // ...and longer than repeatDelay...
      if ( millis() - lastReturn >= repeatRate ) {                  // ...check for repeatRate...
        retVal = currentState;                                      // ...and set retVal to currentState
        btnRepeatCounter++;
        lastReturn = millis();
      } else retVal = 0;                                            // return 0 if repeatDelay hasn't been reached yet
    }
  }
  if ( currentState == 0 && currentState != lastState && millis() - eventStart >= minTime && btnRepeatCounter == 0 ) {
    retVal = lastState;                                             // return lastState if all buttons are released after having been pressed for <minTime> ms
    btnRepeatCounter = 0;
  }
  lastState = currentState;
  lastReadout = millis();
  #ifdef DEBUG                                                      // output some information and read serial input, if available
    uint8_t serialInput = dbgInput();
    if ( serialInput != 0 ) {
      Serial.print(F("inputButtons(): Serial input detected: ")); Serial.println(serialInput);
      retVal = serialInput;
    }
    if ( retVal != 0 ){
      Serial.print(F("inputButtons(): Return value is: ")) ; Serial.print(retVal); Serial.print(F(" - btnRepeatCounter is: ")); Serial.println(btnRepeatCounter);
    }
  #endif
  return retVal;
}


// following will only be included if USENTP is defined
#ifdef USENTP
/* This syncs system time to the RTC at startup and will periodically do other sync related
   things, like syncing rtc to ntp time */
  void syncHelper() {
    static unsigned long lastSync = millis();    // keeps track of the last time a sync attempt has been made
    if ( millis() - lastSync < 60000 && clockStatus != 1 ) return;   // only allow one ntp request per minute
    if ( WiFi.status() != WL_CONNECTED ) {
      #ifdef DEBUG
        Serial.println(F("syncHelper(): No WiFi connection"));
        return;
      #endif
    }
    #ifndef USERTC
      #ifndef USENTP
        #ifdef DEBUG
          Serial.println(F("syncHelper(): No RTC and no NTP configured, nothing to do..."));
          return;
        #endif
      #endif
    #endif
    time_t ntpTime = 0;
    #ifdef USERTC
      RtcDateTime ntpTimeConverted = ntpTime;
    #endif
    if ( clockStatus == 1 ) {                                                                    // looks like the sketch has just started running...
      #ifdef DEBUG
        Serial.println(F("syncHelper(): Initial sync on power up..."));
      #endif
      ntpTime = getTimeNTP();
      #ifdef DEBUG
        Serial.print(F("syncHelper(): NTP result is "));
        Serial.println(ntpTime);
      #endif
      lastSync = millis();
    } else {
      #ifdef DEBUG
        Serial.println(F("syncHelper(): Resyncing to NTP..."));
      #endif
      ntpTime = getTimeNTP();
      #ifdef DEBUG
        Serial.print(F("syncHelper(): NTP result is "));
        Serial.println(ntpTime);
      #endif
      lastSync = millis();
    }
    #ifdef USERTC
      ntpTimeConverted = { year(ntpTime), month(ntpTime), day(ntpTime),
                           hour(ntpTime), minute(ntpTime), second(ntpTime) };
      RtcDateTime rtcTime = Rtc.GetDateTime();                                                 // get current time from the rtc....
      #ifdef DEBUG
        if ( ntpTime > 100 ) {
          Rtc.SetDateTime(ntpTimeConverted);
        }
      #endif
    #else
      time_t sysTime = now();                                                                  // ...or from system
      #ifdef DEBUG
        Serial.println(F("syncHelper(): No RTC configured, using system time"));
        Serial.print(F("syncHelper(): sysTime was "));
        Serial.println(now());
      #endif
      if ( ntpTime > 100 ) { 
        setTime(ntpTime);
      }
    #endif
    #ifdef DEBUG
      Serial.println(F("syncHelper() done"));
    #endif
  }

  
  time_t getTimeNTP() {
    unsigned long startTime = millis();
    time_t timeNTP;
    if ( WiFi.status() != WL_CONNECTED ) {
      #ifdef DEBUG
        Serial.print(F("getTimeNTP(): Not connected, WiFi.status is "));
        Serial.println(WiFi.status());
      #endif
    }                                                                                         // Sometimes the connection doesn't work right away although status is WL_CONNECTED...
    while ( millis() - startTime < 2000 ) {                                                   // ...so we'll wait a moment before causing network traffic
      #ifdef NODEMCU
        yield();
      #endif
    }
    timeClient.update();
    timeNTP = timeClient.getEpochTime();
    if ( timeNTP < 100 ) {
      #ifdef DEBUG
        Serial.print(F("getTimeNTP(): NTP returned ")); Serial.println(timeNTP);
        Serial.print(F(" - trying again..."));
      #endif
    }
    timeClient.update();
    timeNTP = timeClient.getEpochTime();
    if ( timeNTP < 100 ) {
      #ifdef DEBUG
        Serial.print(F("getTimeNTP(): NTP returned ")); Serial.println(timeNTP);
        Serial.print(F(" - giving up"));
      #endif
    }
    #ifdef DEBUG
      Serial.println(F("getTimeNTP() done"));
    #endif
    return timeNTP;
  }
#endif
// ---


// functions below will only be included if DEBUG is defined on top of the sketch
#ifdef DEBUG
  void printTime() {
    /* outputs current system and RTC time to the serial monitor, adds autoDST if defined */
    time_t tmp = now();
    #ifdef USERTC
      RtcDateTime tmp2 = Rtc.GetDateTime().Epoch32Time();
      setTime(tmp2);
      tmp = now();
    #endif
    Serial.println(F("-----------------------------------"));
    Serial.print(F("System time is : "));
    if ( hour(tmp) < 10 ) Serial.print(F("0"));
    Serial.print(hour(tmp)); Serial.print(F(":"));
    if ( minute(tmp) < 10 ) Serial.print(F("0"));
    Serial.print(minute(tmp)); Serial.print(F(":"));
    if ( second(tmp) < 10 ) Serial.print(F("0"));
    Serial.println(second(tmp));
    Serial.print(F("System date is : "));
    Serial.print(year(tmp)); Serial.print("-");
    Serial.print(month(tmp)); Serial.print("-");
    Serial.print(day(tmp)); Serial.println(F(" (Y/M/D)"));
    #ifdef USERTC
      Serial.print(F("RTC time is    : "));
      if ( hour(tmp2) < 10 ) Serial.print(F("0"));
      Serial.print(hour(tmp2)); Serial.print(F(":"));
      if ( minute(tmp2) < 10 ) Serial.print(F("0"));
      Serial.print(minute(tmp2)); Serial.print(F(":"));
      if ( second(tmp2) < 10 ) Serial.print(F("0"));
      Serial.println(second(tmp2));
      Serial.print(F("RTC date is    : "));
      Serial.print(year(tmp2)); Serial.print("-");
      Serial.print(month(tmp2)); Serial.print("-");
      Serial.print(day(tmp2)); Serial.println(F(" (Y/M/D)"));
    #endif
    #ifdef AUTODST
      tmp = myTimeZone.toLocal(tmp);
      Serial.print(F("autoDST time is: "));
      if ( hour(tmp) < 10 ) Serial.print(F("0"));
      Serial.print(hour(tmp)); Serial.print(F(":"));
      if ( minute(tmp) < 10 ) Serial.print(F("0"));
      Serial.print(minute(tmp)); Serial.print(F(":"));
      if ( second(tmp) < 10 ) Serial.print(F("0"));
      Serial.println(second(tmp));
      Serial.print(F("autoDST date is: "));
      Serial.print(year(tmp)); Serial.print("-");
      Serial.print(month(tmp)); Serial.print("-");
      Serial.print(day(tmp)); Serial.println(F(" (Y/M/D)"));
    #endif
    Serial.println(F("-----------------------------------"));
  }

  
  uint8_t dbgInput() {
    /* this catches input from the serial console and hands it over to inputButtons() if DEBUG is defined
       Serial input "7" matches buttonA, "8" matches buttonB, "9" matches buttonA + buttonB */
    if ( Serial.available() > 0 ) {
      uint8_t incomingByte = 0;
      incomingByte = Serial.read();
      if ( incomingByte == 52 ) {          // 4 - long press buttonA
        btnRepeatCounter = 10;
        return 1;
      }
      if ( incomingByte == 53 ) {          // 5 - long press buttonB
        btnRepeatCounter = 10;
        return 2;
      }
      if ( incomingByte == 54 ) {          // 6 - long press buttonA + buttonB
        btnRepeatCounter = 10;
        return 3;
      }
      if ( incomingByte == 55 ) return 1;  // 7 - buttonA
      if ( incomingByte == 56 ) return 2;  // 8 - buttonB
      if ( incomingByte == 57 ) return 3;  // 9 - buttonA + buttonB
    }
    return 0;
  }
 #endif
 // ---


#ifdef USEWIFI
  void connectWPS() {                                                                            // join network using wps. Will try for 3 times before exiting...
    #ifdef DEBUG
      Serial.println(F("connectWPS(): Initializing WPS setup..."));
    #endif
    uint8_t counter = 1;
    static unsigned long startTimer = millis();
    #ifdef LEDSTUFF
      FastLED.clear();
      showChar(10, digitPositions[0], digitYPosition);
      showChar(11, digitPositions[1], digitYPosition);
      showChar(12, digitPositions[2], digitYPosition);
      showChar(counter, digitPositions[3], digitYPosition);
      colorizeOutput(colorMode);
      FastLED.show();
    #endif
    while ( counter < 4 ) {
      #ifdef LEDSTUFF
        if ( millis() % 50 == 0 ) {
          FastLED.clear();
          showChar(10, digitPositions[0], digitYPosition);
          showChar(11, digitPositions[1], digitYPosition);
          showChar(12, digitPositions[2], digitYPosition);
          showChar(counter, digitPositions[3], digitYPosition);
          colorizeOutput(colorMode);
          FastLED.show();
        }
      #endif
      if ( millis() - startTimer > 300 ) {
        #ifdef DEBUG
          Serial.print(F("connectWPS(): Waiting for WiFi/WPS, try "));
          Serial.println(counter);
        #endif
        //WiFi.beginWPSConfig();
        if ( WiFi.SSID().length() <= 0 ) counter++; else counter = 4;
        startTimer = millis();
      }
      #ifdef NODEMCU
        yield();
      #endif
    }
    FastLED.clear();
    startTimer = millis();
    if ( WiFi.SSID().length() > 0 ) {
      #ifdef LEDSTUFF
        FastLED.clear();
        showChar(5, digitPositions[0], digitYPosition);
        showChar(5, digitPositions[1], digitYPosition);
        showChar(1, digitPositions[2], digitYPosition);
        showChar(13, digitPositions[3], digitYPosition);
        colorizeOutput(colorMode);
        FastLED.show();
      #endif
      #ifdef DEBUG
        Serial.print(F("connectWPS(): Connected to SSID: ")); Serial.println(WiFi.SSID());
      #endif
      while ( millis() - startTimer < 2000 ) {
        #ifdef NODEMCU
          yield();
        #endif
      }
      #ifdef USENTP
        clockStatus = 1;
        syncHelper();
        clockStatus = 0;
      #endif USENTP
    } else {
      #ifdef DEBUG
        Serial.println(F("connectWPS(): Failed, no WPS connection established"));
      #endif      
    }
    #ifdef DEBUG
      Serial.println(F("connectWPS() done"));
    #endif
  }
#endif

/* Wooohaa... this one took a bit longer than expected... ^^ /daniel cikic - 07/2021 */

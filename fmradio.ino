

#include <SI4703.h>     // http://www.mathertel.de/Arduino/RadioLibrary.aspx 

//#include <RDSParser.h>  // https://github.com/mathertel/Radio

#include "LowPower.h"

#include <EEPROM.h>

#include <U8g2lib.h>    // https://github.com/olikraus/u8g2/wiki/u8g2reference#reference



//************************* EEPROM internal usage ****************************

#define EEPROM_FreqH        0

#define EEPROM_FreqL        1

#define EEPROM_VOL          2

#define EEPROM_VOLEXT       3


//******************************* Radio **************************************

#define SI4703Address 0x10      // Si4703 I2C Address.

#define resetPin      2      // SI4703 reset pin    

#define FMIN 8500              // FMIN for display only

#define FMAX 11000             // FMAX for display only


//******************************* OLED ***************************************

#define oledVcc         3      // OLED VCC (for power down operation)



//****************************** button switch ***************************

#define PIN_CHANNEL_UP       7

#define PIN_CHANNEL_DOWN     8

#define PIN_VOL_UP           9

#define PIN_VOL_DOWN        10


#define PIN_PUSH            11

#define PIN_HOLD_PWR        12


//********************************** Battery *********************************

#define lowVoltageWarning  3080    // Warn low battery volts.

#define lowVoltsCutoff     2900    // Kill power to display and sleep ATmega328.


//*********************************** icons **********************************

// CHANGE SOMETHING

#define SAVEMARK_X_POS  70


// STEREO

#define STEREO_X_POS  78


//FREQ MODE

#define FREQMODE_X_POS  66


// BATTERY LEVEL

#define BAT_X_POS  90

#define BAT_Y_POS   2

#define BAT_HEIGHT  6

#define BAT_LENGTH  8

#define BAT_LOW     3000

#define BAT_FULL    3200

#define LOW_BAT     "LOW BAT"


// VOLUME LEVEL

#define VOL_X_POS  105

#define VOL_Y_POS   0

#define VOL_WIDTH   3

#define VOL_HEIGHT  8


// RADIO LEVEL

#define RADIO_LEVEL_X_POS 110

#define RADIO_LEVEL_Y_POS 8


#define SCREEN_WIDTH 128

#define SCREEN_HEIGHT 32

#define FREQSTRSIZE 12



//************************** OLED ************************************

//U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0);

U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE, /* clock=*/ SCL, /* data=*/ SDA);   // pin remapping with ESP8266 HW I2C



char tmp[FREQSTRSIZE];

char RDSName[FREQSTRSIZE];

RADIO_FREQ freq;

int volume = 5;

boolean volext = false;

int freqMode = 1;

long volts;

boolean lowVolts = false;

unsigned long timerLowVoltage;

unsigned long timerOLEDOff;

boolean oledIsOn;

RADIO_INFO ri;

//RDSParser rds;

SI4703 radio;

boolean bChanged;

boolean bUseAutoOledOff;


long pwrBtnTimer = 0;

long PWR_BTN_LONG_PRESS_TIME = 3000;


boolean pwrBtnActive = false;

boolean pwrBtnLongPressActive = false;

boolean pwrBtnInitialPush = false;



//volume btn

long volBtnTimer = 0;

long VOL_DISPLAY_TIME = 1500;

boolean volBtnActive = false;



//************************************ SETUP ******************************

void setup() {

  Serial.begin(9600);

  pinMode(PIN_VOL_DOWN, INPUT_PULLUP);

  pinMode(PIN_VOL_UP, INPUT_PULLUP);

  pinMode(PIN_CHANNEL_UP, INPUT_PULLUP);

  pinMode(PIN_CHANNEL_DOWN, INPUT_PULLUP);

  pinMode(PIN_PUSH, INPUT_PULLUP);

  pinMode(PIN_HOLD_PWR, OUTPUT);

  pinMode(oledVcc, OUTPUT);


  digitalWrite(oledVcc, HIGH);


  bUseAutoOledOff = true;

  bChanged = false;

  oledIsOn = false;


  Serial.println(F("Adafruit Radio - Si4703 "));



  //0.5초이상 눌렸는지 체크

  //최초 기동시 turn on 하기 위해 전원 버튼이 눌려지고 있는 상황.

  if (digitalRead(PIN_PUSH) == LOW) {

    delay(200);

    if (digitalRead(PIN_PUSH) == LOW) {

      digitalWrite(PIN_HOLD_PWR, HIGH);

      delay(10);

    }

  }

  else

  {

    return;

  }



  radio.init();


  //radio.debugEnable();    //debug infos to the Serial port

  EEPROMRead();

  sprintf(RDSName, "%d.%d MHz", (unsigned int)freq / 100, (unsigned int)(freq % 100 / 10));


  radio.setBandFrequency(RADIO_BAND_FM, freq);

  radio.setMono(false);

  radio.setMute(false);

  radio.setVolume(volume);

  radio.setVOLEXT(volext);


  // setup the information chain for RDS data.

  //radio.attachReceiveRDS(RDS_process);

  //rds.attachServicenNameCallback(DisplayRDSName);


  //delay(500);

  //u8g2.begin();


  startOled();


}




//******************************* MAIN LOOP ***************************

void loop() {



  if (pwrBtnLongPressActive)

  {

    Serial.println(F("pwrBtnLongPressActive is true "));

    return;

  }


  if (digitalRead(PIN_PUSH) == HIGH) {

    pwrBtnInitialPush = true;               //최초 기동시 turn on 하기 위해 눌렀던 전원 버튼이 릴리즈 된 상황


    if (pwrBtnActive == true) {

      timerOLEDOff = millis();


      if (startOled() == false)

      {

        if (freqMode < 3)

        {

          freqMode++;

        } else {

          freqMode = 1;

        }

      }


      //sprintf(tmp, "test : %d", timerOLEDOff);

      //Serial.println(tmp);



      pwrBtnActive = false;


    }

  }

  else if (pwrBtnInitialPush)

  {



    if (pwrBtnActive == false) {


      pwrBtnActive = true;

      pwrBtnTimer = millis();


    }


    if ((millis() > pwrBtnTimer + PWR_BTN_LONG_PRESS_TIME) && pwrBtnLongPressActive == false) {


      pwrBtnLongPressActive = true;


      if (bChanged)

      {

        EEPROMSave();

      }


      digitalWrite(PIN_HOLD_PWR, LOW);

      byebyeDisplay();

      radio.setMute(true);

      delay(1000);

      stopOled();


      return;

    }

  }


  //u8g2.firstPage();

  volts = readVcc();


  // Serial.println((volts));

  if (bUseAutoOledOff)

  {

    if (millis() > (timerOLEDOff + 15000)) {

      stopOled();

    }

  }


  /*

    if (volts >= lowVoltsCutoff) {

    timerLowVoltage = millis();                             // Battery volts ok so keep resetting cutoff timer.

    }


    if (millis() > (timerLowVoltage + 5000)) {                // more than 5 seconds with low Voltage ?

     stopOled();                            // Kill power to display


     //Wire.beginTransmission(SI4703Address);                 // Put Si4703 tuner into power down mode.

     Wire.write(0x00);

     Wire.write(0x41);

     Wire.endTransmission(true);

     delay(500);

     LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);    // Put ATmega328 to sleep.

    }

  */


  if ((!lowVolts) && (volts < lowVoltageWarning)) {

    lowVolts = true;

  }


  if (digitalRead(PIN_VOL_UP) == LOW) {

    timerOLEDOff = millis();

    if (startOled()) return;


    if (volume < 15) {

      volume++;

    }

    else {

      if (radio.getVOLEXT() == false)

      {

        volume = 1;

        radio.setVolume(volume);

        radio.setVOLEXT(true);

      }

    }


    radio.setVolume(volume);

    bChanged = true;


    volBtnPushCommon();

  }


  if (digitalRead(PIN_VOL_DOWN) == LOW) {

    timerOLEDOff = millis();

    if (startOled()) return;


    if (volume > 0) {

      volume--;

    }


    if (volume == 0) {

      if (radio.getVOLEXT() == true) {

        volume = 15;

        radio.setVOLEXT(false);

      }

    }


    radio.setVolume(volume);

    bChanged = true;



    volBtnPushCommon();

  }


  if (digitalRead(PIN_CHANNEL_UP) == LOW) {

    timerOLEDOff = millis();

    if (startOled()) return;


    switch (freqMode)

    {

      case 1 :

        radio.seekUp(false);

        break;

      case 2 :

        seekAuto(radio.getFrequencyStep());

        break;

      case 3 :

        changeFreq(radio.getFrequencyStep());

        break;

    }


    freq = radio.getFrequency();

    bChanged = true;


  }


  if (digitalRead(PIN_CHANNEL_DOWN) == LOW) {

    timerOLEDOff = millis();

    if (startOled()) return;


    switch (freqMode)

    {

      case 1 :

        radio.seekDown(false);

        break;

      case 2 :

        seekAuto(-radio.getFrequencyStep());

        break;

      case 3 :

        changeFreq(-radio.getFrequencyStep());

        break;

    }


    freq = radio.getFrequency();

    bChanged = true;


  }



  //볼륨 버튼이 눌린 경우 볼륨 display가 될 동안(1초) 메인 화면을 display 하지 않는다.

  if (volBtnActive && (millis() > (volBtnTimer + VOL_DISPLAY_TIME))) {

    volBtnActive = false;

  }


  if (!volBtnActive)

  {

    //Serial.println(F("volBtnActive is false"));

    updateDisplay();

  } else {

    //Serial.println(F("volBtnActive is true"));

  }


  delay(50);

}



/*

  void RDS_process(uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {

  //  g_block1 = block1;

  rds.processData(block1, block2, block3, block4);

  }



  void DisplayRDSName(char *name)

  {

  bool found = false;


  for (uint8_t n = 0; n < 8; n++)

    if (name[n] != ' ') found = true;


  if (found) {

    for (uint8_t n = 0; n < 8; n++) RDSName[n]  = name[n];

    RDSName[8]  = 0;

  }

  }

*/

void BtnPushCommon()

{


}


void volBtnPushCommon()

{

  volumeDisplay();

  volBtnTimer = millis();

  volBtnActive = true;

}


void seekAuto (int step)

{

  while (1) {

    freq = radio.getFrequency() + step;

    if (freq > radio.getMaxFrequency())  freq = radio.getMinFrequency();

    if (freq < radio.getMinFrequency())  freq = radio.getMaxFrequency();

    //radio.clearRDS();

    radio.setFrequency(freq);

    //radio.formatFrequency(RDSName, FREQSTRSIZE);

    updateDisplay();

    delay(10);

    radio.getRadioInfo(&ri);

    if (ri.rssi > 10  || digitalRead(PIN_VOL_UP) == LOW   || digitalRead(PIN_VOL_DOWN) == LOW || digitalRead(PIN_CHANNEL_UP) == LOW || digitalRead(PIN_CHANNEL_DOWN) == LOW) {

      break;

    }

  }

}


void changeFreq (int step)

{

  freq = radio.getFrequency() + step;

  if (freq > radio.getMaxFrequency())  freq = radio.getMinFrequency();

  if (freq < radio.getMinFrequency())  freq = radio.getMaxFrequency();


  radio.setFrequency(freq);


  updateDisplay();

}



void EEPROMSave() {

  freq = radio.getFrequency();

  EEPROM.write(EEPROM_FreqL, (unsigned char)(freq & 0xFF));

  EEPROM.write(EEPROM_FreqH, (unsigned char)((freq >> 8) & 0xFF));

  EEPROM.write(EEPROM_VOL, volume);


  if (radio.getVOLEXT()) {

    EEPROM.write(EEPROM_VOLEXT, 1);

  } else {

    EEPROM.write(EEPROM_VOLEXT, 0);

  }

}


void EEPROMRead() {

  freq =  EEPROM.read(EEPROM_FreqL) + (EEPROM.read(EEPROM_FreqH) << 8) ;

  volume =  EEPROM.read(EEPROM_VOL);

  if (volume > 15) volume = 0;


  if (EEPROM.read(EEPROM_VOLEXT) == 0) {

    volext = false;

  } else {

    volext = true;

  }


}


void stopOled() {

  if (oledIsOn == true) {

    oledIsOn = false;

    digitalWrite(oledVcc, LOW);

  }

}



bool startOled() {


  if (oledIsOn == false) {

    oledIsOn = true;

    digitalWrite(oledVcc, HIGH);

    delay(500);

    u8g2.begin();

    delay(10);

    return true;

  }


  return false;

}




void updateDisplay() {

  if (oledIsOn == false) {

    return;

  }


  int iBatteryRemain = 0;

  radio.getRadioInfo(&ri);


  u8g2.firstPage();


  do {


    u8g2.setFont(u8g2_font_6x13_tr);

    radio.formatFrequency(tmp, FREQSTRSIZE);

    u8g2.drawStr(0, 9, tmp);


    // Change somthing

    //if(bChanged)

    //{

    // u8g2.setFont(u8g2_font_6x10_tr);

    //sprintf(tmp, "!");

    //u8g2.drawStr(SAVEMARK_X_POS,8, tmp);

    //}


    u8g2.setFont(u8g2_font_blipfest_07_tr);

    sprintf(tmp, "M%d", freqMode);

    u8g2.drawStr(FREQMODE_X_POS, 7, tmp);


    // Stereo Status

    if (ri.stereo)

    {

      u8g2.setFont(u8g2_font_blipfest_07_tr);

      sprintf(tmp, "ST");

      u8g2.drawStr(STEREO_X_POS, 7, tmp);

    }


    // BATTERY

    u8g2.drawBox(BAT_X_POS + BAT_LENGTH, BAT_Y_POS + 2, 2, 2);

    u8g2.drawFrame(BAT_X_POS , BAT_Y_POS, BAT_LENGTH, BAT_HEIGHT);

    if (volts > BAT_LOW) {

      iBatteryRemain = ((BAT_LENGTH - 2) * (volts - BAT_LOW)) / (BAT_FULL - BAT_LOW);


      if (iBatteryRemain > 8)

      {

        iBatteryRemain = 8;

      }

      //u8g2.drawBox(BAT_X_POS , BAT_Y_POS, ((BAT_LENGTH-2)*(volts-BAT_LOW))/(BAT_FULL-BAT_LOW), BAT_HEIGHT);

      u8g2.drawBox(BAT_X_POS , BAT_Y_POS, iBatteryRemain, BAT_HEIGHT);

    }


    // VOL

    u8g2.drawFrame(VOL_X_POS, VOL_Y_POS, VOL_WIDTH, VOL_HEIGHT);


    if (radio.getVOLEXT()) {

      u8g2.drawBox(VOL_X_POS, VOL_Y_POS + VOL_HEIGHT - ((volume + 15) / 4), VOL_WIDTH, (volume + 15) / 4);

    } else {

      u8g2.drawBox(VOL_X_POS, VOL_Y_POS + VOL_HEIGHT - (volume / 4), VOL_WIDTH, volume / 4);

    }



    // RADIO

    for (unsigned char i = 0; i < (ri.rssi / 4); i++) {

      u8g2.drawVLine(RADIO_LEVEL_X_POS + (2 * i), RADIO_LEVEL_Y_POS - i, i);

    }




    // u8g2.setFont(u8g2_font_fub14_tf);

    //  if (lowVolts) {

    //    u8g2.drawStr((SCREEN_WIDTH-u8g2.getStrWidth(LOW_BAT))/2,36, LOW_BAT);

    // } else {

    //    //u8g2.drawStr((SCREEN_WIDTH-u8g2.getStrWidth(RDSName))/2,36, RDSName);

    //  }



    //u8g2.setFont(u8g2_font_u8glib_4_tf);

    u8g2.setFont(u8g2_font_blipfest_07_tr);

    u8g2.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 );

    for (RADIO_FREQ f = FMIN; f <= FMAX; f += 100 ) {

      unsigned int x = ((f - FMIN) * 12) / ((FMAX - FMIN) / 10);

      if (f % 500 == 0) {

        u8g2.drawVLine(x + 4, SCREEN_HEIGHT - 8, 8);

        sprintf(tmp, "%d", (unsigned int)f / 100);


        if (f < 10600)

        {

          u8g2.drawStr(x, SCREEN_HEIGHT - 12, tmp);

        } else {

          u8g2.drawStr(x - 1, SCREEN_HEIGHT - 12, tmp);

        }

      } else {

        u8g2.drawVLine(x + 4, SCREEN_HEIGHT - 4, 4);

      }

    }

    unsigned int x = 4 + ((freq - FMIN) * 12) / ((FMAX - FMIN) / 10);

    u8g2.drawTriangle(x, 27, x - 2, 22,  x + 3, 22);


  } while ( u8g2.nextPage() );

}


void volumeDisplay()

{

  if (oledIsOn == false) {

    startOled();

  }


  u8g2.firstPage();


  do {


    u8g2.setFont(u8g2_font_crox5hb_tr);


    if (radio.getVOLEXT()) {

      sprintf(tmp, "VOL %d", volume + 15);

    } else {

      sprintf(tmp, "VOL %d", volume);

    }


    u8g2.drawStr(20, 25, tmp);


  } while ( u8g2.nextPage() );

}


void byebyeDisplay()

{

  if (oledIsOn == false) {

    startOled();

  }


  u8g2.firstPage();


  do {


    u8g2.setFont(u8g2_font_logisoso22_tr   );

    sprintf(tmp, "Bye~");

    u8g2.drawStr(10, 25, tmp);


  } while ( u8g2.nextPage() );

}




// Get Battery Voltage


long readVcc() {

  long result;

  // Read 1.1V reference against AVcc

  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);

  delay(2); // Wait for Vref to settle

  ADCSRA |= _BV(ADSC); // Convert

  while (bit_is_set(ADCSRA, ADSC));

  result = ADCL;

  result |= ADCH << 8;

  result = 1113000L / result; // Back-calculate AVcc in mV

  return result;

}

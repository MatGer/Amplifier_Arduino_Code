/*      -------------------------------WIRING-------------------------------
    Buttons wiring: 5V->PIN | PIN->10K->GND
    Thermistor wiring: Vcc -> Resistor (R) | R -> A7 and Thermistor | Thermistor -> GND
    Pot wiring: Pinout left to right Pins 1,2,3       1=GND 2=Data 3=Vcc
    IR receiver wiring: TSOP34838 Pinout left to right Pins 1,2,3       1=Data 2=GND 3=Vcc
*/
#include <IRremote.h>
#include <FastLED.h>
#include <EEPROM.h>

#define LED_STRIP_PIN 13
#define NUM_LEDS 25
#define BRIGHTNESS 64
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
#define POT_READ A6 //read brightness from pot  
#define IR_PIN 5
#define THERMISTOR_R 100000 //100KΩ
#define THERMISTOR_PIN A7
//These values are in the datasheet
#define RT0 100000   // 100KΩ
#define B 4190      // K

#define LED_PHONO 1
#define LED_TUNER 2
#define LED_AUX 3
#define LED_TAPE 4
#define LED_SRC1 5
#define LED_SRC2 6
#define LED_TAPE_MONITOR 10

CRGB leds[NUM_LEDS];
int EEPROM_source; //address for sources
int EEPROM_tape;  //address for tape monitor
int EEPROM_color;//address for choosing color mode
int indicator_color; //color of indicators
unsigned long hex_value = 0x000000;
IRrecv irrecv(IR_PIN);
decode_results results;
bool a_button_pressed = false;
int br_pot;
int brightness;
uint8_t sinBeat;
int sel;
//temperature control variables
float R1 = 10000;
float RT, VR, ln, TX, T0, VRT;
#define UPDATES_PER_SECOND 100

void setup() {
    pinMode(LED_STRIP_PIN, OUTPUT);  // sets the pin as output 
    FastLED.addLeds<LED_TYPE, LED_STRIP_PIN, COLOR_ORDER>(leds,NUM_LEDS);
    FastLED.setBrightness(55);
    FastLED.setTemperature(Candle);
    Serial.begin(57600);
    
    attachInterrupt(digitalPinToInterrupt(2), colors, RISING);//change color
    attachInterrupt(digitalPinToInterrupt(3), tape_monitor, RISING);//tape monitor
    
    //audio source buttons
    pinMode(14, INPUT);
    pinMode(15, INPUT);
    pinMode(16, INPUT);
    pinMode(17, INPUT);
    pinMode(18, INPUT);
    pinMode(19, INPUT);
    
    // audio source relays
    pinMode(7, INPUT);
    pinMode(8, INPUT);
    pinMode(9, INPUT);
    pinMode(10, INPUT);
    pinMode(11, INPUT);
    pinMode(12, INPUT);
    
    //tape monitor relay-source
    pinMode(3, INPUT);
    pinMode(4, OUTPUT);
    
    //for brightness pot
    pinMode(A6, INPUT);

    //for IR receiver
    pinMode(1,OUTPUT);
    pinMode(6,OUTPUT);
    irrecv.enableIRIn(); // Start the receiver
    
    //change indicator colors on each boot
    indicator_color = EEPROM.read(4);
    if(indicator_color==0){
        hex_value = 0xff0000; //red
    }else if(indicator_color==1){
        hex_value = 0x00ff00; //green
    }else if(indicator_color==2){
        hex_value = 0x0000ff; //blue
    }else{
       indicator_color=0;
       hex_value = 0xff0000; //red
    }
    indicator_color++;
    EEPROM.update(4,indicator_color);
    
    //thermistor
    pinMode(A7, INPUT);
    T0 = 25 + 273.15; 
    delay(4000);
    EEPROM_source = EEPROM.read(0);
    EEPROM_tape = EEPROM.read(1);
    EEPROM_color = EEPROM.read(2);
    
    //read last source
  switcheeprom(EEPROM_source);
  leds[EEPROM_source]=hex_value;
  
  switch (EEPROM_tape){
    case 0:
      digitalWrite(4,LOW);
      break;
    case 1:
      leds[LED_TAPE_MONITOR]=hex_value;
      digitalWrite(4,HIGH);
     break;
   default:
      EEPROM.update(1,0);
      digitalWrite(4,LOW);
      break;
   }

   if(EEPROM_color==255){
    EEPROM_color=0;
    EEPROM.update(2,0);
    sel=EEPROM_color;
   }else{
    sel=EEPROM_color;
   }
}

void loop() {
    //thermistor part
  VRT = analogRead(THERMISTOR_PIN);              //Acquisition analog value of VRT
  VRT = (5.00 / 1023.00) * VRT;      //Conversion to voltage
  VR = 5 - VRT;                     //5=Vcc (supply voltage)
  RT = VRT / (VR / THERMISTOR_R);               //Resistance of RT
  ln = log(RT / RT0);
  TX = (1 / ((ln / B) + (1 / T0))); //Temperature from thermistor
  TX = TX - 273.15;
  Serial.println(TX);
  if(TX>50){
    fill_solid(leds,NUM_LEDS,CRGB::Red);
  }

  br_pot = analogRead(POT_READ);
  brightness = map(br_pot,0,1024,0,255);
  //Serial.println(brightness);
  colormode(sel,brightness);
  if(digitalRead(14)==HIGH){
     EEPROM.update(0,0);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }else if(digitalRead(15)==HIGH){
     EEPROM.update(0,1);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }else if(digitalRead(16)==HIGH){
     EEPROM.update(0,2);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }else if(digitalRead(17)==HIGH){
     EEPROM.update(0,3);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }else if(digitalRead(18)==HIGH){
     EEPROM.update(0,4);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }else if(digitalRead(19)==HIGH){
     EEPROM.update(0,5);
     EEPROM_source = EEPROM.read(0);
     a_button_pressed=true;
  }
  leds[EEPROM_source]=hex_value;
  if(a_button_pressed==true){
    switcheeprom(EEPROM_source);
    a_button_pressed=false;
  }
  if(EEPROM_tape==1){
    leds[LED_TAPE_MONITOR]= hex_value;
  }

  //IR receiver part
  if (irrecv.decode(&results)) {
      //Serial.println(results.value, HEX);
      switch(results.value){
        case 0xE0E048B7:
          digitalWrite(6,HIGH);
          digitalWrite(1,LOW);
          delay(100);
          digitalWrite(6,LOW);
          digitalWrite(1,LOW);
          break;
        case 0xE0E008F7:
          digitalWrite(6,LOW);
          digitalWrite(1,HIGH);
          delay(100);
          digitalWrite(6,LOW);
          digitalWrite(1,LOW);
          break;
        case 0xE0E016E9:
          colors();
          break;
        default:
          break;
      }
      irrecv.resume(); // Receive the next value
  }  
  FastLED.show();
}

void switcheeprom(int source){
    switch (source){
      //digitalWrite to switch the relays
      case 0:
        digitalWrite(7,HIGH);
        digitalWrite(8,LOW);
        digitalWrite(9,LOW);
        digitalWrite(10,LOW);
        digitalWrite(11,LOW);
        digitalWrite(12,LOW);
        break;
      case 1:
        digitalWrite(7,LOW);
        digitalWrite(8,HIGH);
        digitalWrite(9,LOW);
        digitalWrite(10,LOW);
        digitalWrite(11,LOW);
        digitalWrite(12,LOW);
        break;
      case 2:
        digitalWrite(7,LOW);
        digitalWrite(8,LOW);
        digitalWrite(9,HIGH);
        digitalWrite(10,LOW);
        digitalWrite(11,LOW);
        digitalWrite(12,LOW);
        break;
      case 3:
        digitalWrite(7,LOW);
        digitalWrite(8,LOW);
        digitalWrite(9,LOW);
        digitalWrite(10,HIGH);
        digitalWrite(11,LOW);
        digitalWrite(12,LOW);
        break;
      case 4:
        digitalWrite(7,LOW);
        digitalWrite(8,LOW);
        digitalWrite(9,LOW);
        digitalWrite(10,LOW);
        digitalWrite(11,HIGH);
        digitalWrite(12,LOW);        
        break;
      case 5:
        digitalWrite(7,LOW);
        digitalWrite(8,LOW);
        digitalWrite(9,LOW);
        digitalWrite(10,LOW);
        digitalWrite(11,LOW);
        digitalWrite(12,HIGH);
        break;
    }
  }
void colormode(int selection, int bright){
  if(selection==0){//breathing
    if(brightness<=100){
      sinBeat = beatsin8(10,35,100,0,0);
    }else{
      sinBeat = beatsin8(10,35,bright,0,0);
    }
    FastLED.setBrightness(sinBeat);
    fill_solid(leds,NUM_LEDS,CRGB::White);
  }else if(selection==1){//static
    if(bright<10){
      bright=10;
    }
    FastLED.setBrightness(bright);
    fill_solid(leds,NUM_LEDS,CRGB::White);
  }else{//rainbow
    static uint8_t startIndex = 0;
    startIndex = startIndex + 1; /* motion speed */
    FillLEDsFromPaletteColors( startIndex,EEPROM_source,EEPROM_tape);
    FastLED.delay(1000 / UPDATES_PER_SECOND);
  }
}
void tape_monitor(){
  //Serial.println("toggle tape monitor") ;
    if(EEPROM_tape == 0){
      EEPROM.update(1,1);
      digitalWrite(4,HIGH);
      EEPROM_tape = EEPROM.read(1);
    }else{
      EEPROM.update(1,0);
      digitalWrite(4,LOW);
      EEPROM_tape = EEPROM.read(1);
    }
  }
  
  void colors(){
    EEPROM_color++;
    if(EEPROM_color>2){
      EEPROM_color=0;
    }
    EEPROM.update(2,EEPROM_color);
    sel=EEPROM_color;
    //Serial.println(sel); 
  }
  void FillLEDsFromPaletteColors( uint8_t colorIndex, int src, int tape){
    uint8_t brightness = 255; 
    for( int i = 0; i < NUM_LEDS; ++i) {
        leds[i] = ColorFromPalette( RainbowColors_p, colorIndex, brightness, LINEARBLEND);
        colorIndex += 3;
        leds[src]=CRGB::White;
        if(tape==1){
          leds[LED_TAPE_MONITOR]=CRGB::White;
        }
    }
  }

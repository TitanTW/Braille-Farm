/**********************************************************************
Braille Farm — Fertilizer Mixing & Monitoring Controller

  Description:
 - Reads EC, pH
 - Displays all values on an OLED screen.
 - Informs the working status through voice.
 - Control the mixing of fertilizers according to the set ratio.
**********************************************************************/
/***************************************************
*Start program
***************************************************/
// Set fertilizerparameters
#define SET_EC 1.5
#define SET_PH 6.5
#define SET_TEM 28.0
#define SET_HUM 60.0
#define SET_LT 1000.0
#define SET_UDIF 1.02         // 105%
#define SET_LDIF 0.98         // 95%
#define SET_QPUMP_EC 40000L   // EC pump time, 20 second for 1 dif value
#define SET_QPUMP_PH 20000L   // PH pump time, 10 second for 1 dif value
#define SET_MIX_TIME 10000L   // Mix pump time, 10 second

// For Sound player
#include <Arduino.h>
#include <Wire.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
HardwareSerial HWSerial(1);
DFRobotDFPlayerMini dfPlayer;
#define RX_SOUND 5
#define TX_SOUND 6



// For RS485 sensers
#include <ModbusMaster.h>
ModbusMaster node;
#define RX_RS485  18
#define TX_RS485  17
#define ID_PH     1
#define ID_EC     2
#define ID_TH     3
#define ID_LT     4
float VAL_PH, VAL_WT, VAL_EC;
unsigned long TIME_PH, TIME_EC;
byte STATUS_PH = 0, STATUS_EC = 0;
bool IS_FIRST_RUNNING = true;
// LCD1602
#include <LiquidCrystal_I2C.h> // LCD library
LiquidCrystal_I2C lcd(0x27, 16, 2);
#define SDA_PIN 8
#define SCL_PIN 9

// Button
#define PRESS_BUTTON  0
#define RESET_BUTTON  16 //near
#define GREEN1_BUTTON 13 //far
#define GREEN2_BUTTON 14
#define RED_BUTTON    15
byte STATUS_RESET_BUTTON, STATUS_GREEN1_BUTTON, STATUS_GREEN2_BUTTON, STATUS_RED_BUTTON;
int press_cnt = 0;
// Relay
/*
  Pin position       0,1,2,3,4,5,6,7
  Real Pin for code  7,5,3,1,6,4,2,0
*/
#define RELAY1 2 // EC-A
#define RELAY2 41 // EC-B
#define RELAY3 42// pH-Down
#define RELAY4 45// pH-Up
#define RELAY5 46 // Pump

void IRAM_ATTR restartESP() {
 Serial.println("Restarting ESP32...");
 delay(100);
 ESP.restart();
}

void setup() {
 delay(3000);
 Serial.begin(115200);
 Serial.println("Welcome");

 // Set pinmode
 pinMode(RESET_BUTTON, INPUT_PULLUP);
 pinMode(GREEN1_BUTTON, INPUT_PULLUP);
 pinMode(GREEN2_BUTTON, INPUT_PULLUP);
 pinMode(RED_BUTTON, INPUT_PULLUP);
 pinMode(RELAY1, OUTPUT);
 pinMode(RELAY2, OUTPUT);
 pinMode(RELAY3, OUTPUT);
 pinMode(RELAY4, OUTPUT);
 pinMode(RELAY5, OUTPUT);
 delay(500);

 digitalWrite(RELAY1, LOW);
 digitalWrite(RELAY2, LOW);
 digitalWrite(RELAY3, LOW);
 digitalWrite(RELAY4, LOW);
 digitalWrite(RELAY5, LOW);
 // Create interrupt
 attachInterrupt(digitalPinToInterrupt(RESET_BUTTON), restartESP, FALLING);

 // Start Modbus
 Serial2.begin(9600, SERIAL_8N1, RX_RS485, TX_RS485);

 // For led
 Wire.setPins(SDA_PIN, SCL_PIN);
 Wire.begin();
 delay(500);
 lcd.init();
 lcd.backlight();  // Turn on LCD blacklight
 lcd.clear();


 delay(500);


 // For sound player
 HWSerial.begin(9600, SERIAL_8N1, RX_SOUND, TX_SOUND );
 if (!dfPlayer.begin(HWSerial)) {
   Serial.println("DFPlayer Mini not detected!");
   while (true);
 }
 Serial.println("DFPlayer Mini ready!");

 //LCD welcome
 lcd.setCursor(0, 0);
 lcd.print("    Start...    ");
 delay(1000);
}

void loop() {
 delay(1000);
 STATUS_RED_BUTTON = digitalRead(RED_BUTTON);
 STATUS_GREEN1_BUTTON = digitalRead(GREEN1_BUTTON);
 STATUS_GREEN2_BUTTON = digitalRead(GREEN2_BUTTON);
 STATUS_RESET_BUTTON = digitalRead(RESET_BUTTON);

 if(IS_FIRST_RUNNING == true){
   TASK_SENSOR_PH();
   TASK_SENSOR_EC();
   TASK_DISPLAY ();
   IS_FIRST_RUNNING = false;
 }

 if (STATUS_RED_BUTTON == PRESS_BUTTON) {
   press_cnt = 0;
   while (STATUS_RED_BUTTON == PRESS_BUTTON) {
     press_cnt++;
     delay(1000);
     Serial.print("Red button ");
     Serial.println(press_cnt);
     STATUS_RED_BUTTON = digitalRead(RED_BUTTON);

     if (press_cnt >= 3) {
       playSound(1, 30);


       // Sound for VAL_EC
       if (STATUS_EC == 0) {
         playSound(25, 30); // what is this for?
       }
       else if (VAL_EC < SET_EC * SET_LDIF) {
         playSound(8, 30);
       }
       else if (VAL_EC >= SET_EC * SET_LDIF && VAL_EC <= SET_EC * SET_UDIF) {
         playSound(10, 30);
       }
       else {
         playSound(9, 30);
       }

       // Sound for VAL_PH
       if (STATUS_PH == 0) {
         playSound(24, 30); //what is this for?
       }
       else if (VAL_PH < SET_PH * SET_LDIF) {
         playSound(5, 30);
       }
       else if (VAL_PH >= SET_PH * SET_LDIF && VAL_PH <= SET_PH * SET_UDIF) {
         playSound(7, 30);
       }
       else {
         playSound(6, 30);
       }
       TASK_SENSOR();
     }
   }
 }


 if (STATUS_GREEN1_BUTTON == PRESS_BUTTON) {
   press_cnt = 0;
   while (STATUS_GREEN1_BUTTON == PRESS_BUTTON) {
     press_cnt++;
     Serial.print("Green1 button ");
     Serial.println(press_cnt);
     delay(1000);
     STATUS_GREEN1_BUTTON = digitalRead(GREEN1_BUTTON);
     if (press_cnt >= 3) {
       TASK_MIX_EC();
     }
   }
 }

 if (STATUS_GREEN2_BUTTON == PRESS_BUTTON) {
   press_cnt = 0;
   while (STATUS_GREEN2_BUTTON == PRESS_BUTTON) {
     press_cnt++;
     Serial.print("Green2 button ");
     Serial.println(press_cnt);
     delay(1000);
     STATUS_GREEN2_BUTTON = digitalRead(GREEN2_BUTTON);
     if (press_cnt >= 3) {
       TASK_MIX_PH();
     }
   }
 }
}

void TASK_SENSOR() {
 press_cnt = 0;
 TASK_MIX_PUMP();
 TASK_SENSOR_PH();
 TASK_SENSOR_EC();
 TASK_DISPLAY ();
}

void TASK_SENSOR_PH() {
 // pH
 node.begin(ID_PH, Serial2);
 Serial.print(">>> pH read: ");
 uint8_t result1 = node.readHoldingRegisters(0, 2);
 delay(1000);
 if (result1 == node.ku8MBSuccess) {
   STATUS_PH = 1;
   VAL_WT = node.getResponseBuffer(0) / 10.0f;
   VAL_PH = node.getResponseBuffer(1) / 10.0f;
   Serial.print(VAL_PH);
   Serial.print("\t");
   Serial.println(VAL_WT);
 }
 else {
   STATUS_PH = 0;
   VAL_WT = NULL;
   VAL_PH = NULL;
   Serial.println("Can't connect");
 }
}

void TASK_SENSOR_EC() {
 // EC
 node.begin(ID_EC, Serial2);
 Serial.print(">>> EC read: ");
 uint8_t result2 = node.readHoldingRegisters(1, 1);
 delay(1000);
 if (result2 == node.ku8MBSuccess) {
   STATUS_EC = 1;
   VAL_EC = node.getResponseBuffer(0) / 1000.0f;
   Serial.println(VAL_EC);
 }
 else {
   STATUS_EC = 0;
   VAL_EC = NULL;
   Serial.println("Can't connect");
 }
}

void TASK_DISPLAY () {
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("sEC:");
 lcd.print(SET_EC, 1);

 lcd.setCursor(8, 0);
 lcd.print("sPH:");
 lcd.print(SET_PH, 1);

 lcd.setCursor(0, 1);
 lcd.print(" EC:");
 if (STATUS_EC == 0) {
   lcd.print("_._");
 }
 else {
   lcd.print(VAL_EC, 1);
 }

 lcd.setCursor(8, 1);
 lcd.print(" PH:");
 if (STATUS_PH == 0) {
   lcd.print("_._");
 }
 else {
   lcd.print(VAL_PH, 1);
 }
}

void TASK_MIX_EC() {
 TASK_SENSOR_EC();
 press_cnt = 0;
 if (VAL_EC < (SET_EC * SET_LDIF) && STATUS_EC == 1) {
   while (VAL_EC < (SET_EC * SET_LDIF) && STATUS_EC == 1) {
     playSound(8, 30);
     playSound(4, 30);
     float DIF_EC = SET_EC - VAL_EC;
     TIME_EC = DIF_EC * SET_QPUMP_EC;

     Serial.print("EC relay time: ");
     Serial.print(TIME_EC/1000);
     Serial.println(" second");

     // Turn on EC pump
     digitalWrite(RELAY1, HIGH); //changed from LOW to HIGH for all
     digitalWrite(RELAY2, HIGH);
     delay(TIME_EC);
     digitalWrite(RELAY1, LOW);
     digitalWrite(RELAY2, LOW);
     delay(500);

     // Turn on mix pump
     TASK_MIX_PUMP();
     delay(5000);

     TASK_SENSOR_EC();
     TASK_SENSOR_PH();
     TASK_DISPLAY ();
   }
 }
 else {
   playSound(9, 30);
   digitalWrite(RELAY1, LOW);
   digitalWrite(RELAY2, LOW);
   digitalWrite(RELAY5, LOW);
   delay(2000);
 }
}

void TASK_MIX_PH() {
 TASK_SENSOR_PH();
 press_cnt = 0;
 if (VAL_PH < (SET_PH * SET_LDIF) && STATUS_PH == 1) {
   while (VAL_PH < (SET_PH * SET_LDIF) && STATUS_PH == 1) {
     playSound(5, 30);
     playSound(3, 30);
     float DIF_PH = SET_PH - VAL_PH;
     TIME_PH = DIF_PH * SET_QPUMP_PH;

     Serial.print("pH relay time: ");
     Serial.print(TIME_PH/1000);
     Serial.println(" second");

     // Turn on pH Up pump
     digitalWrite(RELAY4, HIGH);
     delay(TIME_PH);
     digitalWrite(RELAY4, LOW);
     delay(500);

     // Turn on mix pump
     TASK_MIX_PUMP();

     TASK_SENSOR_PH();
     TASK_SENSOR_EC();
     TASK_DISPLAY ();
   }
 }
 else if (VAL_PH > (SET_PH * SET_UDIF)) {
   while (VAL_PH > (SET_PH * SET_UDIF)) {
     playSound(7, 30);
     playSound(2, 30);
     float DIF_PH = VAL_PH - SET_PH;
     TIME_PH = DIF_PH * SET_QPUMP_PH;

     Serial.print("pH relay time: ");
     Serial.print(TIME_PH/1000);
     Serial.println(" second");

     // Turn on pH Up pump
     digitalWrite(RELAY3, HIGH); //on
     delay(TIME_PH);
     digitalWrite(RELAY3, LOW); //off
     delay(500);

     // Turn on mix pump
     TASK_MIX_PUMP();

     TASK_SENSOR_PH();
     TASK_SENSOR_EC();
     delay(5000);
     TASK_DISPLAY ();
   }
 }
 else {
   playSound(6, 30);
   digitalWrite(RELAY3, LOW);
   digitalWrite(RELAY4, LOW);
   digitalWrite(RELAY5, LOW);
   delay(2000);
 }
}

void TASK_MIX_PUMP() {
 digitalWrite(RELAY5, HIGH); //on
 delay(SET_MIX_TIME);
 digitalWrite(RELAY5, LOW);// off
 delay(SET_MIX_TIME);
}

void playSound(int sound_no, int volume_lv) {
 dfPlayer.volume(volume_lv);
 dfPlayer.play(sound_no);
 Serial.print(F("Play sound no." ));
 Serial.println(sound_no);
 delay(5000);
}

/***************************************************
*End program
***************************************************/

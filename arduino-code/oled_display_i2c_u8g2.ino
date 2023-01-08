/*
 * Examples of usage:
 * https://home.et.utwente.nl/slootenvanf/2019/11/15/using-oled-display-with-arduino/
 * https://home.et.utwente.nl/slootenvanf/2019/04/04/displays/
 * 
 * You must install the u8g2 library before you use this sketch:
 * Select Tools > Manage Libraries, then search for "U8g2" and install the library.
 * 
 * Learn more about the use of library U8g2:
 * https://github.com/olikraus/u8g2/wiki/u8x8reference
 * This example is based on the original "hello world" example which comes with the library
 * (File > Examples > U8g2 > u8x8 > HelloWorld)
 * 
 * This sketch be used in combination with the EVShield too (in hardware i2c mode).
 * 
 * Connections (hardware i2c):
 * Display: Arduino:
 * GND      GND
 * VCC      VCC (3.3 or 5V)
 * SDA      SDA (pin A4)
 * SCL      SCL (pin A5)
 */
#include <U8g2lib.h>
#include "Rotary.h"
#include "Menus.h"
#include <EEPROM.h>

#include "driver/gpio.h"
#include "driver/can.h"

//Serial rs485
#include <String.h>
#define RXD2 D11
#define TXD2 D10

HardwareSerial rs485(2);
int rs485CycleTime = 2000;
int rs485PublishTime = 0;
boolean checkingVoltages = false;
int victron_bms_connected_EEPROM_adr = 265;
boolean victron_bms_connected = false;

#include <SoftwareSerial.h>
#define RXD1 MISO
#define TXD1 MOSI
SoftwareSerial victron_rs485;
boolean block_rs485 = false;
int checking_bms = 0;

// dummy test Values of Battery
int charge = 0; //percentage
int residual_capacity = 100; // Ah
int nominal_capacity = 105; // Ah
int cell_count = 4;
int * cellVoltages; //mV
boolean discharging = false;
boolean charging = true;
int current = 0; // 0.1A [-250|250] "A"
int voltage = 0; // 0.1V [0|327.67] "V"
int temperature = 00; // 0.1 "°C"

//Can Values
int CAN_Battery_T1 = 210;       //(0.1,0) [-50|75] "°C" Vector__XXX
int CAN_Battery_charge_voltage = 256; // 0,0 - 500,0 V //Maximum charging voltage
int CAN_Battery_charge_current_limit = 600; //0,0 - 500,0 A
int CAN_Battery_discharge_current_limit = 350;  // -500,0 - 0,0  A
int CAN_Battery_discharge_voltage = 230; // 0,0 - 6553,5 V //maximum discharge voltage
byte CAN_SoH = 100; // Battery State of Health

const int CAN_Cylcetime = 1000;
unsigned long canpublishtime = 0;
boolean can_enabled = false;
int can_enabled_EEPROM_adr = 192;

//device_id
int deviceID = 0;
int deviceID_EEPROM_adr = 0;

// rotary encoder
Rotary rotary = Rotary(D2, D3);
int counter = 0;

const int BUTTONPIN = D5;
boolean button_active = true;
int button_delay = 0;
int button_start = millis();
int button_long_press_delay = 500;

// display
// software i2c:
//U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(7, 8, U8X8_PIN_NONE);
// hardware i2c:
U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0);


int display_height = 0;
int display_width = 0;

boolean display_active = true;
int display_turnoff_delay = 100000; // screen turn off delay in milliseconds
int display_turnoff_delay_EEPROM_adr = 64;
int display_turnoff = display_turnoff_delay;

boolean setting_selected = false;

void setup() {
  //rotary encoder
  Serial.begin(9600);
  attachInterrupt(D2, rotate, CHANGE);
  attachInterrupt(D3, rotate, CHANGE);
  pinMode(BUTTONPIN, INPUT_PULLUP);
  attachInterrupt(BUTTONPIN, changeMenu, FALLING);

  //display
  display.begin();
  display.setFontMode(0);
  display.setDrawColor(1);
  display.setFont(u8g2_font_t0_11_tf);
  display_height = display.getDisplayHeight();
  display_width = display.getDisplayWidth();
  display.clearBuffer();
  display.sendBuffer();

  //CAN
  setup_can_driver();

  //device_id
  EEPROM.begin(256);
  
  deviceID = EEPROM.read(deviceID_EEPROM_adr) % 32;
  Serial.println((String)"device ID (from EEPROM):" + EEPROM.readInt(deviceID_EEPROM_adr));
  display_turnoff_delay = (EEPROM.readInt(display_turnoff_delay_EEPROM_adr)>1000)? EEPROM.readInt(display_turnoff_delay_EEPROM_adr) : 1000;
  Serial.println((String) "Display Turn off delay (from EEPROM):" + EEPROM.readInt(display_turnoff_delay_EEPROM_adr));
  can_enabled = EEPROM.readBool(can_enabled_EEPROM_adr);
  Serial.println((String) "Can Enabled (from EEPROM):" + EEPROM.readBool(can_enabled_EEPROM_adr));
  victron_bms_connected = EEPROM.readBool(victron_bms_connected_EEPROM_adr);
  Serial.println((String) "victron_bms_connected  (from EEPROM):" + EEPROM.readBool(victron_bms_connected_EEPROM_adr));
  //delay(1000);

  //rs485
  rs485.begin(9600, SERIAL_8N1, RXD2, TXD2);
  //rs485.endTransmission();
  victron_rs485.begin(9600, SWSERIAL_8N1, RXD1, TXD1);

  //Voltages
  int * cellVoltagesTemp = new int  [cell_count];
  for( int i= 0; i<cell_count; i++){
    cellVoltagesTemp[i] = 0;
  }
  cellVoltages = cellVoltagesTemp;
  delete[] cellVoltagesTemp;

  //Serial.println(getXtalFrequencyMhz());
}

Pages page = SoC;

inline Pages& operator++(Pages& page, int) {
    int i = static_cast<int>(page)+1;
    switch(i){
      case 1: 
        break;
      case 2:
        i = 0;
        break;
    }
    page = static_cast<Pages>(i);
    return page;
}

inline Pages& operator--(Pages& page, int) {
    const int i = static_cast<int>(page)-1;

    if (i < 0) {
        page = static_cast<Pages>(1);
    } else {
        page = static_cast<Pages>((i) % 1);
    }
    return page;
}



Menus menus = pages;

inline Menus& operator++(Menus& menus, int) {
    int i = static_cast<int>(menus)+1;
    switch(i){
      case 1: 
        break;
      case 2:
        i = 0;
        break;
    }
    menus = static_cast<Menus>(i);
    return menus;
}

inline Menus& operator--(Menus& menus, int) {
    const int i = static_cast<int>(menus)-1;

    if (i < 0) {
        menus = static_cast<Menus>(1);
    } else {
        menus = static_cast<Menus>((i) % 1);
    }
    return menus;
}

SoC_Menus soc_menu = page_view;

inline SoC_Menus& operator++(SoC_Menus& soc_menu, int) {
    int i = static_cast<int>(soc_menu)+1;
    switch(i){
      case 1: 
        break;
      case 2:
        i = 0;
        break;
    }
    soc_menu = static_cast<SoC_Menus>(i);
    return soc_menu;
}

inline SoC_Menus& operator--(SoC_Menus& soc_menu, int) {
    const int i = static_cast<int>(soc_menu)-1;

    if (i < 0) {
        soc_menu = static_cast<SoC_Menus>(1);
    } else {
        soc_menu = static_cast<SoC_Menus>((i) % 1);
    }
    return soc_menu;
}

Settings settings_state = normal_view;

inline Settings& operator++(Settings& settings_state, int) {
    int i = static_cast<int>(settings_state)+1;
    settings_state = static_cast<Settings>(i % 2);
    return settings_state;
}

inline Settings& operator--(Settings& settings_state, int) {
    const int i = static_cast<int>(settings_state)-1;

    if (i < 0) {
        settings_state = static_cast<Settings>(1);
    } else {
        settings_state = static_cast<Settings>((i) % 2);
    }
    return settings_state;
}

SettingsOptions options = screen_off_delay;

inline SettingsOptions& operator++(SettingsOptions& options, int) {
    int i = static_cast<int>(options)+1;
    options = static_cast<SettingsOptions>(i % 5);
    return options;
}

inline SettingsOptions& operator--(SettingsOptions& options, int) {
    const int i = static_cast<int>(options)-1;

    if (i < 0) {
        options = static_cast<SettingsOptions>(4);
    } else {
        options = static_cast<SettingsOptions>(i % 5);
    }
    return options;
}





int scroll = 0;

TaskHandle_t sendCanMessagesHandle;
TaskHandle_t checkBMSHandle;

//byte messageByteArray [7] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};


void loop() {
  int time_start = millis();
  
  drawDisplay();

  
  if(!button_active){
    button_active = (button_delay<0) ? true : false;
    button_delay = (button_active) ? 0 : button_delay - (millis() - time_start);
  }
  display_turnoff = (display_active) ? display_turnoff - (millis() - time_start) : display_turnoff;
  if(display_turnoff<0){
    display_active = false;
    display.setPowerSave(!display_active);
  }
  else {
    display_active = true;
    display.setPowerSave(!display_active);
  }

  //CAN Sending 
  if((canpublishtime == 0 || (millis() >= (canpublishtime + CAN_Cylcetime))) && can_enabled) {
    
    //Serial.println("Sending can info");
    canpublishtime = millis();
    xTaskCreatePinnedToCore(
      sendCanMessages, /* Function to implement the task */
      "sendCanMessages", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &sendCanMessagesHandle,  /* Task handle. */
      0); /* Core where the task should run */
    
    
  }

  //block_rs485 = true; 
  //RS485 BMS check
  if((rs485PublishTime == 0 || (millis() >= (rs485PublishTime + rs485CycleTime))) && (display_active || can_enabled)) {
   // checking_bms = checking_bms +1 % 3;
//    Serial.println(display_active);
//    Serial.println(display_turnoff);
//    
    rs485PublishTime = millis();


    xTaskCreatePinnedToCore(
      checkBMS, /* Function to implement the task */
      "checkBMS", /* Name of the task */
      10000,  /* Stack size in words */
      NULL,  /* Task input parameter */
      0,  /* Priority of the task */
      &checkBMSHandle,  /* Task handle. */
      0); /* Core where the task should run */

  } 

/*
  if (victron_rs485.available()) {
    rs485.write(victron_rs485.read());
  }

  if (rs485.available()) {
    victron_rs485.write(rs485.read());
  }*/
  
  
  //delay(1000); // wait one tenth of a second
//  charge = (charge +1)  % 100;
//  voltage = random(220,256);
//  current = ((current + 2000 + 2) % 4000) -2000;
//  residual_capacity = (residual_capacity +1) % 305;
}

void checkBMS(void * pvParameters){
  Serial.println("checkBMS Triggered");
  // rs485
  byte  _incommingByte[128];
  int   _cnt_byte=0;
  if(!victron_bms_connected){
    if(!checkingVoltages){
      rs485.write(0xDD);
      rs485.write(0xA5);
      rs485.write(0x03); // request basic information
      rs485.write(0x00);
      rs485.write(0xFF);
      rs485.write(0xFD);
      rs485.write(0x77);
      checkingVoltages = !checkingVoltages;
    } else {
      rs485.write(0xDD);
      rs485.write(0xA5);
      rs485.write(0x04); // request voltages
      rs485.write(0x00);
      rs485.write(0xFF);
      rs485.write(0xFC);
      rs485.write(0x77);
      checkingVoltages = !checkingVoltages;
    }
  }
  //printf(String((char*) messageByteArray));
  

  if(rs485.available()){     //receive data from rs485   
      
    //clear buffer
    for(int i=0; i<sizeof(_incommingByte); i++) _incommingByte[i] = 0x00;
    
    
    //collect byte data 
    _cnt_byte=0; 
    while(rs485.available()>0){
        byte b8 = rs485.read();
        _incommingByte[_cnt_byte++] = b8;//rs485.read();
        //Serial.println(_cnt_byte);
        //Serial.println(_incommingByte[_cnt_byte],HEX);
        //Serial.println(b8,HEX);
        if(b8 == 119){
          //Serial.println("break");
          break;
        }
      }
           
    //debug monitor
     //for(int i=0; i < _cnt_byte; i++){
     //   Serial.print("_incommingByte["); Serial.print(i); Serial.print("] = "); Serial.print(_incommingByte[i],DEC); Serial.print("[0x"); Serial.print(_incommingByte[i],HEX); Serial.println("]");
     //}
     if(_incommingByte[1] == 3){
       voltage = (((int)round((_incommingByte[4] << 8 | _incommingByte[5]))*0.1));
       Serial.print("voltage: ");
       Serial.println(voltage);
       current = (((int)round(((int16_t)(_incommingByte[6] << 8 | _incommingByte[7])))*0.1));
       Serial.print("current: ");
       Serial.println(current);
       residual_capacity = (((int)round((_incommingByte[8] << 8 | _incommingByte[9]))*0.01));
       Serial.print("residual_capacity: ");
       Serial.println(residual_capacity);
       nominal_capacity = (((int)round((_incommingByte[10] << 8 | _incommingByte[11]))*0.01));
       Serial.print("nominal_capacity: ");
       Serial.println(nominal_capacity);
       charge = (int)round(_incommingByte[23]);
       Serial.print("charge: ");
       Serial.println(charge);
       charging = (_incommingByte[24] & 0x01) == 0x01 ? true : false;
       discharging = (_incommingByte[24] & 0x02) == 0x02 ? true : false; 
       cell_count = _incommingByte[25];
       Serial.println("cell_count:");
       Serial.println(cell_count);
       int ntc_numbers = _incommingByte[26];
       
       int temperatures [ntc_numbers];
       int j = 0;
       for( int i = 0; i< ntc_numbers; i++){
         temperatures [i] = ((int)(_incommingByte[27+j] << 8 | _incommingByte[28+j]) - 2731);
         j+=2;
       }
       temperature = temperatures[0];
       for ( int i = 1; i< ntc_numbers; i++){
          temperature += temperatures[i]; 
       }
       if(ntc_numbers>0){
        temperature = temperature/ntc_numbers;
       }
       //TODO Protection & Cell Voltages
    }
    else if(_incommingByte[1] == 4){
      int cell_begin = 4;
      int * cellVoltagesTemp = new int  [cell_count];
      int j = 0;
      for( int i = 0; i<cell_count; i++){
        cellVoltagesTemp[i] = ((int)(_incommingByte[cell_begin+j] << 8 | _incommingByte[cell_begin +1+j]));
        j+=2;
        Serial.println(cellVoltagesTemp[i]);
      }
      
      //Serial.println("--------------------");
      cellVoltages = cellVoltagesTemp;
      delete[] cellVoltagesTemp;
      
      for(int i = 0; i>cell_count; i++){
        //Serial.println(cellVoltages[i]);
      }
    }
  }
  //delay(1000);
  //checkBMSVoltages();
  //checking_bms = checking_bms +1 % 3;
  vTaskDelete(checkBMSHandle); 
}

void sendCanMessages(void * pvParameters){
  //CNA_send_Network_alive_msg();
  Battery_actual_values_UIt();
  Battery_Manufacturer();
  Battery_Request();
  Battery_limits();
  Battery_SoC_SoH();
  Battery_Error_Warnings();
  vTaskDelete(sendCanMessagesHandle);
}

void drawDisplay(){
  display.clearBuffer();
  switch(settings_state){
    case normal_view:
      buildLowerMenu();
      switch (page) {
        case SoC:
        {
          //void fillArc2(int x, int y, int start_angle, int seg_count, int rx, int ry, int w)
          fillArc2(22,22,180,(int) charge*1.2,20,20,3);
          display.setFont(u8g2_font_tenthinguys_tr);
          display.setDrawColor(1);
          const char *percent_str = ((String) charge + (String) "%").c_str();
          int percent_width = display.getStrWidth(percent_str);
          display.drawStr(((22/2-1)-percent_width)/2+2,26,((String)charge+"%").c_str());
          switch(soc_menu){
            case page_view:
              display.setFont(u8g2_font_tenthinnerguys_tr);
              display.drawStr(50,15, ((String)("Chrg: "+ ((String) residual_capacity) + " Ah")).c_str());
              //display.drawStr(50,30, ((current<0) ? ((String)("Amps:" + ((String) ((int)(current/10))) + "." +  ((String) (current-((int)(current/10))*10)) + "A")).c_str() : ((String)("Amps: " + ((String) ((int)(current/10))) + "." +  ((String) (current-((int)(current/10))*10)) + "A")).c_str()));
              //display.drawStr(50,45, ((voltage<0) ? ((String)("Volts:" + ((String) ((int)(voltage/10))) + "." +  ((String) (voltage-((int)(voltage/10))*10)) + "V")).c_str() : ((String)("Volts: " + ((String) ((int)(voltage/10))) + "." +  ((String) (voltage-((int)(voltage/10))*10)) + "V")).c_str()));
              display.drawStr(50,30, ((current<0) ? ((String)("Amps:-" + ((String) abs((int)(current/10))) + "." +  ((String) abs(current-((int)(current/10))*10)) + "A")).c_str() : ((String)("Amps: " + ((String) ((int)(current/10))) + "." +  ((String) (current-((int)(current/10))*10)) + "A")).c_str()));
              display.drawStr(50,45, ((voltage<0) ? ((String)("Volts:-" + ((String) abs((int)(voltage/10))) + "." +  ((String) abs(voltage-((int)(voltage/10))*10)) + "V")).c_str() : ((String)("Volts: " + ((String) ((int)(voltage/10))) + "." +  ((String) (voltage-((int)(voltage/10))*10)) + "V")).c_str()));
              break;
            case charge_state:
              display.setFont(u8g2_font_tenthinnerguys_tr);
              display.drawStr(50,15, ((temperature<0)? (String)("Temp:-"+ ((String) abs((int)(temperature/10))) + "." +  ((String) abs(temperature-((int)(temperature/10))*10)) + " "+(String)(char)176 + "C") : (String)("Temp: "+ ((String) abs((int)(temperature/10))) + "." +  ((String) abs(temperature-((int)(temperature/10))*10)) + " "+(String)(char)176 + "C")).c_str());
              display.drawStr(50,30, ((charging)? (String)("Charging"+ (String)(char)47):(String)("Charging"+ (String)(char)42)).c_str());
              display.drawStr(50,45, ((discharging)? (String)("Discharging"+ (String)(char)47):(String)("Discharging"+ (String)(char)42)).c_str());
              
              break;
          }
          break;
          }
        case Battery_Voltages:
          display.setFont(u8g2_font_tenthinguys_tr);
          for( int i = 0; i<3; i++){
            display.drawStr(2,(i+1)*15,((String)("Cell "+ (String) (i+scroll+1) )).c_str());
            display.drawStr(50,(i+1)*15,((String)": "+((String)(int)(cellVoltages[i+scroll]/1000) + "." + ((String)(cellVoltages[i+scroll]-((int)(cellVoltages[i+scroll]/1000)))))).c_str());
            display.drawStr(110,(i+1)*15,"V");
          }
          switch (menus){
            case pages:
              display.setDrawColor(0);
              display.drawBox(120, 2, 8, 48);
              display.setDrawColor(1);
              break;
            case voltages:
              display.drawBox(120, 2, 8, 48);
              break;
          }
          break;
      }
      break;
    case settings:
      buildSettingsMenu();
      display.setDrawColor(1);
      display.setFont(u8g2_font_tenthinnerguys_tr);
        display.setDrawColor(1);
        // setting_selected
        // setting_selected = true;
        switch(options){
          case screen_off_delay:
            display.setDrawColor(!setting_selected);
            display.drawBox(0,0*15+3,40,15);
            display.setDrawColor(1);
            display.drawStr(0,2*15, "ID:");
            display.drawStr(0,3*15, "Can:");
            display.drawStr(60,3*15, ((can_enabled)?((String)(char)47):(String)((String)(char)42) ).c_str());
            display.drawStr(60,2*15, ((deviceID==0)?"main":(String)deviceID).c_str());
            display.setDrawColor(setting_selected);
            display.drawStr(0,1*15, "Delay:");
            display.drawBox(50,0*15+3,77,15);
            display.setDrawColor(!setting_selected);
            display.drawStr(60,1*15, ((String) ((int)display_turnoff_delay/1000)+"s").c_str());
            break;
          case device_id:
            display.setDrawColor(!setting_selected);
            display.drawBox(0,1*15+3,40,15);
            display.setDrawColor(1);
            display.drawStr(0,1*15, "Delay:");
            display.drawStr(60,1*15, ((String) ((int)display_turnoff_delay/1000)+"s").c_str());
            display.drawStr(0,3*15, "Can:");
            display.drawStr(60,3*15, ((can_enabled)?((String)(char)47):(String)((String)(char)42) ).c_str());
            display.setDrawColor(setting_selected);
            display.drawStr(0,2*15, "ID:");
            display.drawBox(50,1*15+3,77,15);
            display.setDrawColor(!setting_selected);
            display.drawStr(60,2*15, ((deviceID==0)?"main":(String)deviceID).c_str());
            break;
          case can:
            display.setDrawColor(!setting_selected);
            display.drawBox(0,1*15+3,40,15);
            display.setDrawColor(1);
            display.drawStr(0,1*15, "ID:");
            display.drawStr(60,1*15, ((deviceID==0)?"main":(String)deviceID).c_str());
            display.drawStr(0,3*15, "Victron:");
            display.drawStr(60,3*15, ((victron_bms_connected)?((String)(char)47):(String)((String)(char)42) ).c_str());
            display.setDrawColor(setting_selected);
            display.drawStr(0,2*15, ("Can:"));
            display.drawBox(50,1*15+3,77,15);
            display.setDrawColor(!setting_selected);
            display.drawStr(60,2*15, ((can_enabled)?((String)(char)47):(String)((String)(char)42) ).c_str());
            break;
          case victron_bms:
            display.setDrawColor(!setting_selected);
            display.drawBox(0,1*15+3,40,15);
            display.setDrawColor(1);
            display.drawStr(0,1*15, "Can:");
            display.drawStr(60,1*15, ((deviceID==0)?"main":(String)deviceID).c_str());
            display.drawStr(0,3*15, "Back");
            display.setDrawColor(setting_selected);
            display.drawStr(0,2*15, ("Victron:"));
            display.drawBox(50,1*15+3,77,15);
            display.setDrawColor(!setting_selected);
            display.drawStr(60,2*15, ((victron_bms_connected)?((String)(char)47):(String)((String)(char)42) ).c_str());
            break;
          case back:
            display.setDrawColor(!setting_selected);
            display.drawBox(0,2*15+3,40,15);
            display.setDrawColor(1);
            display.drawStr(0,1*15, "Can:");
            display.drawStr(60,1*15, ((deviceID==0)?"main":(String)deviceID).c_str());
            display.drawStr(0,2*15, "Victron:");
            display.drawStr(60,2*15, ((victron_bms_connected)?((String)(char)47):(String)((String)(char)42)).c_str());
            display.setDrawColor(setting_selected);
            display.drawStr(0,3*15, "Back");
            break;
        }
      
      break;
  }
  
  display.sendBuffer();
}

void buildLowerMenu(){
  display.setDrawColor(1);
  display.setFont(u8g2_font_t0_11_tf);
  display.drawBox(0,display_height - 14,display_width,14);
  int str_width = 0;
  switch (page) {
    case Battery_Voltages:
      display.setDrawColor(0);
      display.drawBox(1,display_height - 13,(display_width-2)/2,12);
      str_width = display.getStrWidth("Voltages");
      display.drawStr(display_width/2 + ((display_width/2 -1) - str_width)/2, display_height -3, "Voltages");
      display.setDrawColor(1);
      str_width = display.getStrWidth("SoC");
      display.drawStr(((display_width/2 -1) - str_width)/2, display_height -3, "SoC");
      break;
    case SoC:
      display.setDrawColor(0);
      display.drawBox((display_width+2)/2,display_height - 13,(display_width-2)/2-1,12);
      str_width = display.getStrWidth("SoC");
      display.drawStr(((display_width/2 -1) - str_width)/2, display_height -3, "SoC");
      display.setDrawColor(1);
      str_width = display.getStrWidth("Voltages");
      display.drawStr(display_width/2 + ((display_width/2 -1) - str_width)/2, display_height -3, "Voltages");
      break; 
  }
  
}

void buildSettingsMenu(){
  display.setDrawColor(1);
  display.setFont(u8g2_font_t0_11_tf);
  display.drawBox(0,display_height - 14,display_width,14);
  int str_width = 0;
  
  display.setDrawColor(1);
  display.drawBox(1,display_height - 13,display_width-2,12);
  display.setDrawColor(0);
  str_width = display.getStrWidth("Settings");
  display.drawStr((((display_width -1) - str_width)/2), display_height -3, "Settings");
    
}

// rotate is called anytime the rotary inputs change state.
void rotate() {
  display_turnoff = display_turnoff_delay;
  rs485Flush();
  unsigned char result = rotary.process();
  if (result == DIR_CW) {
    switch (settings_state) {
      case normal_view:
        switch (menus) {
          case pages:
            page ++;
            break;
          case voltages:
            scroll = (scroll<cell_count -3 ? scroll +1 : scroll);
            break;
        
        }
        break;
      case settings: {
        if(setting_selected){
          switch(options){
            case screen_off_delay:
              display_turnoff_delay = (display_turnoff_delay +1000)%3600000;
              break;
            case device_id:
              deviceID = (deviceID +1) % 32;
              break;
          }
        } else {
          options ++;
        }
      }
    }
    
    //counter++;
    //Serial.println(counter);
  } else if (result == DIR_CCW) {
    switch(settings_state){
      case normal_view:
        switch (menus) {
          case pages:
            page --;
            break;
          case voltages:
            scroll = (scroll>0 ? scroll -1 : scroll);
            break;
        }
      break;
      case settings: {
        if(setting_selected){
          switch(options){
            case screen_off_delay:
              display_turnoff_delay = (display_turnoff_delay < 2000) ? 1000 : display_turnoff_delay -1000;
              break;
            case device_id:
              deviceID --;
              if(deviceID<0){
                deviceID=31;
              }
              break;
            
          }
        } else {
          options --; 
        }
      }
    }
    
    //counter--;
    //Serial.println(counter);
  }
}

TaskHandle_t saveSettings;
void changeMenu(){
  
  display_turnoff = display_turnoff_delay;
  rs485Flush();
  if(button_active){
    checkPress();
    button_start = millis();
    if(display_active){
      switch(settings_state){
        case normal_view:
          switch(page){
            case SoC:
              soc_menu++;
              break;
            case Battery_Voltages:
              menus++;
              break;
          }
          break;
        case settings:
          switch(options){
            case screen_off_delay:
            case device_id:
              setting_selected = !setting_selected;
              break;
            case back:
//              save_settings();
              
              xTaskCreatePinnedToCore(
                save_settings, /* Function to implement the task */
                "save_settings", /* Name of the task */
                10000,  /* Stack size in words */
                NULL,  /* Task input parameter */
                0,  /* Priority of the task */
                &saveSettings,  /* Task handle. */
                0); /* Core where the task should run */
              break;
            case can:
              can_enabled = !can_enabled;
              break;
            case victron_bms:
              victron_bms_connected = !victron_bms_connected;
              break;
          }
          break;
      }
       
    }
    button_delay += 200;
    button_active = !button_active;
  }
  
}

void rs485Flush(){
  while(rs485.available() > 0) {
    char t = rs485.read();
  }
}


IRAM_ATTR void save_settings(void * pvParameters){
  EEPROM.writeInt(deviceID_EEPROM_adr,deviceID);
  EEPROM.commit();
  EEPROM.writeInt(display_turnoff_delay_EEPROM_adr,display_turnoff_delay);
  EEPROM.commit();
  EEPROM.writeBool(can_enabled_EEPROM_adr,can_enabled);
  EEPROM.commit();
  EEPROM.writeBool(victron_bms_connected_EEPROM_adr,victron_bms_connected);
  EEPROM.commit();
  settings_state = normal_view;
  vTaskDelete(saveSettings);
}

void checkPress() {
  if((millis() - button_start < button_long_press_delay)){
     Serial.println("Double press detected");
     settings_state = settings;
     options = screen_off_delay;
     setting_selected = true;
  }
}

// #########################################################################
// Draw a circular or elliptical arc with a defined thickness
// #########################################################################

// x,y == coords of centre of arc
// start_angle = 0 - 359
// seg_count = number of 3 degree segments to draw (120 => 360 degree arc)
// rx = x axis radius
// ry = y axis radius
// w  = width (thickness) of arc in pixels
// Note if rx and ry are the same then an arc of a circle is drawn
float DEG2RAD = PI/180;

void fillArc2(int x, int y, int start_angle, int seg_count, int rx, int ry, int w)
{

  byte seg = 3; // Segments are 3 degrees wide = 120 segments for 360 degrees
  byte inc = 3; // Draw segments every 3 degrees, increase to 6 for segmented ring

    // Calculate first pair of coordinates for segment start
    float sx = cos((start_angle - 90) * DEG2RAD);
    float sy = sin((start_angle - 90) * DEG2RAD);
    uint16_t x0 = sx * (rx - w) + x;
    uint16_t y0 = sy * (ry - w) + y;
    uint16_t x1 = sx * rx + x;
    uint16_t y1 = sy * ry + y;

  // Draw colour blocks every inc degrees
  for (int i = start_angle; i < start_angle + seg * seg_count; i += inc) {

    // Calculate pair of coordinates for segment end
    float sx2 = cos((i + seg - 90) * DEG2RAD);
    float sy2 = sin((i + seg - 90) * DEG2RAD);
    int x2 = sx2 * (rx - w) + x;
    int y2 = sy2 * (ry - w) + y;
    int x3 = sx2 * rx + x;
    int y3 = sy2 * ry + y;

    display.drawTriangle(x0, y0, x1, y1, x2, y2);
    display.drawTriangle(x1, y1, x2, y2, x3, y3);
    //Serial.println("Segment "+ (String) i +" drawn");
    // Copy segment end to sgement start for next segment
    x0 = x2;
    y0 = y2;
    x1 = x3;
    y1 = y3;
  }
}

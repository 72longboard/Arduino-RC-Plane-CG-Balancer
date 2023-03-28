#include <EEPROM.h>
#include <U8g2lib.h>
#include <TimerOne.h>
#include <Rotary.h>

bool statusLedOn = false;

// Timer
#define TIMER 1000

// Display and menu stuff
int displayCurrentPage = 0;
bool setNeedsDisplay = false;

// Main menu fixed to 3 items, left, center, right...
#define MENU_SELECTED_TIMEOUT 4000
#define MENU_POS_Y 62
#define MENU_POS_Y_HIDDEN 76
#define MENU_ANIMATION_PIXEL_STEP 2
String menuItems[3] = {"WHEEL", "CG", "SETUP"};
int menuActive = 1;             // left active
int menuSelected = menuActive;  // selected
bool menuPageMode = false;      // true => rotary encoder control page and not menu

// Menu animation
bool menuAnimationRunning = false;
int menuPosY = MENU_POS_Y;

// Rotary Encoder
#define ROTARY_SWITCH 12  // A1
Rotary rotary = Rotary(9, 7);

#define ROTARY_ACCEL_OFFSET1 50 //20
#define ROTARY_ACCEL_OFFSET2 150 //50
#define ROTARY_ACCEL_OFFSET3 300 //70
unsigned long rotaryLastMove;
bool rotaryButtonPressed = false;

// Logic
long int heartbeat = 0;
#define HEARTBEAT_TRIGGER 1000
#define HEARTBEAT_TRIGGER_TIME 50

// ========================================================
// PAGES STUFF
// ========================================================

#define SETUP_MENU_ITEMS 3
String setupMenuItems[SETUP_MENU_ITEMS] = {"EXIT", "LDR LEVEL", "VERSION & INFO"};
int setupMenuSelected = 0;

// Display
U8G2_SH1106_128X64_VCOMH0_1_4W_HW_SPI u8g2(U8G2_R0, 10, 8);


//load cell  ----------------------------------------------------------------------------
#include <HX711_ADC.h>

HX711_ADC LoadCell_1(1, 2); //HX711 1 Pin map
HX711_ADC LoadCell_2(3, 4); //HX711 2 Pin map
HX711_ADC LoadCell_3(5, 6); //HX711 3 Pin map

int sliderPosX = EEPROM.read(1) << 8 | EEPROM.read(0); //Distance between centre point of main wheels and tip (nose or tail) wheel
int sliderPosY = EEPROM.read(3) << 8 | EEPROM.read(2); //Distance of specified CG location from main wheels

unsigned long t = 0;
long Weight_Left  = 0;
long Weight_Right = 0;
long Weight_Tael = 0;
long Weight_Total = 0;
long CG_A =0;
int CG_D = 0; //Difference between actual and specified CG: CG(a) - CG(s)
int CG_D_Line = 0;



void setup() {
  //Load cell Start
  LoadCell_1.begin();
  LoadCell_2.begin();
  LoadCell_3.begin();

  unsigned long stabilizingtime = 1000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  byte loadcell_3_rdy = 0;
  
  while ((loadcell_1_rdy + loadcell_2_rdy + loadcell_3_rdy) < 3) { //run startup, stabilization and tare, both modules simultaniously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilizingtime, _tare);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilizingtime, _tare);
    if (!loadcell_3_rdy) loadcell_3_rdy = LoadCell_3.startMultiple(stabilizingtime, _tare);
  }
 
  LoadCell_1.setCalFactor(218.64); // user set calibration value (float)
  LoadCell_2.setCalFactor(215.65); // user set calibration value (float)
  LoadCell_3.setCalFactor(218.36); // user set calibration value (float)
  
//Load cell End

  // Menu Button
  pinMode(ROTARY_SWITCH, INPUT);
  digitalWrite(ROTARY_SWITCH, INPUT_PULLUP);

#ifdef BUTTON1_PIN
  pinMode(BUTTON1_PIN, INPUT);
  digitalWrite(BUTTON1_PIN, INPUT_PULLUP);
#endif

  // OLED Display
  u8g2.begin();
  setNeedsDisplay = true;
  Timer1.initialize(TIMER);
  Timer1.attachInterrupt(timerEvent);

}

void loop() {
  load_cell();
  
  //digitalWrite(STATUS_LED_PIN, statusLedOn);

  if (menuAnimationRunning) {
    if (menuPageMode && menuPosY < MENU_POS_Y_HIDDEN) {
      // do animation
      menuPosY = menuPosY + MENU_ANIMATION_PIXEL_STEP;
      setNeedsDisplay = true;
    }
    if (!menuPageMode && menuPosY > MENU_POS_Y) {
      // do animation
      menuPosY = menuPosY - MENU_ANIMATION_PIXEL_STEP;
      setNeedsDisplay = true;
    }
  }
  if (menuAnimationRunning && (menuPosY == MENU_POS_Y || menuPosY == MENU_POS_Y_HIDDEN)) {
    // looks like animation is done
    menuAnimationRunning = false;
  }

  if (setNeedsDisplay) {
    //noInterrupts();
    displayRenderCurrentPage();
    setNeedsDisplay = false;
    //interrupts();
  }

}

// ========================================================
// TIMER
// ========================================================

void timerEvent() {
  // Heartbeat
  if (heartbeat > HEARTBEAT_TRIGGER) {
    statusLedOn = true;
  }
  if (heartbeat > HEARTBEAT_TRIGGER + HEARTBEAT_TRIGGER_TIME) {
    statusLedOn = false;
    heartbeat = 0;
  }
  heartbeat++;

  // Menu logic
  unsigned long timeOffset = millis() - rotaryLastMove;
  if (timeOffset > MENU_SELECTED_TIMEOUT) {
    // deselect menu
    menuSelected = menuActive;
    rotaryLastMove = millis();
    setNeedsDisplay = true;
  }
  
  // Rotary Encoder
  unsigned char result = rotary.process();
  if (result) {

    if (!menuPageMode) {
      if (result == DIR_CW) {
        // right
        if (menuSelected < 3) {
          menuSelected++;
        }
      } else {
        // left
        if (menuSelected > 1) {
          menuSelected--;
        }
      }
      setNeedsDisplay = true;
    } else {

      // Acceleration
      byte acceleration = 1;
      unsigned long timeOffset = millis() - rotaryLastMove;
      
      //Serial.println(timeOffset);

      if (displayCurrentPage == 0 || displayCurrentPage == 1) {
        if (timeOffset < ROTARY_ACCEL_OFFSET1) {
          acceleration = 16;
        } else if (timeOffset < ROTARY_ACCEL_OFFSET2) {
          acceleration = 4;
        } else if (timeOffset < ROTARY_ACCEL_OFFSET3) {
          acceleration = 2;
        }
      
        // Development test => control slider
        if (result == DIR_CW) {
          // right
          if (sliderPosX < 3128) {
            sliderPosX = sliderPosX + acceleration;
          }

          if (sliderPosY < 3128) {
            sliderPosY = sliderPosY + acceleration;
          }
        } else {
          // left
          if (sliderPosX > 0) {
            sliderPosX = sliderPosX - acceleration;
          }

          if (sliderPosY > 0) {
            sliderPosY = sliderPosY - acceleration;
          }
        }
        setNeedsDisplay = true;
      }

      if (displayCurrentPage == 2) {

        if (result == DIR_CW) {
          // right
          setupMenuSelected++;
        } else {
          // left
          setupMenuSelected--;
        }
        if (setupMenuSelected > SETUP_MENU_ITEMS - 1) {
          setupMenuSelected = SETUP_MENU_ITEMS - 1;
        }
        if (setupMenuSelected < 1) {
          setupMenuSelected = 0;
        }
        setNeedsDisplay = true;
      }

    }

    rotaryLastMove = millis();
    
  }

  // Rotary button
  if (buttonEvent()) {
    rotaryLastMove = millis();
    if (menuActive == menuSelected) {
      if (!menuPageMode) {
        // give controls to page (button press on selected page)
        menuPageMode = true;
        menuAnimationRunning = true;
        sliderPosX =  EEPROM.read(1) << 8 | EEPROM.read(0);
        sliderPosY =  EEPROM.read(3) << 8 | EEPROM.read(2);
        //sliderPosX = 64;
        
        setNeedsDisplay = true;
        
        
      } else {
        menuPageMode = false;   
        menuAnimationRunning = true;
        setNeedsDisplay = true;
        sliderPosX =  EEPROM.read(1) << 8 | EEPROM.read(0);
        sliderPosY =  EEPROM.read(3) << 8 | EEPROM.read(2);
        //sliderPosX = 64;

        setupMenuSelected = 0;
        
        
      }
    }
    if (!menuPageMode) {
      menuActive = menuSelected;
      if (menuActive == 1) {
        displayCurrentPage = 0;
        u8g2.print("VALUE ");
      }
      if (menuActive == 2) {
        displayCurrentPage = 1;
      }
      if (menuActive == 3) {
        displayCurrentPage = 2;
      }
      setNeedsDisplay = true;
    }
  }

  // Action button => reset page mode during development
#ifdef BUTTON1_PIN
  if (digitalRead(BUTTON1_PIN) == 0 && menuPageMode) {
    menuPageMode = false;   
    menuAnimationRunning = true;
    setNeedsDisplay = true;
    sliderPosX =  EEPROM.read(1) << 8 | EEPROM.read(0);
    sliderPosY =  EEPROM.read(3) << 8 | EEPROM.read(2);
    //sliderPosX = 64;
   
  }
#endif
  
}

bool buttonEvent() {
  bool result = false;
  bool menuButton = false;
  if (digitalRead(ROTARY_SWITCH) == 1) {
    menuButton = true;
  }
  if (menuButton && !rotaryButtonPressed) {
    rotaryButtonPressed = true;
  } else if (!menuButton && rotaryButtonPressed) {
    rotaryButtonPressed = false;
    result = true;
  }
  return result;
}

void displayRenderCurrentPage() {
  // Main Title OLED Display update
  u8g2.firstPage();
  do {

    if (displayCurrentPage == 0) {
      u8g2.setFont(u8g2_font_8x13B_tr);
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(0, 12, "DISTANCE D"); 
      u8g2.setCursor(54, 12);
      u8g2.print(sliderPosX);
    }

    if (displayCurrentPage == 1) {
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.drawStr(0, 12, "DISTANCE CG "); 
      u8g2.setFont(u8g2_font_5x7_tr);
      u8g2.setCursor(56, 12);
      u8g2.print(sliderPosY);
    }

    if (displayCurrentPage == 2) {
      if (!menuPageMode) {

        u8g2.setFont(u8g2_font_5x7_tr);
        u8g2.drawStr(0, 10, "TOTAL :"); u8g2.setCursor(36, 10); u8g2.print(Weight_Left + Weight_Right + Weight_Tael);  //Total weight of airplane: W(p) + W(s) + W(t)
       
        if ((Weight_Tael * sliderPosX) / (Weight_Left + Weight_Right + Weight_Tael) - sliderPosY < 0) {
          u8g2.setCursor(60, 10); u8g2.print("NOW NH");
          }
        else if ((Weight_Tael * sliderPosX) / (Weight_Left + Weight_Right + Weight_Tael) - sliderPosY > 0) {
          u8g2.setCursor(60, 10); u8g2.print("NOW TH");
          } 
        else {
          u8g2.setCursor(60, 10); u8g2.print("BALANCED!");
        }

        u8g2.drawStr(0, 20, "W(L) :"); u8g2.setCursor(34, 20); u8g2.print(Weight_Left);  u8g2.drawStr(60, 20, "G"); 
        u8g2.drawStr(0, 28, "W(R) :"); u8g2.setCursor(34, 28); u8g2.print(Weight_Right); u8g2.drawStr(60, 28, "G"); 
        u8g2.drawStr(0, 36, "W(T) :"); u8g2.setCursor(34, 36); u8g2.print(Weight_Tael);  u8g2.drawStr(60, 36, "G"); 
        u8g2.drawStr(74, 28, "CG A "); u8g2.setCursor(96, 28); if ((Weight_Left + Weight_Right + Weight_Tael) > 100){u8g2.print((Weight_Tael * sliderPosX) / (Weight_Left + Weight_Right + Weight_Tael));}      //Actual CG location behind main wheels: W(t) x D / W(tot)  
        u8g2.drawStr(74, 20, "NEED "); u8g2.setCursor(96, 20); u8g2.print((Weight_Left + Weight_Right + Weight_Tael) * sliderPosY / sliderPosX); //Weight required at tip wheel for balanced CG: W(tot) x CG(a) / D
        u8g2.drawStr(74, 36, "CG D");  u8g2.setCursor(96, 36); if ((Weight_Left + Weight_Right + Weight_Tael) > 100){u8g2.print(((Weight_Tael * sliderPosX) / (Weight_Left + Weight_Right + Weight_Tael)) - sliderPosY);} //Difference between actual and specified CG: CG(a) - CG(s)
        drawSliderCG(40);
      } else {
        // Sub  메뉴
        drawPageMenu();

        if (setupMenuSelected == 0) {
          u8g2.setFont(u8g2_font_5x7_tr);
          u8g2.drawStr(0, 28, "DEMO MODE");
          u8g2.drawStr(0, 38, "PRESS BUTTON TO EXIT");
        }

        if (setupMenuSelected == 3) {
                              
        }
      }
    }

    if (displayCurrentPage == 0) {
      u8g2.setFont(u8g2_font_5x7_tr);
      if (menuPageMode) { 
        u8g2.drawStr(0, 28, "ROTARY CONTROL ON PAGE");
        u8g2.setCursor(0, 46);
        u8g2.print("VALUE ");
        u8g2.print(sliderPosX);
        EEPROM.update(0, sliderPosX & 0xff);
        EEPROM.update(1, sliderPosX >> 8);        
      } else {
        u8g2.drawStr(0, 28, "DISTANCE WHEEL TO WHEEL");

      }
      
      drawSlider(31);
    }

 if (displayCurrentPage == 1) {
      u8g2.setFont(u8g2_font_5x7_tr);
      if (menuPageMode) { 
        u8g2.drawStr(0, 28, "ROTARY CONTROL ON PAGE");
        u8g2.setCursor(0, 46);
        u8g2.print("VALUE ");
        u8g2.print(sliderPosY);
        EEPROM.update(2, sliderPosY & 0xff);
        EEPROM.update(3, sliderPosY >> 8);
      } else {
        u8g2.drawStr(0, 28, "DISTANCE WHEEL TO CG");
      }

      drawSliderY(31);
    }

    drawMenuBar();
    
  } while ( u8g2.nextPage() );
}

void drawPageMenu() {
  u8g2.setFont(u8g2_font_6x12_tr);
  if (displayCurrentPage == 2) {
    String text = setupMenuItems[setupMenuSelected];
    // center text
    int textWidth = u8g2.getStrWidth(text.c_str());
    int textX = (128 - textWidth) / 2;
    int textXPadding = 4;
    u8g2.drawRBox(textX - textXPadding, 0, textWidth + textXPadding + textXPadding, 11, 2);
    u8g2.setDrawColor(0);
    u8g2.setCursor(textX, 11 - 2);
    u8g2.print(text);
    u8g2.setDrawColor(1);

    bool drawLeftTriangle = false;
    bool drawRightTriangle = false;

    if (setupMenuSelected < SETUP_MENU_ITEMS - 1) {
      drawRightTriangle = true;
    }
    if (setupMenuSelected > 0) {
      drawLeftTriangle = true;
    }

    if (drawLeftTriangle) {
      // Triangle left
      u8g2.drawTriangle(4, 1, 4, 9, 0, 5);
    }
    if (drawRightTriangle) {
      // Triangle right
      u8g2.drawTriangle(128 - 5, 1, 128 - 5, 9, 127, 5);
    }
    u8g2.drawHLine(0, 14, 128);
  }
  
}

//--------------
//Number 1. Draw a square on the menu
//--------------
void drawSlider(int yPos) {
  u8g2.drawFrame(0, yPos, 128, 6);
  if (sliderPosX < 1) {
    sliderPosX = 0;
  }
  //if (sliderPosX > 128) {
    //sliderPosX = 128;
  //}
  u8g2.drawVLine(sliderPosX, yPos, 6);
}

//--------------
//Number 2. Draw a square on the menu
//--------------
void drawSliderY(int yPos) {
  u8g2.drawFrame(0, yPos, 128, 6);
  if (sliderPosY < 1) {
    sliderPosY = 0;
  }
  //if (sliderPosX > 128) {
    //sliderPosX = 128;
  //}
  u8g2.drawVLine(sliderPosY, yPos, 6);
}

//--------------
//Number 3. Draw a square on the menu
//--------------
void drawSliderCG(int yPos) {
  u8g2.drawFrame(0, yPos, 128, 6);
  if (CG_D < 1) {
    CG_D_Line = 0;
  }
  if (CG_D > 128) {
    CG_D_Line = 128;
  }
  if (CG_D < 126 ) {
    
  }
  u8g2.drawVLine(64, yPos, 6);
  u8g2.drawVLine(CG_D_Line, yPos, 6);

}

void drawMenuBar() {
  int textX = 0;
  int textY = menuPosY;
  int textWidth = 0;
  int textXPadding = 4;
  
  u8g2.setFont(u8g2_font_6x12_tr);
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, textY - 11 - 2, 128);

  if (textY < MENU_POS_Y_HIDDEN) {
    // center menu
    String text = menuItems[1];
    textWidth = u8g2.getStrWidth(text.c_str());
    textX = (128 - textWidth) / 2;
    if (menuActive == 2) {
      u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(0);
    } 
    if (menuActive != menuSelected && menuSelected == 2) {
      u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(1);
    }
  
    u8g2.setCursor(textX, textY);
    u8g2.print(text);
    u8g2.setDrawColor(1);
  
    // left menu
    text = menuItems[0];
    textX = textXPadding;
    textWidth = u8g2.getStrWidth(text.c_str());
    if (menuActive == 1) {
      u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(0);
    } 
    if (menuActive != menuSelected && menuSelected == 1) {
      u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(1);
    }
    u8g2.setCursor(textX, textY);
    u8g2.print(text);
    u8g2.setDrawColor(1);
  
    // right menu
    text = menuItems[2];
    textWidth = u8g2.getStrWidth(text.c_str());
    textX = 128 - textWidth - textXPadding;
    if (menuActive == 3) {
      u8g2.drawRBox(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(0);
    }
    if (menuActive != menuSelected && menuSelected == 3) {
      u8g2.drawRFrame(textX - textXPadding, textY + 2 - 11, textWidth + textXPadding + textXPadding, 11, 2);
      u8g2.setDrawColor(1);
    }
    u8g2.setCursor(textX, textY);
    u8g2.print(text);
    u8g2.setDrawColor(1);
  }
  
}

void load_cell() {
static boolean newDataReady = 0;
  const int serialPrintInterval = 0; 
    if (LoadCell_1.update()) newDataReady = true;
        LoadCell_2.update();
        LoadCell_3.update();

   if ((newDataReady)) {
    if (millis() > t + serialPrintInterval) {
       Weight_Left = LoadCell_1.getData();
       Weight_Right = LoadCell_2.getData();
       Weight_Tael = LoadCell_3.getData();
   
      newDataReady = 0;
      t = millis();
    }
  }

  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell_1.tareNoDelay();
      LoadCell_2.tareNoDelay();
      LoadCell_3.tareNoDelay();
    }
  }

}

#include<EEPROM.h>

#include "esp_pm.h"
#include "CompositeGraphics.h"
#include "Image.h"
#include "CompositeColorOutput.h"

#include "headlessLogo.h"

#include "collectProduct.h"
#include "dispensing.h"
#include "noteAccepted.h"
#include "pleaseRefill.h"
#include "pushButton.h"
#include "syncComplete.h"
#include "welcome.h"
#include "insertTenTaka.h"
#include "timeOut.h"

#define motorON 1       //toggle the values for relay
#define motorOFF 0

#define totalColumn 1
#define mProduct_in_Column 20  //maximum product in a column

#define eeprAddr1 100  //EEPROM Address where column number will store
#define eeprAddr2 110
#define eeprAddr3 120
#define eeprSize 128  //EEPROM Size in Byte

#define limit1 21     //Defining Limit Switch input pin for motor rotation
#define limit2 19
#define limit3 18
#define limit4 13

#define motor1 23     //Defining Motor output pin
#define motor2 22
#define motor3 4
#define motor4 2
#define motor5 32
#define motor6 33
#define motor7 26
#define motor8 27

#define pushbutton 39     //Defining pin for PushButton, ResetButton & Buzzer 39,36
#define resetButton 36
//#define buzzer 5

#define noteAcceptorTX 14   //Cash Acceptor Rx & Tx
#define noteAcceptorRX 35

CompositeGraphics graphics(CompositeColorOutput::XRES, CompositeColorOutput::YRES);
CompositeColorOutput composite(CompositeColorOutput::NTSC);

Image<CompositeGraphics> logo_image(headlessLogo::xres, headlessLogo::yres, headlessLogo::pixels);

Image<CompositeGraphics> collectProduct_t(collectProduct::xres, collectProduct::yres, collectProduct::pixels);
Image<CompositeGraphics> dispensing_t(dispensing::xres, dispensing::yres, dispensing::pixels);
Image<CompositeGraphics> noteAccepted_t(noteAccepted::xres, noteAccepted::yres, noteAccepted::pixels);
Image<CompositeGraphics> pleaseRefill_t(pleaseRefill::xres, pleaseRefill::yres, pleaseRefill::pixels);
Image<CompositeGraphics> pushButton_t(pushButton::xres, pushButton::yres, pushButton::pixels);
//Image<CompositeGraphics> pushButton_t(pushButton::xres, pushButton::yres, pushButton::pixels);
Image<CompositeGraphics> syncComplete_t(syncComplete::xres, syncComplete::yres, syncComplete::pixels);
Image<CompositeGraphics> welcome_t(welcome::xres, welcome::yres, welcome::pixels);
Image<CompositeGraphics> insertTenTaka_t(insertTenTaka::xres, insertTenTaka::yres, insertTenTaka::pixels);
Image<CompositeGraphics> timeOut_t(timeOut::xres, timeOut::yres, timeOut::pixels);

int column = 0;
int dispanseCount = 0;
int columnDispenseCount = 0;
int maxPressTime = 60000;  //Maximum Time for cash acceptor remain ON after push button pressed
int credit = 0;
int price = 30;     //Change the price as requirement

void showImage(Image<CompositeGraphics> text) {
  graphics.begin(0);  //clear screen black
  logo_image.draw(graphics, 60, 40);
  text.draw(graphics, 90, 100);
  //  icon.draw(graphics, 251, 104);
  graphics.end();
  composite.sendFrameHalfResolution(&graphics.frame);
}

void setup() {
  Serial.begin(115200);

  composite.init();
  graphics.init();
  showImage(welcome_t);
  EEPRomRead();        //Get the Column/Row number to be dispensed after a restart

  pinMode(noteAcceptorRX, INPUT_PULLUP);
  pinMode(noteAcceptorTX, OUTPUT);

  pinMode(limit1, INPUT_PULLUP);
  pinMode(limit2, INPUT_PULLUP);
  pinMode(limit3, INPUT_PULLUP);
  pinMode(limit4, INPUT_PULLUP);

  pinMode(pushbutton, INPUT_PULLUP);
  pinMode(resetButton, INPUT_PULLUP);

  pinMode(motor1, OUTPUT);
  pinMode(motor2, OUTPUT);
  pinMode(motor3, OUTPUT);
  pinMode(motor4, OUTPUT);
  pinMode(motor5, OUTPUT);
  pinMode(motor6, OUTPUT);
  pinMode(motor7, OUTPUT);
  pinMode(motor8, OUTPUT);

  digitalWrite(motor1, motorOFF);
  digitalWrite(motor2, motorOFF);
  digitalWrite(motor3, motorOFF);
  digitalWrite(motor4, motorOFF);
  digitalWrite(motor5, motorOFF);
  digitalWrite(motor6, motorOFF);
  digitalWrite(motor7, motorOFF);
  digitalWrite(motor8, motorOFF);

  digitalWrite(noteAcceptorTX, LOW);
  delay(10000);
  showImage(pushButton_t);
}


void EEPRomRead() {            //read function for EEPRom

  EEPROM.begin(eeprSize);
  delay(500);
  column = EEPROM.readInt(eeprAddr1);
  dispanseCount = EEPROM.readInt(eeprAddr2);
  columnDispenseCount = EEPROM.readInt(eeprAddr3);
  EEPROM.end();
  delay(500);
  Serial.println("Read from EEPRom");
  Serial.print("Column value = ");
  Serial.print(column);
  Serial.print(" & Dispense Count = ");
  Serial.print(dispanseCount);
  Serial.print(" & Column Dispense Count = ");
  Serial.println(columnDispenseCount);

  delay(500);
}

void EEPRomwrite(int col, int dCount, int cdCount) {            //write function to EEPRom

  EEPROM.begin(eeprSize);
  delay(500);
  EEPROM.writeInt(eeprAddr1, col);
  delay(100);
  EEPROM.writeInt(eeprAddr2, dCount);
  delay(100);
  EEPROM.writeInt(eeprAddr3, cdCount);
  delay(100);
  EEPROM.commit();
  EEPROM.end();
  Serial.print("Column value = ");
  Serial.print(col);
  Serial.print("Dispense count= ");
  Serial.print(dCount);
  Serial.print("Column Dispense count= ");
  Serial.print(cdCount);
  Serial.println(" Write to EEPRom");

}

void reset() {              //Reset the column value to "1" in EEPRom
  column = 1;
  dispanseCount = 0;
  columnDispenseCount = 0;
  delay(500);
  EEPRomwrite(column, dispanseCount, columnDispenseCount);
  showImage(syncComplete_t);
  Serial.println("Reset Done");
  delay(2000);
  EEPRomRead();
  showImage(pushButton_t);
}

void loop() {
  int pushState = digitalRead(pushbutton);
  int resetState = digitalRead(resetButton);
  if ( dispanseCount >= totalColumn * mProduct_in_Column) {
    showImage(pleaseRefill_t);
    Serial.println("product sesh.");
    while (true) {
      int resetState = digitalRead(resetButton);
      if (resetState == LOW) {
        //        Serial.println("dhuklam");
        int x = 0;
        while (digitalRead(resetButton) == LOW) {
          //          Serial.println("abar dhuklam");
          delay(100);
          x++;
          if (x > 30) {
            reset();
            x = 0;
            break;
          }
        }
      }
      if (dispanseCount < totalColumn * mProduct_in_Column) {
        break;
      }
    }
  }
  if (pushState == LOW) {
    int y = 0;
    while (digitalRead(pushbutton) == LOW)
    {
      y++;
      if (y >= 50) {
        showImage(insertTenTaka_t);
        Serial.println("Button Pressed");
        EEPRomRead();
        noteAccept();
        break;
      }
    }
  }
  if (resetState == LOW) {
    //    Serial.println("dhuklam");
    int x = 0;
    while (digitalRead(resetButton) == LOW) {
      //      Serial.println("abar dhuklam");
      delay(100);
      x++;
      if (x > 30) {
        reset();
        x = 0;
        break;
      }
    }
  }
  
}

void noteAccept() {                         //Note Accept & Dispense Processing function
  digitalWrite(noteAcceptorTX, HIGH);
  int startTime = millis();
  while (true) {
    int pwm_value = pulseIn(noteAcceptorRX, LOW);
    if (pwm_value != 0) {
      startTime = millis();
      credit = credit + 1;
      Serial.print(credit);
      Serial.print(",");
    }
    if (credit >= price - 5 && credit <= price + 5) { //taking offset 5 pulse
      digitalWrite(noteAcceptorTX, LOW);
      credit = 0;
      Serial.println(" ");
      Serial.print(credit);
      Serial.println("$ Credited! Acceptor Disabled");
      showImage(noteAccepted_t);

      delay(1000);
      showImage(dispensing_t);
      dispense(column);
      delay(500);
      showImage(collectProduct_t);
      delay(3000);
      showImage(pushButton_t); //welcome
      break;
    }
    if (millis() - startTime >= maxPressTime ) {
      Serial.println(" ");
      showImage(timeOut_t);
      delay(2000);
      showImage(pushButton_t); //welcome
      Serial.println("Timeout! Acceptor Disabled");
      digitalWrite(noteAcceptorTX, LOW);
      break;
    }
  }
}

void dispense(int col) {                    //Column/Row Selection for dispensing
  Serial.println("Dispencing...");
  Serial.println(col);
  switch (col) {
    case 1:
      runMotor_spring(motor1, limit1);
      break;
    case 2:
      runMotor_spring(motor2, limit2);
      break;
    case 3:
      runMotor_spring(motor3, limit3);
      break;
    case 4:
      runMotor_spring(motor4, limit3);
      break;
    case 5:
      runMotor_spring(motor5, limit4);
      break;
    case 6:
      runMotor_spring(motor6, limit4);
      break;
   case 7:
     runMotor_spring(motor7, limit4);
     break;
   case 8:
     runMotor_spring(motor8, limit4);
     break;
    default:
      break;

  }
  columnDispenseCount = columnDispenseCount + 1;
  dispanseCount = dispanseCount + 1;
  if (columnDispenseCount >= mProduct_in_Column) {
    column = column + 1;
    columnDispenseCount = 0;
  }

  if (column > totalColumn) {
    column = 1;
  }

  EEPRomwrite(column, dispanseCount, columnDispenseCount);     //Update column/row number to EEPRom
  Serial.println("Dispense Done!");
}

void runMotor_spring(int motorPin, int rotationPin) {    //for spring type dispense chember
  delay(20);
  digitalWrite(motorPin, motorON);
  delay(1000);
  Serial.print("motor ");
  Serial.print(column);
  Serial.println(" start");
  Serial.print("MotorPin: ");
  Serial.println(motorPin);
  Serial.print("RotationPin: ");
  Serial.println(rotationPin);


  int h = digitalRead(rotationPin);
  Serial.println(h);
  while (h == LOW)
  {
    delay(50);
    h = digitalRead(rotationPin);
    Serial.print(h);
  }
  if (h != LOW)
  {
    //    Serial.print(h);
    //    delay(50);
    digitalWrite(motorPin, motorOFF);
    Serial.print("motor ");
    Serial.print(column);
    Serial.println(" stop");
  }
}

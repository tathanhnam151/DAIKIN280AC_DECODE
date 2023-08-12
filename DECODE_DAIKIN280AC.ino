#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27, 16 column and 2 rows


volatile uint16_t inputCaptureData[300]; //To store received time periods
volatile uint16_t value[300]; //To store received time periods
volatile uint8_t isFirstTriggerOccured = 0; //First Trigger Flag
volatile uint8_t receiveCounter = 0;  //Receiver Counter
volatile uint8_t receiveComplete = 0; //Receive Complete Flag
volatile uint8_t partCounter = 0; //Receive Complete Flag
volatile uint8_t isLast = 0; //Receive Complete Flag
volatile uint8_t hexDecoder; //Receive Complete Flag
volatile uint8_t hexValue[70]; //Receive Complete Flag
const byte frameDetected = 9;
const byte swing = 10;
const byte powerful = 11;
const byte econo = 12;

void timerOneConfigForCapture();
uint32_t getCommand();

ISR (TIMER1_CAPT_vect) {  //Timer 01 has been configured to work with Input capture mode
  if (isFirstTriggerOccured) {  //Capturing will start after first falling edge detected by ICP1 Pin
    inputCaptureData[receiveCounter] = ICR1;  //Read the INPUT CAPTURE REGISTER VALUE
    if (inputCaptureData[receiveCounter] >  500) {  // if the value is greater than 625 (~2.5ms), then
      receiveCounter = 0; //reset "receiveCounter"
      receiveComplete = 0;  //reset "receiveComplete"
    } else {
      receiveCounter++;
      if (receiveCounter == 64 && partCounter != 2) { //if all the bits are detected,
        isLast = 0;
        partCounter++;
        receiveComplete = 1;  //then set the "receiveComplete" flag to "1"
      }
      if (receiveCounter == 152) {
        isLast = 1;
        partCounter = 0;
        receiveComplete = 1;  //then set the "receiveComplete" flag to "1"
      }
    }
  } else {
    isFirstTriggerOccured = 1;  //First falling edge occured! Start capturing from the second falling edge.
  }
  TCNT1 = 0;  //Reset Timer 01 counter after every capture
}

void setup() {
  pinMode (frameDetected, OUTPUT);
  pinMode (powerful, OUTPUT);
  pinMode (econo, OUTPUT);
  timerOneConfigForCapture(); //Configure Timer 01 to run in Input Capture Mode
  Serial.begin(115200); //Serial Interface for Debugging
  Serial.println("Decoder Starting!!");
  lcd.init();
  lcd.backlight();
  // Print a message to the LCD.
  lcd.print("OFF");
}

void loop() {
  getCommand();
}


void timerOneConfigForCapture() {
  DDRB &= ~(1 << DDB0); //Set digital pin 8 as a input
  PORTB |= (1 << PORTB0); //Internal Pull-up enabled
  cli();  //Stop all interrupte until timer 01 configs are done
  TCCR1A = 0x00;  //Set to 0
  TCCR1B &= ~(1 << ICES1);  //Falling edge trigger enabled
  TCCR1B |= (1 << CS11) | (1 << CS10);  //Prescaler to 64 -> will increment Timer01 every 4us
  TCCR1C = 0x00;  //Set to 0
  TIMSK1 |= (1 << ICIE1); //Enable input capture interrupt
  sei();  //Enable all global interrupts
}

/*
   the time period t is calculated by:
   ( inputCaptureData[<INDEX>] * 4us ) / 1000 -> will give the result in milliseconds
   Ex:
      t = (325 * 4) / 1000
      t = 1.3ms
*/
uint32_t getCommand() {
  if (receiveComplete) {  //If receive complete, start decoding process
    receiveComplete = 0;  //Set the receive complete to 0 for next data to be captured
    digitalWrite (frameDetected, HIGH);
    uint32_t receiveString = 0; //To store decoded value
    int index = 0;
    for (int i = 0; i < receiveCounter; i++) {  //Decode all 32 bits receive as time periods
      if ((i + 1) % 4 == 0) {
        int receiveStream = 0; //To store decoded value
        for (int j = i - 3; j < i + 1; j++ ) {
          value[j] = inputCaptureData[j];
          if (inputCaptureData[j] < 250 && inputCaptureData[j] > 200) {  //if the time period t* -> 1.0ms < t < 1.3ms  (0.8-1.0)   1=250
            receiveStream = (receiveStream << 1); //Only bit shift the current value
          } else if (inputCaptureData[j] < 500 && inputCaptureData[j] > 425) {  //if the time period t* -> 2.0ms < t < 2.5ms (1.7-2.0)
            receiveStream = (receiveStream << 1); //Only bit shift the current value
            receiveStream |= 0x0001;  //increment value by 1 using Logic OR operation
          }
        }
        hexDecoder = receiveStream;
        Serial.print(hexDecoder, HEX); //Print the value in serial monitor for debugging
        if (isLast) {
          hexValue[index] = hexDecoder;
          if (index == 10) {
            if (hexValue[index] == 1 || hexValue[index] == 3 || hexValue[index] == 5 || hexValue[index] == 7) {
              lcd.setCursor(0, 0); //collumn,line
              lcd.print("OFF");
              lcd.setCursor(10, 1);
              if (hexValue[index] == 3) {
                lcd.print("t:OFF ");
              } else if (hexValue[index] == 5) {
                lcd.print("t:ON  ");
              } else if (hexValue[index] == 7) {
                lcd.print("t:BOTH");
              } else lcd.print("      ");
            } else {
              lcd.setCursor(0, 0); //collumn,line
              lcd.print("ON ");
              lcd.setCursor(10, 1);
              if (hexValue[index] == 11) {
                lcd.print("t:OFF ");
              } else if (hexValue[index] == 13) {
                lcd.print("t:ON  ");
              } else if (hexValue[index] == 15) {
                lcd.print("t:BOTH");
              } else lcd.print("      ");
            }
          }
          if (index == 11) {
            getMode (hexValue[index]);
          }
          if (index == 13) {
            byte temp = getTemp (hexValue[index - 1], hexValue[index]);
            Serial.print(temp);
            if (temp == 0 || (hexValue[index - 2] == 6)) {
              lcd.setCursor(4, 0); //collumn,line
              // print the number of seconds since reset:
              lcd.print("    ");
            } else {
              Serial.print("oC");
              lcd.setCursor(4, 0); //collumn,line
              // print the number of seconds since reset:
              lcd.print(temp);
              lcd.print("oC");
            }
          }
          if (index == 16) {
            if (hexValue[index]) {
              digitalWrite (swing, HIGH);
            }
            else digitalWrite(swing, LOW);
          }
          if (index == 17) {
            lcd.setCursor(0, 1);
            lcd.print("F:");
            byte fanSpeed = getFanSpeed(hexValue[index]);
          }
          if (index == 26) {
            if (hexValue[index]) {
              digitalWrite (powerful, HIGH);
            }
            else digitalWrite(powerful, LOW);
          }
          if (index == 32) {
            if (hexValue[index]) {
              digitalWrite (econo, HIGH);
            }
            else digitalWrite(econo, LOW);
          }
          Serial.print(" ");
          index++;
        }

      }
    }
    Serial.println(""); //Print the value in serial monitor for debugging
    digitalWrite (frameDetected, LOW);

    return receiveString; //Return the received data stream
  }
  return 0; //default return value is 0
}


uint16_t getTemp(byte t1, byte t2) {
  byte temp = 0;
  if (t1 == 0 && t2 == 2) {
    temp = 32;
  }
  if (t1 == 7 && t2 == 12) {
    temp = 31;
  }
  if (t1 == 3 && t2 == 12) {
    temp = 30;
  }
  if (t1 == 5 && t2 == 12) {
    temp = 29;
  }
  if (t1 == 1 && t2 == 12) {
    temp = 28;
  }
  if (t1 == 6 && t2 == 12) {
    temp = 27;
  }
  if (t1 == 2 && t2 == 12) {
    temp = 26;
  }
  if (t1 == 4 && t2 == 12) {
    temp = 25;
  }
  if (t1 == 0 && t2 == 12) {
    temp = 24;
  }
  if (t1 == 7 && t2 == 4) {
    temp = 23;
  }
  if (t1 == 3 && t2 == 4) {
    temp = 22;
  }
  if (t1 == 5 && t2 == 4) {
    temp = 21;
  }
  if (t1 == 1 && t2 == 4) {
    temp = 20;
  }
  if (t1 == 6 && t2 == 4) {
    temp = 19;
  }
  if (t1 == 2 && t2 == 4) {
    temp = 18;
  }
  if (t1 == 4 && t2 == 4) {
    temp = 17;
  }
  if (t1 == 0 && t2 == 4) {
    temp = 16;
  }
  if (t1 == 7 && t2 == 8) {
    temp = 15;
  }
  if (t1 == 3 && t2 == 8) {
    temp = 14;
  }
  if (t1 == 5 && t2 == 8) {
    temp = 13;
  }
  if (t1 == 1 && t2 == 8) {
    temp = 12;
  }
  if (t1 == 6 && t2 == 8) {
    temp = 11;
  }
  if (t1 == 2 && t2 == 8) {
    temp = 10;
  }
  if (t1 == 0 && t2 == 3) {
    temp = 0;
  }
  return temp;
}

uint8_t getMode(byte m) {
  if (m == 0) {
    lcd.setCursor(9, 0);
    lcd.print("Auto ");
    Serial.print (m);
  }
  if (m == 4) {
    lcd.setCursor(9, 0);
    lcd.print("Humid");
    Serial.print (m);
  }
  if (m == 12) {
    lcd.setCursor(9, 0);
    lcd.print("Cool ");
    Serial.print (m);
  }
  if (m == 2) {
    lcd.setCursor(9, 0);
    lcd.print("Heat ");
    Serial.print (m);
  }
  if (m == 6) {
    lcd.setCursor(9, 0);
    lcd.print("Fan  ");
    Serial.print (m);
  }
  return 0;
}

uint8_t getFanSpeed(byte fanSpeed) {
  lcd.setCursor(2, 1);
  switch (fanSpeed) {
    case 5:
      lcd.print("Auto ");
      break;
    case 13:
      lcd.print("Night");
      break;
    case 12:
      lcd.print("1    ");
      break;
    case 2:
      lcd.print("2    ");
      break;
    case 10:
      lcd.print("3    ");
      break;
    case 6:
      lcd.print("4    ");
      break;
    case 14:
      lcd.print("5    ");
      break;
    default:
      lcd.print("?    ");
      break;
  }

}

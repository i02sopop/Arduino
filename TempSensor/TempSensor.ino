#include <LiquidCrystal.h>
#include <util/delay_basic.h>

// Initialize the LCD.
LiquidCrystal lcd(10, 11, 12, 13, 3, 4, 5, 6, 7, 8, 9);

int tempPin = 5;

void setup() {
    // set up the LCD's number of columns and rows: 
    lcd.begin(16, 2);
	// pinMode(A0, INPUT);
}

void loop() {
	int i; // Counter
	float tempc = 0.0; // temperature variables
	float maxT = -100.0, minT = 100.0; // to start max/min temperature

	for(i = 0; i < 8; i++) {
		// gets 8 samples of temperature
		tempc = tempc + (analogRead(tempPin) * 5.0  / 1024.0) * 100.0 - 273.15; //* 100.0 - 273.15 - 40.0;
		delay(1000);
	}

	tempc = tempc/8.0; // better precision
	if(tempc > maxT) { maxT = tempc; } // set max temperature
	if(tempc < minT) { minT = tempc; } // set min temperature

	char line1[16];
	sprintf(line1, "Temp ");

	// set the cursor to column 0, line 1
	lcd.setCursor(0, 0);
	lcd.print(tempc, 2);

	// set the cursor to column 0, line 2
	lcd.setCursor(0, 1);
}

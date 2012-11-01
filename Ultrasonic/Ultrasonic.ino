#include <LiquidCrystal.h>
#include <util/delay_basic.h>

// Initialize the LCD.
LiquidCrystal lcd(10, 11, 12, 13, 3, 4, 5, 6, 7, 8, 9);

int echoPin = A0;
int trigPin = A1;

long Timing() {
	digitalWrite(trigPin, LOW);
	delayMicroseconds(2);
	digitalWrite(trigPin, HIGH);
	delayMicroseconds(10);
	digitalWrite(trigPin, LOW);
	return pulseIn(echoPin,HIGH);
}

long Ranging(int sys) {
	long duration = Timing();
	long distacne_cm = duration / 29 / 2 ;
	long distance_inc = duration / 74 / 2;
	return (sys) ? distacne_cm : distance_inc;
}

void setup() {
    lcd.begin(16, 2);
	pinMode(echoPin, INPUT);
	pinMode(trigPin, OUTPUT);
}

void loop() {
	char line[16];
	sprintf(line, "Distance: %i cm ", Ranging(true));

	// set the cursor to column 0, line 1
	lcd.setCursor(0, 0);
	lcd.print("Testing...");

	// set the cursor to column 0, line 2
	lcd.setCursor(0, 1);
	lcd.print(line);
	delay(1000);
}

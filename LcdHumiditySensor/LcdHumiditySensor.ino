#include <LiquidCrystal.h>
#include <util/delay_basic.h>

// Initialize the LCD.
LiquidCrystal lcd(10, 11, 12, 13, 3, 4, 5, 6, 7, 8, 9);

// Initialize sensor parameters.
const int sensorReadTimeoutMillis = 1000;
const int serialBaudRate = 9600;
const int sensorReadIntervalMs = 5000;
int millisSinceLastRead = 0;
int sensorPin = 2;
int tempPin = 5;

// Microseconds since last state transition. Used to check timing, which is required
// to read bits and helpful for error checking in other states.
volatile long lastTransitionMicros = 0;

// This counter tracks how many signal line changes have happened in this
// sensor run, so we can record each pulse length in the right part of the timings
// array. We also use this for sanity checking; if we didn't get 86 pulses, something
// went wrong.
volatile int signalLineChanges;

// The timings array stores intervals between pulse changes so we can decode them
// once the sensor finishes sending the pulse sequence.
int timings[88];

// If an interrupt needs to report an error, we store the message
// here and print it once the interrupt has ended.
char errorMsgBuf[256];

// and a flag to store error state during sensor read/processing
boolean errorFlag = false;
  
// Interrupt service routine to capture timing between signal pulses and record
// them. The pulse sequence will be analysed once it's all been captured so we don't
// have to worry so much about execution time.
void sensorLineChange() {
	const long pulseMicros = micros() - lastTransitionMicros;
	lastTransitionMicros = micros();
	timings[signalLineChanges] = pulseMicros;
	signalLineChanges++;
}

void flagError() {
	// On error restore the sensor pin to pulled up with high impedance
	pinMode(sensorPin, INPUT);
	digitalWrite(sensorPin, HIGH);

	// Flag error state
	errorFlag = true;

	// then write the error msg
	Serial.println(errorMsgBuf);
}

int shiftNextBit(int oldValue, int pulseMicros, int timingIndex) {
	// The datasheet says 0 pulses are 22-26us and 1 pulses are 70us. The datasheet is a lie, there's
	// lots more variation than that. I've observed 0s from 12 <= x <= 40 for example.
	if(pulseMicros > 10 && pulseMicros < 40) {
		return (oldValue<<1) | 0;
	} else if(pulseMicros > 60 && pulseMicros < 85) {
		return (oldValue<<1) | 1;
	} else {
		sprintf(errorMsgBuf, "Bad bit pulse length: %i us at timing idx %i", pulseMicros, timingIndex);
		flagError();
		return 0xFFFF;
	}
}

int readnbits(int *timingsIdx, int nbits) {
	const int *t = timings + *timingsIdx;
	const int *tStop = t + nbits * 2;
	int result = 0;
	int pulseMicros = *t;

	while(t != tStop) {
		// Ensure the passed pulse length is within an acceptable range for a pre-bit-transmission
		// low pulse. It should be 45-55 us, but the datasheet is full of lies and we seem to need a tolerance
		// around 35-75us to get it to actually work.
		pulseMicros = *(t++);
		(*timingsIdx)++;
		if(pulseMicros <= 35 || pulseMicros >= 75) {
			sprintf(errorMsgBuf, "Low pulse before bit transmit (%i us) outside 45-70us tolerance at timing idx %i", pulseMicros, *timingsIdx);
			flagError();
		}

		result = shiftNextBit(result, *(t++), (*timingsIdx)++);
	}

	return result;
}

void analyseTimings(int * temperature_decidegrees_c, int * rel_humidity_decipercent) {
	// The pulse sequence is:
	// (Processed by requestSensorRead()):
	// LOW (1000 - 10000us) - our sensor request
	// HIGH (20-40us) - our pull high after sensor request
	// LOW (80us) - Sensor pulls down to ACK request
	// HIGH (80us) - Sensor pulls up to ACK request
	// (Processed as results here):
	// 16 bits of humidity data, 8 bits integral and 8 bits fractional part
	// 16 bits of temperature data, 8 bits integral and 8 bits fractional part
	// 8 bits of checksum
	// each bit of which is sent as:
	// LOW (50us): Expect bit pulse
	// -- THEN EITHER --
	// HIGH(18-30us): 0 bit
	// -- OR --
	// HIGH(55-85us): 1 bi
	// Start at the 6th entry of the timings array, the first pre-bit low pulse
	int timingsIdx = 5;
	int humid16 = readnbits(&timingsIdx, 16);

	if(errorFlag) {
		Serial.println("Failed to capture humidity data");
		return;
	}

	int temp16 = readnbits(&timingsIdx, 16);
	if(errorFlag) {
		Serial.println("Failed to capture temperature data");
		return;
	}

	int checksum8 = readnbits(&timingsIdx, 8);
	if(errorFlag) {
		Serial.println("Failed to capture checksum");
		return;
	}

	// Verify the checksum. "Checksum" is too good a word to describe this
	// but it'll probably help catch the odd bit error.
	byte cs = (byte)(humid16>>8) + (byte)(humid16&0xFF) + (byte)(temp16>>8) + (byte)(temp16&0xFF);
	if(cs != checksum8) {
		sprintf(errorMsgBuf, "Checksum mismatch, bad sensor read");
		flagError();
	}

	// If bit 16 is set in the temperature, temperature is negative.
	if(temp16 & (1<<15))
        temp16 = -(temp16 & (~(1<<15)));

    if(!errorFlag) {
        *temperature_decidegrees_c = temp16;
        *rel_humidity_decipercent = humid16;
    }
}

// Signal the sensor to request data.
// At entry the line should be pulled high, and it'll be pulled high
// on return.
boolean requestSensorRead() {
    // Make sure we're pulled HIGH to start with
    if(digitalRead(sensorPin) != HIGH) {
        sprintf(errorMsgBuf, "Line not HIGH at entry to requestSensorRead()");
        flagError();
        return false;
    }

    // Pulse the line low for 1-10ms (we use 7ms)
    // to request that the sensor take a reading
    digitalWrite(sensorPin, LOW);
    pinMode(sensorPin, OUTPUT);
    delayMicroseconds(7000);
  
    // Push the pin explicitly HIGH. This shouldn't really be necessary, but appears to
    // produce a more clean and definite response, plus the datasheet says we should push
    // it high for 20-40us. Without this the sensor readings tend to be unreliable.
    digitalWrite(sensorPin, HIGH);
    delayMicroseconds(30);
  
    // Go back to input mode and restore the pull-up so we can receive the sensor's
    // acknowledgement LOW pulse. The pin will remain HIGH because of the 20k pull-up
    // resistor activated by the Arduino when an OUTPUT pin is set HIGH.
    pinMode(sensorPin, INPUT);
  
    // Then wait for the sensor to pull it low. The sensor will respond
    // after between 20 and 100 microseconds by pulling the line low.
    // If it hasn't done so within 200 microseconds assume it didn't see our
    // pulse and enter error state.
    int pulseLength = pulseIn(sensorPin, LOW, 200);
    if(!pulseLength) {
        sprintf(errorMsgBuf, "Sensor read failed: Sensor never pulled line LOW after initial request");
        flagError();
        return false;
    }

    // The sensor has pulled the line low. Wait for it to return to HIGH.
    delayMicroseconds(5);
    pulseLength = pulseIn(sensorPin, HIGH, 200);
    if(!pulseLength) {
        sprintf(errorMsgBuf, "Sensor read failed: Sensor didn't go back HIGH after LOW response to read request");
        flagError();
        return false;
    }

    return true;
}

// Capture sensor data, returning true on success and false on failure.
//
// On false return the arguments are **NOT** modified.
//
// Temperature is returned in decidegrees centigrade, ie degrees C times ten.
// Divide by ten to get the real temperature. Eg a return of 325 means a
// temperature of 32.4 degrees C. Negative temperatures may be returned,
// so make sure to use the absolute of the remainder when extracting the
// fractional part, eg:
//
// int wholePart = temperature/10;
// int fractionalPart = abs(temperature%10);
//
// Humidity is returned in percentage relative humidity times ten. Divide by ten
// to get the real relative humidity. Eg a return of 631 means 63.1% humidity.
//
boolean readSensor(int *temperature_decidegrees_c, int *rel_humidity_decipercent) {
	// Clear all the sensor reader state before we start reading
	detachInterrupt(sensorPin - 2);
	for(int i = 0; i < 87; i++) { timings[i] = 0; }
	errorFlag = false;
	lastTransitionMicros = micros();
	signalLineChanges = 0;
	// TODO: memory barrier to guarantee visibility of timings array

    // Install the state transition interrupts so we see our own request pulses
    // and all the sensor replies,
    attachInterrupt(sensorPin - 2, sensorLineChange, CHANGE);

    // Send a sensor read request pulse (7000ms LOW) and check that the sensor replies with 80ms low
    if(requestSensorRead()) {
        // collect the sensor line pulses then stop capturing interrupts on the line
        // once we either time out or get all the sensor pulses we expect.
        const long startMillis = millis();
        do {
            delay(100);
        } while(signalLineChanges < 86 && (((long)millis() - startMillis) < sensorReadTimeoutMillis));

        detachInterrupt(sensorPin - 2);
        if(signalLineChanges != 86) {
            sprintf(errorMsgBuf, "MISSED INTERRUPTS: Expected 86 line changes, saw %i", signalLineChanges);
            flagError();
        } else {
            // TODO: memory barrier to ensure visibility of timings array
            // Feed the collected pulse sequence through the state machine to analyze it.
            analyseTimings(temperature_decidegrees_c, rel_humidity_decipercent);
        }
    }

    return !errorFlag;
}

float readAnalogTemp() {
	float tempc = 0; // temperature variables
	float samples[8]; // variables to make a better precision
	int i;

	// gets 8 samples of temperature
	for(i = 0; i < 8; i++) {
		samples[i] = (analogRead(tempPin) * 5.0  / 1024.0) * 100.0 - 273.15;
		tempc = tempc + samples[i];
		delay(1000);
	}

	tempc = tempc/8.0; // better precision
	return tempc;
}

void setup() {
    // set up the LCD's number of columns and rows: 
    lcd.begin(16, 2);

    // INPUT mode is the default, but being explicit never hurts
	// We set it for the digital sensor and for the analog sensor
    pinMode(tempPin, INPUT);

    // Pull the sensor pin high using the Arduino's built-in 20k
    // pull-up resistor. The sensor expects it to be pulled high
    // for the first 2s and kept high as its default state.
    digitalWrite(tempPin, HIGH);

    // The sensor expects no communication for the first 2s, so delay
    // entry to the polling loop to give it tons of time to warm up.
    delay(5000);

    // Now ensure the serial port is ready
    Serial.begin(serialBaudRate);
    Serial.println("Sensor initialized, Ready for first reading");
}

void loop() {
    int temperature = 0, humidity = 0;
	float tempc = readAnalogTemp();
	float maxT = -100.0, minT = 100.0; // to start max/min temperature
    boolean success = readSensor(&temperature, &humidity);

	if(tempc > maxT) { maxT = tempc; } // set max temperature
	if(tempc < minT) { minT = tempc; } // set min temperature

    if (success) {
        char line1[16];
        char line2[16];
        sprintf(line1, "Temp %i.%i ", temperature / 10, abs(temperature % 10));
        sprintf(line2, "Hum Rel %i.%i", humidity / 10, humidity % 10);

		// set the cursor to column 0, line 1
		// (note: line 1 is the second row, since counting begins with 0):
		lcd.setCursor(0, 0);
		lcd.print(line1);
		lcd.print(tempc, 2);

		lcd.setCursor(0, 1);
		// print the number of seconds since reset:
		lcd.print(line2);
	}

	delay(sensorReadIntervalMs);
}

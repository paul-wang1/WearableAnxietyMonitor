/*
This file contains the function definitions for the grove sensor class.
*/

#include "grove.h"
#include <Arduino.h>

/*
Input: The pin number for the analog-to-digital converter.
Purpose: Constructor for the grove sensor class.
*/
Grove::Grove(int pin) {
  ADC_pin = pin;
}

/*
Purpose: Converts the analog value from the grove sensor to digital and
returns it.
*/
int Grove::readValue() {
    return analogRead(ADC_pin);
}

/*
Purpose: Takes 10 readings from the grove sensor with a delay between each
reading, averages them, and returns the average value.
*/
int Grove::readAvgVal() {
    long sum = 0;
    for (int i = 0; i < 10; i++) {
        sum += readValue();
        delay(5);
    }
    return sum / 10;
}

/*
Input: The calibrated value for the grove sensor when no one is wearing it.
Purpose: Calculates the resistance of the skin based on the data from
the grove sensor.
*/
int Grove::calcRes(int caliVal) {
    int avgVal = readAvgVal();
    return (4096 + 2 * avgVal) * 10000 / (caliVal - avgVal);
}




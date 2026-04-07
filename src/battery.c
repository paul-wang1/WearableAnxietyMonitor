/*
This file contains the function definitions for the grove sensor class.
*/

#include "battery.h"
#include <Arduino.h>

Battery::Battery(int pin) {
    ADC_pin = pin;
}




/*
This file contains the function definitions for the grove sensor class.
*/

#include "battery.h"
#include <Arduino.h>

Battery::Battery(int pin) {
    ADC_pin = pin;
}

int Battery::readValue() {
    return analogRead(ADC_pin);
}

float Battery::calcVoltage() {
    int val = readValue();
    return (val / 4096.0) * 3.3;
}


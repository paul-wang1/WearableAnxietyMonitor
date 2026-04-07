/*
This file contains the function declarations for the battery class.
*/

#ifndef BATTERY_H
#define BATTERY_H

class Battery {
    public:
        Battery(int pin);

    private:
        int ADC_pin;
};

#endif
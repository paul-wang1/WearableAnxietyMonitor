/*
This file contains the function declarations for the grove sensor class.
*/

#ifndef GROVE_H
#define GROVE_H

/*
This is the class for the grove sensor, which contains functions to initialize
an object of the class, read from the sensor, and calculate resistance.
*/
class Grove {
  public:
    Grove(int pin);
    int readValue();
    int readAvgVal();
    int calcRes(int caliVal);
    float calcConductance(int caliVal);

  private:
    int ADC_pin;
};

#endif
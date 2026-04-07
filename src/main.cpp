#include <Arduino.h>
#include "grove.h"
#include "battery.h"

Grove groveSensor(A0);
Battery batteryReader(A1);


void setup(){
  Serial.begin(9600);
}

void loop(){
  // Serial.print(groveSensor.calcConductance(2400));
  // Serial.println();
  Serial.print(batteryReader.calcVoltage());
  Serial.println();
}

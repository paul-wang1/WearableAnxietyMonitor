#include <Arduino.h>
#include "grove.h"

Grove groveSensor(A0);

void setup(){
  Serial.begin(9600);
}

void loop(){
  Serial.print(groveSensor.calcConductance(2400));
  Serial.println();
}

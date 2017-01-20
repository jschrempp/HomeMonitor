#ifndef SISGlOBALS_H_INCLUDE
#define SISGlOBALS_H_INCLUDE

/***************************************************************************************************/
// SISGlobals.h
//  Defines all global variables used by the SIS firmware. Note that any variables
//  defined in this file are declared in the file SISGlobals.cpp
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/

#include "application.h"

// convenience constants
const String DOUBLEQ = "\"";

// Configuration constants
const int MAX_WIRELESS_SENSORS = 20;

// constants for SIS operation. Will be stored/retrieved from eeprom
extern String utcOffset;    // PST is -8
extern String observeDST;	// no" if locale does not observe DST

// Types of sensors the system supports
enum enum_sensorType {
    esensorTypeUnknown,
    ePIR,
    eSeparation,
    eExitDoor
};

// Stings that map to enum_sensorType to provide human readable descriptions
extern String sensorType_strings[];

// Information about a sensor configured in the system
struct type_sensor {
    String sensorName = "No Name Assigned";
    unsigned long activateCode = 0;
    unsigned long lastTripTime = 0;
    enum_sensorType sensorType = esensorTypeUnknown;
    bool alarmOnTrip = 0;

};

// Holds information about EVERY sensor configured in the system
extern type_sensor sensor_info[];


#endif //prevent double includes
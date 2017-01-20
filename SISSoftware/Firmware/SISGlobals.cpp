/***************************************************************************************************/
// SISGlobals.cpp
//  Declares all global variables used by the SIS firmware. Note that any variables
//  declared in this file are defined in the file SISGlobals.h
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/

#include <SISGlobals.h>

// SIS config data. This info is stored/retrieved from non-volatile memory
String utcOffset = "-8.0";	// Pacific Standard Time
String observeDST = "yes";	// no" if locale does not observe DST

// Stings that map to enum_sensorType to provide human readable descriptions
String sensorType_strings[] {
    "Unknown",
    "PIR",
    "Separation",
    "Exit Door"
};

// Holds information about EVERY sensor configured in the system
type_sensor sensor_info[MAX_WIRELESS_SENSORS];


#ifndef SISCONFIGSTORE_H_INCLUDE
#define SISCONFIGSTORE_H_INCLUDE
/***************************************************************************************************/
// SISConfigStore.h
//  Routines used by SIS to store the configuration in non-volatile memory and to retrieve
//  that information upon restart.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/

// Called once. Initializes some internal values
void configStoreInit();

// Writes configuration values to EEPROM. Can be called any time.
void writeConfig();

// Reads configuration values from EEPROM. Can be called any time.
void restoreConfig();

#endif // to prevent duplication of header files

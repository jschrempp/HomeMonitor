#ifndef SISINTERRUPTSERVICE_H_INCLUDE
#define SISINTERRUPTSERVICE_H_INCLUDE

/***************************************************************************************************/
// SISInterruptService.h
//  The routines to handle interrupt servicing and decoding of the protocol.
//
//  Call getNewSensorCode() to retrieve a new sensor code. Returns 0 if no code available.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  This software uses code extracted from the Arduino RC-Switch library:
//    https://github.com/sui77/rc-switch
// Portions of this software that have been extracted from RC-Switch are subject to the RC-Switch
// license, as well as the the SIS Terms of Use.
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/

// Called once to initialize the ISR module
void initISR();

// Call to get a new code if one is available. Once called the next call will
// return 0 unless another code has been received.
unsigned long getNewSensorCode();

// Call to keep a simulation test running
void simulateSensor();






#endif  // prevent duplicate header includes

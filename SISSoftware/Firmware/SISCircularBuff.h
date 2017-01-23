#ifndef SISCIRCULARBUFF_H_INCLUDE
#define SISCIRCULARBUFF_H_INCLUDE

/***************************************************************************************************/
// SISCircularBuff.h
//  A cicular buffer used by SIS to hold a set sensor trips and events.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
#include "application.h"

// Called to find out how many cBuf entries remain to be published to the cloud
// return:  int of the number of entries that remain to be published to spark cloud.
int getNumToPublish();

// Called whenever a cBuf entry is published to the cloud
// Decrements an internal module counter.
void decrementNumToPublish();

// Insert a string of data into the circular buffer at the tail.
// If the buffer is full this will remove the oldest entry
void cBufInsert(String data);

// Read a value from the circular buffer, offset position.
//     offset = 0 is most recent entry
// This read is non destructive
String cBufRead(int offset);

// Read the circular buffer at position Offset and return a string in stringPtr
// that is formatted for human consumption. Returns -1 if the specified Offset
// is beyong the end of the circular buffer.
//    offset is an absolute position in cBuf, not relative to current head/tail
int readFromBuffer(int offset, char stringPtr[]);

#endif // prevent double includes

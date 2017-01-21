/***************************************************************************************************/
// SISCircularBuff.cpp
//  A cicular buffer used by SIS to hold a set sensor trips and events.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
 #include <SISCircularBuff.h>
 #include <SISGlobals.h>

 String cBuf[BUF_LEN];   // circular buffer to store events and messages as they happen
                         // Expected format of the string stored in cBuf is:
                         // TYPE,SEQUENCENUMBER,INDEX,EPOCTIME
                         // where
                         //    TYPE is A (advisory) or S (sensor)
                         //    SEQUENCENUMBER is a monotonically increasing global (eventNumber)
                         //    INDEX is into sensorName[] for type sensor
                         //          or enum_messageIndex for type advisory
                         //    EPOCTIME is when the entry happened
                         // see cBufInsert(), cBufRead(), readFromBuffer(), logSensor(), logMessage()

 int head = 0;       	// index of the head of the circular buffer
 int tail = 0;       	// index of the tail of the buffer
 int g_numToPublish = -1; // Number of entries in cBuf[] that remain to be published to spark cloud.
                          // This is incremented when events are added to the cBuf[] and decremented
                          // when an entry is published to the spark cloud.

/******************************************** getNumToPublish() *****************************************/
// getNumToPublish():  Called to find out how many cBuf entries remain to be published to the cloud
//  Arguments: none
//	return:  int of the number of entries that remain to be published to spark cloud.
int getNumToPublish() {

    return g_numToPublish;
}

/******************************************** decrementNumToPublish() *****************************************/
// decrementNumToPublish(): Called whenever a cBuf entry is published to the cloud
//  Arguments: none
//	return:  none
void decrementNumToPublish() {

    g_numToPublish--;
}

/******************************************** cBufInsert() *****************************************/
// cBufInsert():  insert a string into the circular buffer at the current tail position.
//  Arguments:
//	String data:  the string data (string object) to store in the circular buffer
//	return:  none.

void cBufInsert(String data)
{
  static boolean fullBuf = false;	// false for the first pass (empty buffer locations)

  cBuf[tail] = data;	// write the data at the end of the buffer
  g_numToPublish++;     // note that there is a new buffer entry to publish

  //  adjust the tail pointer to the next location in the buffer
  tail++;
  if(tail >= BUF_LEN)
  {
	tail = 0;
	fullBuf = true;
  }

  //  the first time through the buffer, the head pointer stays put, but after the
  //	buffer wraps around, the head of the buffer is the tail pointer position
  if(fullBuf)
  {
	head = tail;
  }

}
/***************************************** end of cBufInsert() **************************************/

/********************************************* cBufRead() *******************************************/
// cBufRead():  read back a String object from the "offset" location in the cirular buffer.  The
//	offset location of zero is the latest value in (tail of) the circular buffer.
//  Arguments:
//	int offset:  the offset into the buffer where zero is the most recent entry in the circular buffer
//       and 1 is the next most recent, etc.
//  Return:  the String at the offset location in the circular buffer.

String cBufRead(int offset)
{
  int locationInBuffer;

  locationInBuffer = tail -1 - offset;
  if(locationInBuffer < 0)
  {
    locationInBuffer += BUF_LEN;
  }

  return cBuf[locationInBuffer];

}
/****************************************** end of cBufRead() ***************************************/

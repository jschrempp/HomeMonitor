/***************************************************************************************************/
// TPPCircularBuff.cpp
//  A cicular buffer used by SIS to hold a set sensor trips and events.
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
 #include <TPPCircularBuff.h>
 #include <SISGlobals.h>
 #include <TPPUtils.h>

 String cBuf[BUF_LEN];   // circular buffer to store events and messages as they happen
                         // Expected format of the string stored in cBuf is:
                         // TYPE,SEQUENCENUMBER,INDEX,EPOCTIME
                         // where
                         //    TYPE is A (advisory) or S (sensor)
                         //    SEQUENCENUMBER is a monotonically increasing global (eventNumber)
                         //    INDEX is into sensorName[] for type sensor
                         //          or enum_messageIndex for type advisory
                         //    EPOCTIME is when the entry happened
                         // see cBufInsert(), cBufRead(), readSISFromBuffer(), logSensor(), logMessage()

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


/********************************** readSISFromBuffer() ******************************************/
// readSISFromBuffer(): utility fujction to read from the circular buffer into the
//  character array passed in as stringPtr[].
//  This routine is specific for the SIS format strings in the circular buffer.
//  Arguments:
//      int offset: the offset into the circular buffer to read from. 0 is the latest entry.  The
//          next to latest entry is 1, etc. back to BUF_LEN -1.
//       	BUF_LEN can be determined from the cloud variable "bufferSize".  If location exceeds
//       	BUF_LEN - 1, the value that is read out is the oldest value in the buffer, and
//       	-1 is returned by the function.  Otherwise, the value is determined by location
//       	and 0 is returned by this function.
//      char stringPtr[]: pointer to the string that will be returned from this
//        function. The format of the string expected by the web site is one of:
//            (S:nnn) SENSORNAME tripped at DATETIME Z (epoc:EPOCTIME)
//            (S:nnn) SENSORNUMBER detected at DATETIME Z (epoc:EPOCTIME)
//
//  Return:  0 if a valid location was specified, otherwise, -1.
// XXX this routine expects the circular buffer entries to be in a very
//     specific format. Perhaps we should specify that format, or verify it,
//     in cBufInsert? Or have a magic number in the entry so that this routine
//     can test for the magic number before trying to format the entry.

int readSISFromBuffer(int offset, char stringPtr[])
{
	int result;     	// the result code to return
    String g_bufferReadout = String("");   // temporary use


	// check and fix, if necessary, the offset into the circular buffer
	if(offset >= BUF_LEN)
	{
    	offset = BUF_LEN - 1;   // the maximum offset possible
    	result = -1;        	// return the error code
	}
	else
	{
    	result = 0;         	// return no error code
	}


	// now retrieve the data requested from the circular buffer and place the result string
    // in g_bufferReadout
	g_bufferReadout = "" + cBufRead(offset);

	#ifdef DEBUG
    	Serial.println(g_bufferReadout);
	#endif

	// create the readout string for the cloud from the buffer data
	if(g_bufferReadout != "")  // skip empty log entries
	{
    	int index;

       // parse the comma delimited string into its substrings
      // result of parse is in global array g_dest[]

    	parser(g_bufferReadout);

    	// format the sequence number and place into g_bufferReadout
        g_bufferReadout = "(S:";
        g_bufferReadout += g_dest[1];
        g_bufferReadout += ")";

        // Determine message type
    	if(g_dest[0] == "S")  	// sensor type message
    	{

        	// format the sensor Name from the index
        	index = g_dest[2].toInt();
            g_bufferReadout += sensor_info[index].sensorName;
            g_bufferReadout += " tripped at ";
    	}
    	else    	// advisory type message
    	{
            g_bufferReadout += g_dest[2];
            g_bufferReadout += " detected at ";
    	}

    	// add in the timestamp

    	index = g_dest[3].toInt();
        g_bufferReadout += Time.timeStr(index).c_str();
        g_bufferReadout += " Z (epoch:";
        g_bufferReadout += g_dest[3];
        g_bufferReadout += "Z)";

	}

    g_bufferReadout.toCharArray(stringPtr, g_bufferReadout.length() + 1 );
	stringPtr[g_bufferReadout.length() + 2] = '\0';

	return result;

}

/********************************** end of readSISFromBuffer() ****************************************/

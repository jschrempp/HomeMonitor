/***************************************************************************************************/
// SISUtils.h
//  Routines of general use
//
//  Use of this software is subject to the Terms of Use which can be found at:
//  https://github.com/TeamPracticalProjects/SISProject/blob/master/SISDocs/Terms_of_Use_License_and_Disclaimer.pdf
//
//  1/20/2017
//
//  (c) 2015, 2016, 2017 by Bob Glicksman and Jim Schrempp
/***************************************************************************************************/
#include <SISUtils.h>
#include <SISGlobals.h>

/*************************************** parser() **********************************************/
// parser(): parse a comma delimited string into its individual substrings.
//  Arguments:
//  	source:  String object to parse
//  Return: the number of substrings found and parsed out
//
//  This functions uses the following global constants and variables:
//  	const int MAX_SUBSTRINGS -- nominal value is 6
//  	String g_dest[MAX_SUBSTRINGS] -- array of Strings to hold the parsed out substrings

// XXX This should be made a generic routine that returns an array of strings so
//     that we don't have to use a global.

int parser(String source)
{
	int lastComma = 0;
	int presentComma = 0;

	//parse the string argument until there are no more substrings or until MAX_SUBSTRINGS
	//  are parsed out
	int index = 0;
	do
	{
    	presentComma = source.indexOf(',', lastComma);
    	if(presentComma == -1)
    	{
        	presentComma = source.length();
    	}
      g_dest[index++] = "" + source.substring(lastComma, presentComma);
    	lastComma = presentComma + 1;

	} while( (lastComma < source.length() ) && (index < MAX_SUBSTRINGS) );

	return (index);
}
/************************************ end of parser() ********************************************/

/*
copyright 1990 through 2009 by Mel Bartels

	 This file is part of scope.exe the stepper version.

	 Scope.exe is free software; you can redistribute it and/or modify
	 it under the terms of the GNU General Public License as published by
	 the Free Software Foundation; either version 2 of the License, or
	 (at your option) any later version.

	 Scope.exe is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	 GNU General Public License for more details.

	 You should have received a copy of the GNU General Public License
	 along with scope.exe; if not, write to the Free Software
	 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <math.h>
#include <time.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <values.h>
#include <string.h>
#include <ctype.h>
#include <dir.h>
#include <process.h>
#include <graphics.h>
#include "header.h"

/* encoders... */

/* for Tangent types such as MGIII, Ouranos, BBox, NGC models, Celestron Advanced AstroMaster, and
StarPort units: transmit 'Q', receive '+12345<tab>+67890<return>' = total of 14 chars,
+12345 = azimuth, <tab> = decimal 9, +67890 = altitude, <return> = decimal 13;

if 4000 count encoders, then output is from -2000 to +1999, representing -180 to 180 degrees;

to change resolution, output 'Rxxxxx<tab>yyyyy<return>' where xxxxx is azimuth and yyyyy is
altitude, if resolution > 32767 then counts returned as unsigned, counts go from 0 to resolution-1,
max resolution is 65534, if command is successful, receive 'R'

some units set resolution by outputting 'Zxxxxx<space>yyyyy' where xxxxx is resolution in x axis and
yyyyy is resolution in y axis; properly formatted 'Z' command returns "*"; check current
configuration by sending 'H', unit will respond by sending current configuration

for BSeg unit:
transmit 'B' for native mode, receive 'B'<return> if successful;
transmit 'Q', receive 'HHHH<tab>HHHH<tab>HHHH<return> where HHHH is an unsigned hexidecimal number;
set encoder resolution via jumpers on BSeg PCB

for Dave Ek unit:
transmit 'z' followed by 4 bytes: dec enc. low byte, dec enc. high byte, RA enc. low byte, RA enc.
high byte, and expect 'r' in return;
transmit 'y', receive 4 bytes of enc. positions per byte order above

Sky Commander: transmit 'e'; receive az most significant byte, az least significant byte, alt most
significant byte, alt least significant byte

For later models of Sky Commander: transmit return and receive:
[1] space
[2,3] integer part of RA hours
[4] fixed decimal point
[5-7] fractional part of RA hours
[8] space
[9] + or - for declination
[10,11] integer part of Declination
[12] fixed decimal point
[13-15] fractional part of dec
[16] NULL
These equatorial must be transformed into altaz coordinates, then into equivalent encoder counts.

for LM629Serial: trasmit 'P#' this queries the LM629 for its current x and y encoder positions;
the return is x:y which would be the current encoder positions for x (az) and y (alt);
x and y have a range of +1,073,741,824 to -1,073,741,824. */

void InitEncoders( void)
{
	EncoderType = NoEncoders;
	if( strcmpi( EncoderString, NoEncodersStr) == 0)
	{
		EncoderType = NoEncoders;
		EncoderState = NotInitialized;
		if( DisplayOpeningMsgs)
			printf( "\nNo encoders indicated in %s", ConfigFile);
	}
	else
		if( !EncoderComPort && strcmpi( EncoderString, EncoderMouseStr) != 0)
		{
			EncoderType = NoEncoders;
			EncoderState = NotInitialized;
			if( DisplayOpeningMsgs)
				printf( "\nno encoders since encoder com port set to zero");
		}
		else
		{
			if( MakeEncoderResetLogFile)
			{
				EncoderOutput = fopen( EncoderResetLogFile, "w");
				if( Input == NULL)
					BadExit( strcat( "Could not open ", EncoderResetLogFile));
			}

			if( strcmpi( EncoderString, EncoderBSegStr) == 0)
			{
				EncoderType = EncoderBSeg;
				ResetEncoders_f_ptr = ResetEncodersBSeg;
				ReadEncoders_f_ptr = ReadEncodersBSeg;
				QueryEncoders_f_ptr = QueryEncodersQ;
				InitEncodersBSeg();
			}
			else
				if( strcmpi( EncoderString, EncoderResetViaRStr) == 0)
				{
					EncoderType = EncoderResetViaR;
					ResetEncoders_f_ptr = ResetEncodersResetViaR;
					ReadEncoders_f_ptr = ReadEncodersQ;
					QueryEncoders_f_ptr = QueryEncodersQ;
					InitEncodersResetViaR();
				}
				else
					if( strcmpi( EncoderString, EncoderResetViaZStr) == 0)
					{
						EncoderType = EncoderResetViaZ;
						ResetEncoders_f_ptr = ResetEncodersResetViaZ;
						ReadEncoders_f_ptr = ReadEncodersQ;
						QueryEncoders_f_ptr = QueryEncodersQ;
						InitEncodersResetViaZ();
					}
					else
						if( strcmpi( EncoderString, EncoderNoResetStr) == 0)
						{
							EncoderType = EncoderNoReset;
							ResetEncoders_f_ptr = ResetEncodersNoReset;
							ReadEncoders_f_ptr = ReadEncodersQ;
							QueryEncoders_f_ptr = QueryEncodersQ;
							InitEncodersNoReset();
						}
						else
							if( strcmpi( EncoderString, EncoderMouseStr) == 0)
							{
								EncoderComPort = 0;
								EncoderType = EncoderMouse;
								ResetEncoders_f_ptr = ResetEncodersMouse;
								ReadEncoders_f_ptr = ReadEncodersMouse;
								QueryEncoders_f_ptr = QueryEncodersMouse;
								InitEncodersMouse();
							}
							else
								if( strcmpi( EncoderString, EncoderEkStr) == 0)
								{
									EncoderType = EncoderEk;
									ResetEncoders_f_ptr = ResetEncodersEk;
									ReadEncoders_f_ptr = ReadEncodersEk;
									QueryEncoders_f_ptr = QueryEncoders_y;
									InitEncodersEk();
								}
								else
									if( strcmpi( EncoderString, EncoderSkyCommanderStr) == 0)
									{
										EncoderType = EncoderSkyCommander;
										ResetEncoders_f_ptr = ResetEncodersNoReset;
										ReadEncoders_f_ptr = ReadEncodersSkyCommander;
										QueryEncoders_f_ptr = QueryEncoders_e;
										InitEncodersSkyCommander();
									}
									else
										if( strcmpi( EncoderString, EncoderSkyCommanderRStr) == 0)
										{
											EncoderType = EncoderSkyCommanderR;
											ResetEncoders_f_ptr = ResetEncodersNoReset;
											ReadEncoders_f_ptr = ReadEncodersSkyCommanderR;
											QueryEncoders_f_ptr = QueryEncoders_cr;
											InitEncodersSkyCommanderR();
										}
										else
											BadExit( strcat( "Bad EncoderString: ", EncoderString));
	}
	if( EncoderComPort < 0 || EncoderComPort > 4)
		BadExit( "EncoderComPort must be 0, 1, 2, 3, or 4");
	if( EncoderType && !AltEncoderCountsPerRev)
		BadExit( "AltEncoderCountsPerRev must be > 0");
	if( EncoderType && !AzEncoderCountsPerRev)
		BadExit( "AzEncoderCountsPerRev must be > 0");

	EncoderTrackOnResetCount = 0;
	EncoderTrackOffResetCount = 0;
	EncoderSlewResetCount = 0;
}

void CloseEncoderResetLogFile( void)
{
	if( MakeEncoderResetLogFile)
		fclose( EncoderOutput);
}

void InitEncodersBSeg( void)
{
	Byte B;
	Flag QuitFlag = No;

	ResetEncoderBox = True;
	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder BSeg box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder BSeg box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder BSeg box");
		}
		else
			printf( " no");
	}
	if( ResetEncoderBox)
	{
		if( DisplayOpeningMsgs)
		{
			printf( "\nPress any key after turning on the encoder BSeg box...\n");
			while( !QuitFlag)
			{
				if( ReadSerial( EncoderComPort, &B))
					printf( "%c", B);
				if( KeyStroke)
				{
					QuitFlag = Yes;
					B = getch();
				}
			}
		}
		ResetEncoders_f_ptr();
		/* report results */
		if( DisplayOpeningMsgs)
		{
			if( EncoderState == Read)
				printf( "\nSet encoder BSeg box to native mode");
			else
				printf( "\nCould not set encoder BSeg box to native mode");
		}
		/* get initial encoder counts */
		if( EncoderState == Read)
		{
			QueryAndReadEncoders();
			if( DisplayOpeningMsgs)
				if( EncoderState == Read)
					printf( "\nRead encoder BSeg box");
				else
					printf( "\nCould not read encoder BSeg box");
		}
	}
}

void InitEncodersResetViaR( void)
{
	Byte B;
	Flag QuitFlag = No;

	ResetEncoderBox = True;
	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder ResetViaR box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder ResetViaR box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder ResetViaR box");
		}
		else
			printf( " no");
	}
	if( ResetEncoderBox)
	{
		if( DisplayOpeningMsgs)
		{
			printf( "\nPress any key after turning on the encoder ResetViaR box...\n");
			while( !QuitFlag)
			{
				if( ReadSerial( EncoderComPort, &B))
					printf( "%c", B);
				if( KeyStroke)
				{
					QuitFlag = Yes;
					B = getch();
				}
			}
		}
		ResetEncoders_f_ptr();
		/* report results */
		if( DisplayOpeningMsgs)
		{
			if( EncoderState == Read)
				printf( "\nReset encoder ResetViaR box to %ld %ld counts per revolution",
				AltEncoderCountsPerRev, AzEncoderCountsPerRev);
			else
				printf( "\nCould not set encoder ResetViaR box");
		}
		/* get initial encoder counts */
		if( EncoderState == Read)
		{
			QueryAndReadEncoders();
			if( DisplayOpeningMsgs)
				if( EncoderState == Read)
					printf( "\nRead encoder ResetViaR box");
				else
					printf( "\nCould not read encoder ResetViaR box");
		}
	}
}

void InitEncodersResetViaZ( void)
{
	Byte B;
	Flag QuitFlag = No;

	ResetEncoderBox = True;
	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder ResetViaZ box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder ResetViaZ box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder ResetViaZ box");
		}
		else
			printf( " no");
	}
	if( ResetEncoderBox)
	{
		if( DisplayOpeningMsgs)
		{
			printf( "\nPress any key after turning on the encoder ResetViaZ box...\n");
			while( !QuitFlag)
			{
				if( ReadSerial( EncoderComPort, &B))
					printf( "%c", B);
				if( KeyStroke)
				{
					QuitFlag = Yes;
					B = getch();
				}
			}
		}
		ResetEncoders_f_ptr();
		/* report results */
		if( DisplayOpeningMsgs)
		{
			if( EncoderState == Read)
				printf( "\nReset encoder ResetViaZ box to %ld %ld counts per revolution",
				AltEncoderCountsPerRev, AzEncoderCountsPerRev);
			else
				printf( "\nCould not set encoder ResetViaZ box");
		}
		/* get initial encoder counts */
		if( EncoderState == Read)
		{
			QueryAndReadEncoders();
			if( DisplayOpeningMsgs)
				if( EncoderState == Read)
					printf( "\nRead encoder ResetViaZ box");
				else
					printf( "\nCould not read encoder ResetViaZ box");
		}
	}
}

void InitEncodersNoReset( void)
{
	Byte B;
	Flag QuitFlag = No;

	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder NoReset box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder NoReset box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder NoReset box");
		}
		else
			printf( " no");
	}
	if( DisplayOpeningMsgs)
	{
		printf( "\nPress any key after turning on the encoder NoReset box...\n");
		while( !QuitFlag)
		{
			if( ReadSerial( EncoderComPort, &B))
				printf( "%c", B);
			if( KeyStroke)
			{
				QuitFlag = Yes;
				B = getch();
			}
		}
	}
	/* get initial encoder counts */
	QueryAndReadEncoders();
	if( DisplayOpeningMsgs)
		if( EncoderState == Read)
			printf( "\nRead encoder NoReset box");
		else
			printf( "\nCould not read encoder NoReset box");
}

void InitEncodersMouse( void)
{
	/* this function calls BadExit() if not successful */
	ResetMouse();
	if( DisplayOpeningMsgs)
		printf( "\nUsing mouse encoder");
	/* set relative counters to zero */
	GetMousePositionRelative();
	EncoderState = Read;
}

void InitEncodersEk( void)
{
	Byte B;
	Flag QuitFlag = No;

	ResetEncoderBox = True;
	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder Ek box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder Ek box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder Ek box");
		}
		else
			printf( " no");
	}
	if( ResetEncoderBox)
	{
		if( DisplayOpeningMsgs)
		{
			printf( "\nPress any key after turning on the encoder Ek box...\n");
			while( !QuitFlag)
			{
				if( ReadSerial( EncoderComPort, &B))
					printf( "%c", B);
				if( KeyStroke)
				{
					QuitFlag = Yes;
					B = getch();
				}
			}
		}
		ResetEncoders_f_ptr();
		/* report results */
		if( DisplayOpeningMsgs)
		{
			if( EncoderState == Read)
				printf( "\nReset encoder Ek box to %ld %ld counts per revolution",
				AltEncoderCountsPerRev, AzEncoderCountsPerRev);
			else
				printf( "\nCould not set encoder Ek box");
		}
		/* get initial encoder counts */
		if( EncoderState == Read)
		{
			QueryAndReadEncoders();
			if( DisplayOpeningMsgs)
				if( EncoderState == Read)
					printf( "\nRead encoder Ek box");
				else
					printf( "\nCould not read encoder Ek box");
		}
	}
}

void InitEncodersSkyCommander( void)
{
	Byte B;
	Flag QuitFlag = No;

	if( DisplayOpeningMsgs)
	{
		printf( "\nUse current encoder Sky Commander box coordinates (y/n)? ");
		Response = getch();
		if( Response == 'Y' || Response == 'y' || Response == Return)
		{
			printf( " yes");
			QueryAndReadEncoders();
			if( EncoderState == Read)
			{
				ResetEncoderBox = False;
				printf( "\nSuccessfully read encoder Sky Commander box alt: %6ld az: %6ld",
				EncoderCount.A, EncoderCount.Z);
			}
			else
				printf( "\nCould not read encoder Sky Commander box");
		}
		else
			printf( " no");
	}
	if( DisplayOpeningMsgs)
	{
		printf( "\nPress any key after turning on the encoder Sky Commander box...\n");
		while( !QuitFlag)
		{
			if( ReadSerial( EncoderComPort, &B))
				printf( "%c", B);
			if( KeyStroke)
			{
				QuitFlag = Yes;
				B = getch();
			}
		}
	}
	/* get initial encoder counts */
	QueryAndReadEncoders();
	if( DisplayOpeningMsgs)
		if( EncoderState == Read)
			printf( "\nRead encoder Sky Commander box");
		else
			printf( "\nCould not read encoder Sky Commander box");
}

void InitEncodersSkyCommanderR( void)
{
	Byte B;
	Flag QuitFlag = No;
	int EncByteCount = 0;

	if( DisplayOpeningMsgs)
	{
		printf( "\nPress any key after turning on the the Sky Commander encoder box...\n");
		EncoderState = ReadReady;
		InitRingBuffers( EncoderComPort);
		WriteSerial( EncoderComPort, (Byte) Return);
		while( !QuitFlag)
		{
			if( ReadSerial( EncoderComPort, &B))
			{
				printf( "%c", B);
				EncByteCount++;
			}
			if( KeyStroke)
			{
				QuitFlag = Yes;
				B = getch();
			}
		}
		if( EncByteCount == MaxEncSkyCommanderR)
		{
			EncoderState = Read;
			if( DisplayOpeningMsgs)
				printf( "\nSky Commander encoder box successfully read.\n");
		}
		else
			if( DisplayOpeningMsgs)
				printf( "\nSky Commander encoder box not found.\n");
	}
}

/* sets BSeg to native mode */
void ResetEncodersBSeg( void)
{
	Byte B1 = 0;
	Byte B2 = 0;

	WriteSerial( EncoderComPort, 'B');
	delay( SerialWriteDelayMs);
	ReadSerial( EncoderComPort, &B1);
	ReadSerial( EncoderComPort, &B2);
	if( B1 == 'B' && B2 == Return)
		EncoderState = Read;
	else
		EncoderState = CommError;
}

void ResetEncodersResetViaR( void)
{
	Byte B = 0;

	/* send Rzzzzz<Tab>aaaaa<Return>, if successful, expect 'R' */
	EncStr[0] = 'R';
	EncStr[6] = Tab;
	EncStr[12] = Return;

	BuildEncStrRes();

	for( EncIx = 0; EncIx < MaxEncQRZ; EncIx++)
	{
		WriteSerial( EncoderComPort, EncStr[EncIx]);
		delay( SerialWriteDelayMs);
	}
	/* get to last char sent */
	while( ReadSerial( EncoderComPort, &B))
		;
	if( B == 'R')
		EncoderState = Read;
	else
		EncoderState = CommError;
}

void ResetEncodersResetViaZ( void)
{
	Byte B = 0;

	/* send Zxxxxx<space>yyyyy<Return>, if successful, expect '*' */
	EncStr[0] = 'Z';
	EncStr[6] = ' ';
	EncStr[12] = Return;

	BuildEncStrRes();

	for( EncIx = 0; EncIx < MaxEncQRZ; EncIx++)
	{
		WriteSerial( EncoderComPort, EncStr[EncIx]);
		delay( SerialWriteDelayMs);
	}
	/* get to last char sent */
	while( ReadSerial( EncoderComPort, &B))
		;
	if( B == '*')
		EncoderState = Read;
	else
		EncoderState = CommError;
}

void ResetEncodersNoReset( void)
{
	EncoderState = Read;
}

void ResetEncodersMouse( void)
{
	GetMousePositionRelative();
	EncoderState = Read;
}

void ResetEncodersEk( void)
{
	Byte A, B, C, D;

	B = (int) (AltEncoderCountsPerRev / 256);
	A = (int) (AltEncoderCountsPerRev - B*256);
	D = (int) (AzEncoderCountsPerRev / 256);
	C = (int) (AzEncoderCountsPerRev - D*256);

	WriteSerial( EncoderComPort, 'z');
	WriteSerial( EncoderComPort, A);
	WriteSerial( EncoderComPort, B);
	WriteSerial( EncoderComPort, C);
	WriteSerial( EncoderComPort, D);
	delay( SerialWriteDelayMs);

	/* get to last char sent */
	while( ReadSerial( EncoderComPort, &B))
		;
	if( B == 'r')
		EncoderState = Read;
	else
		EncoderState = CommError;
}

void BuildEncStrRes( void)
{
	unsigned Res;
	int X;

	Res = (unsigned) AzEncoderCountsPerRev;
	X = Res/10000;
	EncStr[1] = '0' + X;
	Res -= X*10000;
	X = Res/1000;
	EncStr[2] = '0' + X;
	Res -= X*1000;
	X = Res/100;
	EncStr[3] = '0' + X;
	Res -= X*100;
	X = Res/10;
	EncStr[4] = '0' + X;
	Res -= X*10;
	EncStr[5] = '0' + Res;

	Res = (unsigned) AltEncoderCountsPerRev;
	X = Res/10000;
	EncStr[7] = '0' + X;
	Res -= X*10000;
	X = Res/1000;
	EncStr[8] = '0' + X;
	Res -= X*1000;
	X = Res/100;
	EncStr[9] = '0' + X;
	Res -= X*100;
	X = Res/10;
	EncStr[10] = '0' + X;
	Res -= X*10;
	EncStr[11] = '0' + Res;
}

void QueryEncodersQ( void)
{
	InitRingBuffers( EncoderComPort);
	WriteSerial( EncoderComPort, (Byte) 'Q');
	EncoderState = ReadReady;
	for( EncIx = 0; EncIx < MaxEnc; EncIx++)
		EncStr[EncIx] = 0;
	EncIx = 0;
}

void QueryEncoders_y( void)
{
	InitRingBuffers( EncoderComPort);
	WriteSerial( EncoderComPort, (Byte) 'y');
	EncoderState = ReadReady;
	for( EncIx = 0; EncIx < MaxEnc; EncIx++)
		EncStr[EncIx] = 0;
	EncIx = 0;
}

void QueryEncoders_e( void)
{
	InitRingBuffers( EncoderComPort);
	WriteSerial( EncoderComPort, (Byte) 'e');
	EncoderState = ReadReady;
	for( EncIx = 0; EncIx < MaxEnc; EncIx++)
		EncStr[EncIx] = 0;
	EncIx = 0;
}

void QueryEncoders_cr( void)
{
	InitRingBuffers( EncoderComPort);
	WriteSerial( EncoderComPort, (Byte) Return);
	EncoderState = ReadReady;
	for( EncIx = 0; EncIx < MaxEnc; EncIx++)
		EncStr[EncIx] = 0;
	EncIx = 0;
}

void QueryEncodersMouse( void)
{
	GetMousePositionRelative();
	EncoderState = ReadReady;
}

void ReadEncodersQ( void)
{
	char Num[7];

	while( EncIx<MaxEncQRZ && ReadSerial( EncoderComPort, &EncStr[EncIx++]))
		;
	if( EncIx==MaxEncQRZ
	&& (EncStr[6]==Tab || EncStr[6]==Blank)
	&& EncStr[13]==Return)
	{
		Num[6] = '\0';

		/* azimuth */
		strncpy( Num, (char*) &EncStr[0], 6);
		EncoderCount.Z = atol( Num);

		/* altitude */
		strncpy( Num, (char*) &EncStr[7], 6);
		EncoderCount.A = atol( Num);

		EncIx = 0;
		EncoderState = Read;

		if( !AltEncoderDir)
			if (EncoderType == EncoderResetViaR && AltEncoderCountsPerRev <= 32767)
				/* encoder counts from - values to + values... */
				EncoderCount.A = -EncoderCount.A;
			else
				/* encoder counts from 0 up... */
				EncoderCount.A = AltEncoderCountsPerRev - EncoderCount.A;
		if( !AzEncoderDir)
			if (EncoderType == EncoderResetViaR && AltEncoderCountsPerRev <= 32767)
				EncoderCount.Z = -EncoderCount.Z;
			else
				EncoderCount.Z = AzEncoderCountsPerRev - EncoderCount.Z;
	}
	else
		EncoderState = NoRead;
}

/*                       1111   1
index: 0123  4  5678  9  0123   4
char:  HHHH<tab>HHHH<tab>HHHH<return> */

/* BSeg box must be jumpered to output 3 encoders (alt, az, field rotation) */
void ReadEncodersBSeg( void)
{
	char Num[7];
	char* EndPtr;

	while( EncIx<MaxEncBSeg && ReadSerial( EncoderComPort, &EncStr[EncIx++]))
		;
	if( EncIx==MaxEncBSeg && EncStr[4]==Tab && EncStr[9]==Tab
	&& EncStr[14]==Return)
	{
		Num[0] = '0';
		Num[1] = 'x';
		Num[6] = '\0';

		/* azimuth */
		strncpy( &Num[2], (char*) &EncStr[0], 4);
		/* 0x10 means that Num is in hex (base 0x10) */
		EncoderCount.Z = strtol( Num, &EndPtr, 0x10);

		/* altitude */
		strncpy( &Num[2], (char*) &EncStr[5], 4);
		EncoderCount.A = strtol( Num, &EndPtr, 0x10);

		EncIx = 0;
		EncoderState = Read;

		if( !AltEncoderDir)
			EncoderCount.A = AltEncoderCountsPerRev - EncoderCount.A;
		if( !AzEncoderDir)
			EncoderCount.Z = AzEncoderCountsPerRev - EncoderCount.Z;
	}
	else
		EncoderState = NoRead;
}

void ReadEncodersMouse( void)
{
	if( AltEncoderDir)
		EncoderCount.A += MouseYMickeyRelative;
	else
		EncoderCount.A -= MouseYMickeyRelative;
	if( EncoderCount.A > AltEncoderCountsPerRev)
		EncoderCount.A -= AltEncoderCountsPerRev;
	if( EncoderCount.A < 0L)
		EncoderCount.A += AltEncoderCountsPerRev;

	if( AzEncoderDir)
		EncoderCount.Z += MouseXMickeyRelative;
	else
		EncoderCount.Z -= MouseXMickeyRelative;
	if( EncoderCount.Z > AzEncoderCountsPerRev)
		EncoderCount.Z -= AzEncoderCountsPerRev;
	if( EncoderCount.Z < 0L)
		EncoderCount.Z += AzEncoderCountsPerRev;

	EncoderState = Read;
}

void ReadEncodersEk( void)
{
	unsigned A, B, C, D;

	while( EncIx<MaxEncEk && ReadSerial( EncoderComPort, &EncStr[EncIx++]))
		;
	if( EncIx==MaxEncEk)
	{
		A = (unsigned) EncStr[0];
		B = (unsigned) EncStr[1];
		C = (unsigned) EncStr[2];
		D = (unsigned) EncStr[3];
		EncoderCount.A = (B<<8) + A;
		EncoderCount.Z = (D<<8) + C;
		EncoderState = Read;
		if( !AltEncoderDir)
			EncoderCount.A = AltEncoderCountsPerRev - EncoderCount.A;
		if( !AzEncoderDir)
			EncoderCount.Z = AzEncoderCountsPerRev - EncoderCount.Z;
	}
	else
		EncoderState = NoRead;
}

void ReadEncodersSkyCommander( void)
{
	unsigned A, B, C, D;

	while( EncIx<MaxEncEk && ReadSerial( EncoderComPort, &EncStr[EncIx++]))
		;
	if( EncIx==MaxEncEk)
	{
		A = (unsigned) EncStr[0];
		B = (unsigned) EncStr[1];
		C = (unsigned) EncStr[2];
		D = (unsigned) EncStr[3];
		EncoderCount.A = (C<<8) + D;
		EncoderCount.Z = (A<<8) + B;
		EncoderState = Read;
		if( !AltEncoderDir)
			EncoderCount.A = AltEncoderCountsPerRev - EncoderCount.A;
		if( !AzEncoderDir)
			EncoderCount.Z = AzEncoderCountsPerRev - EncoderCount.Z;
	}
	else
		EncoderState = NoRead;
}

void ReadEncodersSkyCommanderR( void)
{
	char ReadStr[MaxEncSkyCommanderR];
	double Ra = 0;
	double Dec = 0;
	struct Position Temp;

	while( EncIx<MaxEncSkyCommanderR && ReadSerial( EncoderComPort, &EncStr[EncIx++]))
		;

	if( EncIx==MaxEncSkyCommanderR)
	{
		for( EncIx = 0; EncIx<MaxEncSkyCommanderR; EncIx++)
			ReadStr[EncIx] = EncStr[EncIx];
		sscanf( ReadStr, "%lf %lf", &Ra, &Dec);
      //gotoxy( 3, 3); printf("   %f %f   ", Ra, Dec);

		if( EncoderState > NotInitialized && Two.Init)
		{
			Temp.Ra = Current.Ra;
			Temp.Dec = Current.Dec;
			Temp.Az = Current.Az;
			Temp.Alt = Current.Alt;
			Temp.SidT = Current.SidT;

			Current.Ra = Ra * HrToRad;
			Current.Dec = Dec * DegToRad;
			GetAltaz();

			EncoderCount.A = Current.Alt / OneRev * AltEncoderCountsPerRev;
			EncoderCount.Z = Current.Az / OneRev * AzEncoderCountsPerRev;
			if( !AltEncoderDir)
				EncoderCount.A = AltEncoderCountsPerRev - EncoderCount.A;
			if( !AzEncoderDir)
				EncoderCount.Z = AzEncoderCountsPerRev - EncoderCount.Z;

			Current.Ra = Temp.Ra;
			Current.Dec = Temp.Dec;
			Current.Az = Temp.Az;
			Current.Alt = Temp.Alt;
			Current.SidT = Temp.SidT;
		 }

		EncoderState = Read;
	}
	else
		EncoderState = NoRead;
}

void QueryAndReadEncoders( void)
{
	QueryEncoders_f_ptr();
	delay( SerialWriteDelayMs);
	if( EncoderState == ReadReady)
		ReadEncoders_f_ptr();
}

/*
void TestEncoders( void)
{
	int X, Y;

	printf( "\n\n\nTest of encoders, press any key to quit...\n\n");
	X = wherex();
	Y = wherey();
	while( !KeyStroke)
	{
		QueryAndReadEncoders();
		if( EncoderState == Read)
		{
			gotoxy( X, Y);
			printf("Encoder counts of alt: %6ld az: %6ld", EncoderCount.A,
			EncoderCount.Z);
			delay( SerialWriteDelayMs);
		}
	}
	getch();
	NewLine;
	NewLine;
	ContMsgRoutine();
}
*/

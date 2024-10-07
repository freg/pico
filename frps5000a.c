/*******************************************************************************
 *
 * cf: Filename: ps5000aCon.c
 *
 * maquette
 * 1. acquisition 16 bits
 * 2. sortie format texte: A,<value>,B,<value>\n
 * 3. fréquence d'échantillonnage 10Mhz
 *
 ******************************************************************************/

#include <stdio.h>

#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <libps5000a/ps5000aApi.h>
#ifndef PICO_STATUS
#include <libps5000a/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define memcpy_s(a,b,c,d) memcpy(a,c,d)

typedef enum enBOOL{FALSE,TRUE} BOOL;


/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)


int32_t cycles = 0;

#define BUFFER_SIZE 	1024

#define QUAD_SCOPE		4
#define DUAL_SCOPE		2

#define MAX_PICO_DEVICES 64
#define TIMED_LOOP_STEP 500

typedef struct
{
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	float analogueOffset;
}CHANNEL_SETTINGS;

typedef enum
{
	MODEL_NONE = 0,
	MODEL_PS5242A = 0xA242,
	MODEL_PS5242B = 0xB242,
	MODEL_PS5243A = 0xA243,
	MODEL_PS5243B = 0xB243,
	MODEL_PS5244A = 0xA244,
	MODEL_PS5244B = 0xB244,
	MODEL_PS5442A = 0xA442,
	MODEL_PS5442B = 0xB442,
	MODEL_PS5443A = 0xA443,
	MODEL_PS5443B = 0xB443,
	MODEL_PS5444A = 0xA444,
	MODEL_PS5444B = 0xB444
} MODEL_TYPE;

typedef enum
{
	SIGGEN_NONE = 0,
	SIGGEN_FUNCTGEN = 1,
	SIGGEN_AWG = 2
} SIGGEN_TYPE;

typedef struct tPwq
{
	PS5000A_CONDITION * pwqConditions;
	int16_t nPwqConditions;
	PS5000A_DIRECTION * pwqDirections;
	int16_t nPwqDirections;
	uint32_t lower;
	uint32_t upper;
	PS5000A_PULSE_WIDTH_TYPE type;
}PWQ;

typedef struct
{
	int16_t handle;
	MODEL_TYPE				model;
	int8_t						modelString[8];
	int8_t						serial[10];
	int16_t						complete;
	int16_t						openStatus;
	int16_t						openProgress;
	PS5000A_RANGE			firstRange;
	PS5000A_RANGE			lastRange;
	int16_t						channelCount;
	int16_t						maxADCValue;
	SIGGEN_TYPE				sigGen;
	int16_t						hasHardwareETS;
	uint16_t					awgBufferSize;
	CHANNEL_SETTINGS	channelSettings [PS5000A_MAX_CHANNELS];
	PS5000A_DEVICE_RESOLUTION	resolution;
	int16_t						digitalPortCount;
}UNIT;

uint32_t	timebase = 8;
BOOL			scaleVoltages = TRUE;

uint16_t inputRanges [PS5000A_MAX_RANGES] = {
												10,
												20,
												50,
												100,
												200,
												500,
												1000,
												2000,
												5000,
												10000,
												20000,
												50000};

int16_t			g_autoStopped;
int16_t   	g_ready = FALSE;
uint64_t 		g_times [PS5000A_MAX_CHANNELS];
int16_t     g_timeUnit;
int32_t     g_sampleCount;
uint32_t		g_startIndex;
int16_t			g_trig = 0;
uint32_t		g_trigAt = 0;
int16_t			g_overflow = 0;

int8_t blockFile[20]  = "block.txt";
int8_t streamFile[20] = "stream.txt";

typedef struct tBufferInfo
{
	UNIT * unit;
	int16_t **driverBuffers;
	int16_t **appBuffers;

} BUFFER_INFO;


/****************************************************************************
* SetDefaults - restore default settings
****************************************************************************/
void setDefaults(UNIT * unit)
{
	PICO_STATUS status;
	PICO_STATUS powerStatus;
	int32_t i;

	status = ps5000aSetEts(unit->handle, PS5000A_ETS_OFF, 0, 0, NULL);					// Turn off hasHardwareETS
	printf(status?"setDefaults:ps5000aSetEts------ 0x%08lx \n":"", status);

	powerStatus = ps5000aCurrentPowerSource(unit->handle);

	for (i = 0; i < unit->channelCount; i++) // reset channels to most recent settings
	{
		if(i >= DUAL_SCOPE && powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
		{
			// No need to set the channels C and D if Quad channel scope and power not enabled.
		}
		else
		{
			status = ps5000aSetChannel(unit->handle, (PS5000A_CHANNEL)(PS5000A_CHANNEL_A + i),
				unit->channelSettings[PS5000A_CHANNEL_A + i].enabled,
				(PS5000A_COUPLING)unit->channelSettings[PS5000A_CHANNEL_A + i].DCcoupled,
				(PS5000A_RANGE)unit->channelSettings[PS5000A_CHANNEL_A + i].range, 
				unit->channelSettings[PS5000A_CHANNEL_A + i].analogueOffset);

			printf(status?"SetDefaults:ps5000aSetChannel------ 0x%08lx \n":"", status);

		}
	}
}

/****************************************************************************
* adc_to_mv
*
* Convert an 16-bit ADC count into millivolts
****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t rangeIndex, UNIT * unit)
{
	return (raw * inputRanges[rangeIndex]) / unit->maxADCValue;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 16-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t rangeIndex, UNIT * unit)
{
	return (mv * unit->maxADCValue) / inputRanges[rangeIndex];
}

/****************************************************************************************
* ChangePowerSource - function to handle switches between +5V supply, and USB only power
* Only applies to PicoScope 544xA/B units 
******************************************************************************************/
PICO_STATUS changePowerSource(int16_t handle, PICO_STATUS status, UNIT * unit)
{
	int8_t ch;

	switch (status)
	{
		case PICO_POWER_SUPPLY_NOT_CONNECTED:		// User must acknowledge they want to power via USB
					printf("\nPowering the unit via USB\n");
					status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_NOT_CONNECTED);		// Tell the driver that's ok
				
					if(status == PICO_OK && unit->channelCount == QUAD_SCOPE)
					{
						unit->channelSettings[PS5000A_CHANNEL_C].enabled = FALSE;
						unit->channelSettings[PS5000A_CHANNEL_D].enabled = FALSE;
					}
					else if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
					{
						status = changePowerSource(handle, status, unit);
					}
					else
					{
						// Do nothing
					}



			break;

		case PICO_POWER_SUPPLY_CONNECTED:
			printf("\nUsing +5 V power supply voltage.\n");
			status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver we are powered from +5V supply
			break;

		case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:
			
		  
		  printf("\nSwitching to use USB power from non-USB 3.0 port.\n");
		  status = ps5000aChangePowerSource(handle, PICO_USB3_0_DEVICE_NON_USB3_0_PORT);		// Tell the driver that's ok
		  
		  if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
		    {
		      status = changePowerSource(handle, status, unit);
		    }
		  else
		    {
		      // Do nothing
		    }
		  
		  break;

		case PICO_POWER_SUPPLY_UNDERVOLTAGE:
		  printf("\nUSB not supplying required voltage");
		  printf("\nPlease plug in the +5 V power supply\n");
		  status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver that's ok
		  break;
	}

	printf("\n");
	return status;
}

/****************************************************************************
* ClearDataBuffers
*
* stops GetData writing values to memory that has been released
****************************************************************************/
PICO_STATUS clearDataBuffers(UNIT * unit)
{
	int32_t i;
	PICO_STATUS status;

	for (i = 0; i < unit->channelCount; i++) 
	{
		if(unit->channelSettings[i].enabled)
		{
			if ((status = ps5000aSetDataBuffers(unit->handle, (PS5000A_CHANNEL) i, NULL, NULL, 0, 0, PS5000A_RATIO_MODE_NONE)) != PICO_OK)
			{
				printf("clearDataBuffers:ps5000aSetDataBuffers(channel %d) ------ 0x%08lx \n", i, status);
			}
		}
	}
	return status;
}


/****************************************************************************
* Initialise unit' structure with Variant specific defaults
****************************************************************************/
void set_info(UNIT * unit)
{
	int8_t description [11][25]= { "Driver Version",
		"USB Version",
		"Hardware Version",
		"Variant Info",
		"Serial",
		"Cal Date",
		"Kernel Version",
		"Digital HW Version",
		"Analogue HW Version",
		"Firmware 1",
		"Firmware 2"};

	int16_t i = 0;
	int16_t requiredSize = 0;
	int8_t line [80];
	int32_t variant;
	PICO_STATUS status = PICO_OK;

	// Variables used for arbitrary waveform parameters
	int16_t			minArbitraryWaveformValue = 0;
	int16_t			maxArbitraryWaveformValue = 0;
	uint32_t		minArbitraryWaveformSize = 0;
	uint32_t		maxArbitraryWaveformSize = 0;

	//Initialise default unit properties and change when required
	unit->sigGen = SIGGEN_FUNCTGEN;
	unit->firstRange = PS5000A_10MV;
	unit->lastRange = PS5000A_20V;
	unit->channelCount = DUAL_SCOPE;
	unit->awgBufferSize = MIN_SIG_GEN_BUFFER_SIZE;
	unit->digitalPortCount = 0;

	if (unit->handle) 
	{
		printf("Device information:-\n\n");

		for (i = 0; i < 11; i++) 
		{
			status = ps5000aGetUnitInfo(unit->handle, line, sizeof (line), &requiredSize, i);

			// info = 3 - PICO_VARIANT_INFO
			if (i == PICO_VARIANT_INFO) 
			{
				variant = atoi(line);
				memcpy(&(unit->modelString), line, sizeof(unit->modelString)==5?5:sizeof(unit->modelString));

				unit->channelCount = (int16_t)line[1];
				unit->channelCount = unit->channelCount - 48; // Subtract ASCII 0 (48)

				// Determine if the device is an MSO
				if (strstr(line, "MSO") != NULL)
				{
					unit->digitalPortCount = 2;
				}
				else
				{
					unit->digitalPortCount = 0;
				}
				
			}
			else if (i == PICO_BATCH_AND_SERIAL)	// info = 4 - PICO_BATCH_AND_SERIAL
			{
				memcpy(&(unit->serial), line, requiredSize);
			}

			printf("%s: %s\n", description[i], line);
		}

		printf("\n");

		// Set sig gen parameters
		// If device has Arbitrary Waveform Generator, find the maximum AWG buffer size
		status = ps5000aSigGenArbitraryMinMaxValues(unit->handle, &minArbitraryWaveformValue, &maxArbitraryWaveformValue, &minArbitraryWaveformSize, &maxArbitraryWaveformSize);
		unit->awgBufferSize = maxArbitraryWaveformSize;

		if (unit->awgBufferSize > 0)
		{
			unit->sigGen = SIGGEN_AWG;
		}
		else
		{
			unit->sigGen = SIGGEN_FUNCTGEN;
		}
	}
}

/****************************************************************************
* printResolution
*
* Outputs the resolution in text format to the console window
****************************************************************************/
void printResolution(PS5000A_DEVICE_RESOLUTION * resolution)
{
	switch(*resolution)
	{
		case PS5000A_DR_8BIT:

			printf("8 bits");
			break;

		case PS5000A_DR_12BIT:

			printf("12 bits");
			break;

		case PS5000A_DR_14BIT:

			printf("14 bits");
			break;

		case PS5000A_DR_15BIT:

			printf("15 bits");
			break;

		case PS5000A_DR_16BIT:

			printf("16 bits");
			break;

		default:

			break;
	}

	printf("\n");
}




/****************************************************************************
* displaySettings 
* Displays information about the user configurable settings in this example
* Parameters 
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void displaySettings(UNIT *unit)
{
	int32_t ch;
	int32_t voltage;
	PICO_STATUS status = PICO_OK;
	PS5000A_DEVICE_RESOLUTION resolution = PS5000A_DR_8BIT;

	printf("\nReadings will be scaled in %s\n", (scaleVoltages)? ("millivolts") : ("ADC counts"));
	printf("\n");

	for (ch = 0; ch < unit->channelCount; ch++)
	{
		if (!(unit->channelSettings[ch].enabled))
		{
			printf("Channel %c Voltage Range = Off\n", 'A' + ch);
		}
		else
		{
			voltage = inputRanges[unit->channelSettings[ch].range];
			printf("Channel %c Voltage Range = ", 'A' + ch);
			
			if (voltage < 1000)
			{
				printf("%dmV\n", voltage);
			}
			else
			{
				printf("%dV\n", voltage / 1000);
			}
		}
	}

	printf("\n");

	status = ps5000aGetDeviceResolution(unit->handle, &resolution);

	printf("Device Resolution: ");
	printResolution(&resolution);

}

/****************************************************************************
* openDevice 
* Parameters 
* - unit        pointer to the UNIT structure, where the handle will be stored
* - serial		pointer to the int8_t array containing serial number
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS openDevice(UNIT *unit, int8_t *serial)
{
	PICO_STATUS status;
	unit->resolution = PS5000A_DR_8BIT;

	if (serial == NULL)
	{
		status = ps5000aOpenUnit(&unit->handle, NULL, unit->resolution);
	}
	else
	{
		status = ps5000aOpenUnit(&unit->handle, serial, unit->resolution);
	}

	unit->openStatus = (int16_t) status;
	unit->complete = 1;

	return status;
}

/****************************************************************************
* handleDevice
* Parameters
* - unit        pointer to the UNIT structure, where the handle will be stored
*
* Returns
* - PICO_STATUS to indicate success, or if an error occurred
***************************************************************************/
PICO_STATUS handleDevice(UNIT * unit)
{
	int16_t value = 0;
	int32_t i;
	struct tPwq pulseWidth;
	PICO_STATUS status;

	printf("Handle: %d\n", unit->handle);
	
	if (unit->openStatus != PICO_OK)
	{
		printf("Unable to open device\n");
		printf("Error code : 0x%08x\n", (uint32_t) unit->openStatus);
		
		exit(99); // exit program
	}

	printf("Device opened successfully, cycle %d\n\n", ++cycles);
	
	// Setup device info - unless it's set already
	if (unit->model == MODEL_NONE)
	{
		set_info(unit);
	}

	// Turn off any digital ports (MSO models only)
	if (unit->digitalPortCount > 0)
	{
		printf("Turning off digital ports.");

		for (i = 0; i < unit->digitalPortCount; i++)
		{
			status = ps5000aSetDigitalPort(unit->handle, (PS5000A_CHANNEL)(i + PS5000A_DIGITAL_PORT0), 0, 0);
		}
	}
	
	timebase = 1;

	ps5000aMaximumValue(unit->handle, &value);
	unit->maxADCValue = value;

	status = ps5000aCurrentPowerSource(unit->handle);

	for (i = 0; i < unit->channelCount; i++)
	{
		// Do not enable channels C and D if power supply not connected for PicoScope 544XA/B devices
		/*if(unit->channelCount == QUAD_SCOPE && status == PICO_POWER_SUPPLY_NOT_CONNECTED && i >= DUAL_SCOPE)
		{
			unit->channelSettings[i].enabled = FALSE;
		}
		else*/
		{
			unit->channelSettings[i].enabled = TRUE;
		}

		unit->channelSettings[i].DCcoupled = TRUE;
		unit->channelSettings[i].range = PS5000A_5V;
		unit->channelSettings[i].analogueOffset = 0.0f;
	}

	memset(&pulseWidth, 0, sizeof(struct tPwq));

	setDefaults(unit);

	/* Trigger disabled	*/
	status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_A, 0, PS5000A_RISING, 0, 0);

	return unit->openStatus;
}

/****************************************************************************
* closeDevice 
****************************************************************************/
void closeDevice(UNIT *unit)
{
	ps5000aCloseUnit(unit->handle);
}

double GetTimeStamp(void)
{
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);
  return 1e9 * start.tv_sec + start.tv_nsec;        // return ns time stamp
}
void collectRawFr(UNIT *unit)
{
  int16_t status, over=0;
  int32_t no=1;
  int16_t * buffers[2 * PS5000A_MAX_CHANNELS];
  #define TBUF 6000
  #define NBPAGES 1000
  FILE * fi;
  double debut, fin, result, sec, hz ;
  fi = fopen("data.txt", "a");
  printf("Lancement de l'acquisition sur un buffer de %d et %d pages\n", TBUF, NBPAGES);
  ps5000aStop(unit->handle);
  ps5000aSetDataBuffer(unit->handle, PS5000A_CHANNEL_A, &buffers, TBUF, 1, 0);
  debut = GetTimeStamp();
  int nbval=0;
  for (int ib=0; ib<NBPAGES; ib++)
    {
      status = ps5000aGetValues(unit->handle, PS5000A_CHANNEL_A, &no, 0, 0, TBUF, &over);

      for(int i=0; i<TBUF; i++)
	{
	  nbval++;
	  fprintf(fi,"ADC_A,ADC_B");
	  for (int j = 0; j < unit->channelCount; j++)
	    {
	      fprintf(fi,",%6d",
		   adc_to_mv(buffers[j * 2][i], unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit) );
	    }
	  fprintf(fi, "\n");
	}
    }
  fclose(fi);
  fin = GetTimeStamp();

  result = fin - debut ;
  sec = result / 1e9;
  hz = sec ? nbval/sec:1;
  printf("lecture de %d valeurs deb: %f fin: %f en %f nano secondes %f s %f Hz \n",
	 nbval, debut, fin, result, sec, hz);
}
/****************************************************************************
* mainMenu
* Controls default functions of the seelected unit
* Parameters
* - unit        pointer to the UNIT structure
*
* Returns       none
***************************************************************************/
void mainMenu(UNIT *unit)
{
	int8_t ch = '.';
	PICO_STATUS status = ps5000aSetDeviceResolution(unit->handle, (PS5000A_DEVICE_RESOLUTION)PS5000A_DR_16BIT);
	displaySettings(unit);
	printf("\n\n");
	collectRawFr(unit);
}


/****************************************************************************
* main
*
***************************************************************************/
int32_t main(void)
{
	int8_t ch;
	uint16_t devCount = 0, listIter = 0,	openIter = 0;
	//device indexer -  64 chars - 64 is maximum number of picoscope devices handled by driver
	int8_t devChars[] =
			"1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz#";
	PICO_STATUS status = PICO_OK;
	UNIT allUnits[MAX_PICO_DEVICES];

	printf("PicoScope 5000 Series (ps5000a) Driver Example Program\n");
	printf("\nEnumerating Units...\n");

	do
	{
		status = openDevice(&(allUnits[devCount]), NULL);
		
		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED 
					|| status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			allUnits[devCount++].openStatus = (int16_t) status;
		}

	} while(status != PICO_NOT_FOUND);

	if (devCount == 0)
	{
		printf("Picoscope devices not found\n");
		return 1;
	}

	// if there is only one device, open and handle it here
	if (devCount == 1)
	{
		printf("Found one device, opening...\n\n");
		status = allUnits[0].openStatus;

		if (status == PICO_OK || status == PICO_POWER_SUPPLY_NOT_CONNECTED
					|| status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
		{
			if (allUnits[0].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED || allUnits[0].openStatus == PICO_USB3_0_DEVICE_NON_USB3_0_PORT)
			{
				allUnits[0].openStatus = (int16_t)changePowerSource(allUnits[0].handle, allUnits[0].openStatus, &allUnits[0]);
			}

			set_info(&allUnits[0]);
			status = handleDevice(&allUnits[0]);
		}

		if (status != PICO_OK)
		{
			printf("Picoscope devices open failed, error code 0x%x\n",(uint32_t)status);
			return 1;
		}

		mainMenu(&allUnits[0]);
		closeDevice(&allUnits[0]);
		printf("Exit...\n");
		return 0;
	}
	return 0;
}

/*******************************************************************************
 *
 * Filename: exemple.c from ps5000aCon.c
 * gcc exemple.c -g -o exe -lm -luuid -lps5000a  -Wno-format -Wl,-s  -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include
 *
 * Description:
 *   This is a console mode program that demonstrates how to use some of 
 *	 the PicoScope 5000 Series (ps5000a) driver API functions to perform operations
 *	 using a PicoScope 5000 Series Flexible Resolution Oscilloscope.
 *
 *	Supported PicoScope models:
 *
 *		PicoScope 5242A/B/D & 5442A/B/D
 *		PicoScope 5243A/B/D & 5443A/B/D
 *		PicoScope 5244A/B/D & 5444A/B/D
 *
 *  Linux platforms:
 *
 *		instructions from https://www.picotech.com/downloads/linux
 * Copyright (C) 2013-2018 Pico Technology Ltd. See LICENSE file for terms.
 *
 ******************************************************************************/

#include <stdio.h>
//#include <uuid/uuid.h>
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <ctype.h>
#include <libps5000a/ps5000aApi.h>
#ifndef PICO_STATUS
#include <libps5000a/PicoStatus.h>
#endif

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
typedef enum enBOOL{FALSE,TRUE} BOOL;
double GetTimeStamp(void)
{
  struct timespec start;
  clock_gettime(CLOCK_REALTIME, &start);
  return 1e9 * start.tv_sec + start.tv_nsec;        // return ns time stamp
}
/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
  struct termios oldt, newt;
  int32_t ch;
  int32_t bytesWaiting;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setbuf(stdin, NULL);
  do {
    ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
    if (bytesWaiting)
      getchar();
  } while (bytesWaiting);

  ch = getchar();

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return ch;
}

int32_t _kbhit()
{
  struct termios oldt, newt;
  int32_t bytesWaiting;
  tcgetattr(STDIN_FILENO, &oldt);
  newt = oldt;
  newt.c_lflag &= ~( ICANON | ECHO );
  tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  setbuf(stdin, NULL);
  ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

  tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  return bytesWaiting;
}

int32_t fopen_s(FILE ** a, const int8_t * b, const int8_t * c)
{
  FILE * fp = fopen(b,c);
  *a = fp;
  return (fp>0)?0:-1;
}

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

uint32_t	timebase = 8, timeInterval = 0;
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
int8_t streamFile[100] = "stream.txt";
#define BaseSFile "stream"

typedef struct tBufferInfo
{
  UNIT * unit;
  int16_t **driverBuffers;
  int16_t **appBuffers;

} BUFFER_INFO;

/****************************************************************************
 * callbackStreaming
 * Used by ps5000a data streaming collection calls, on receipt of data.
 * Used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 callBackStreaming(	int16_t handle,
				int32_t noOfSamples,
				uint32_t startIndex,
				int16_t overflow,
				uint32_t triggerAt,
				int16_t triggered,
				int16_t autoStop,
				void	*pParameter)
{
  int32_t channel;
  BUFFER_INFO * bufferInfo = NULL;

  if (pParameter != NULL)
    {
      bufferInfo = (BUFFER_INFO *) pParameter;
    }

  // used for streaming
  g_sampleCount = noOfSamples;
  g_startIndex  = startIndex;
  g_autoStopped = autoStop;

  // flag to say done reading data
  g_ready = TRUE;

  // flags to show if & where a trigger has occurred
  g_trig = triggered;
  g_trigAt = triggerAt;

  g_overflow = overflow;

  if (bufferInfo != NULL && noOfSamples)
    {
      for (channel = 0; channel < bufferInfo->unit->channelCount; channel++)
	{
	  if (bufferInfo->unit->channelSettings[channel].enabled)
	    {
	      if (bufferInfo->appBuffers && bufferInfo->driverBuffers)
		{
		  // Max buffers
		  if (bufferInfo->appBuffers[channel * 2]  && bufferInfo->driverBuffers[channel * 2])
		    {
		      memcpy_s (&bufferInfo->appBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t),
				&bufferInfo->driverBuffers[channel * 2][startIndex], noOfSamples * sizeof(int16_t));
		    }

		  // Min buffers
		  if (bufferInfo->appBuffers[channel * 2 + 1] && bufferInfo->driverBuffers[channel * 2 + 1])
		    {
		      memcpy_s (&bufferInfo->appBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t),
				&bufferInfo->driverBuffers[channel * 2 + 1][startIndex], noOfSamples * sizeof(int16_t));
		    }
		}
	    }
	}
    }
}

/****************************************************************************
 * Callback
 * used by ps5000a data block collection calls, on receipt of data.
 * used to set global flags etc checked by user routines
 ****************************************************************************/
void PREF4 callBackBlock( int16_t handle, PICO_STATUS status, void * pParameter)
{
  if (status != PICO_CANCELLED)
    {
      g_ready = TRUE;
    }
}

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
      do
	{
	  printf("\n5 V power supply not connected.");
	  printf("\nDo you want to run using USB only Y/N?\n");
	  ch = toupper(_getch());
				
	  if(ch == 'Y')
	    {
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

	    }
	}
      while(ch != 'Y' && ch != 'N');
      printf(ch == 'N'?"Please use the +5V power supply to power this unit\n":"");
      break;

    case PICO_POWER_SUPPLY_CONNECTED:
      printf("\nUsing +5 V power supply voltage.\n");
      status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver we are powered from +5V supply
      break;

    case PICO_USB3_0_DEVICE_NON_USB3_0_PORT:
      do
	{
	  printf("\nUSB 3.0 device on non-USB 3.0 port.");
	  printf("\nDo you wish to continue Y/N?\n");
	  ch = toupper(_getch());

	  if (ch == 'Y')
	    {
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

	    }
	} while (ch != 'Y' && ch != 'N');
      printf(ch == 'N' ? "Please use a USB 3.0 port or press 'Y'.\n" : "");
      break;

    case PICO_POWER_SUPPLY_UNDERVOLTAGE:
      do
	{
	  printf("\nUSB not supplying required voltage");
	  printf("\nPlease plug in the +5 V power supply\n");
	  printf("\nHit any key to continue, or Esc to exit...\n");
	  ch = _getch();
				
	  if (ch == 0x1B)	// ESC key
	    {
	      exit(0);
	    }
	  else
	    {
	      status = ps5000aChangePowerSource(handle, PICO_POWER_SUPPLY_CONNECTED);		// Tell the driver that's ok
	    }
	}
      while (status == PICO_POWER_SUPPLY_REQUEST_INVALID);
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
 * streamDataHandler
 * - Used by the two stream data examples - untriggered and triggered
 * Inputs:
 * - unit - the unit to sample on
 * - preTrigger - the number of samples in the pre-trigger phase 
 *					(0 if no trigger has been set)
 ***************************************************************************/
void streamDataHandler(UNIT * unit, uint32_t preTrigger)
{
  int32_t i, j;
  uint32_t sampleCount = 50000; /* make sure overview buffer is large enough */
  FILE * fp = NULL;
  int16_t * buffers[2 * PS5000A_MAX_CHANNELS];
  int16_t * appBuffers[2 * PS5000A_MAX_CHANNELS];
  PICO_STATUS status;
  PICO_STATUS powerStatus;
  uint32_t sampleInterval;
  int32_t index = 0;
  int32_t totalSamples = 0, id = 0; // compteurs
  uint32_t postTrigger;
  int16_t autostop;
  uint32_t downsampleRatio;
  uint32_t triggeredAt = 0;
  PS5000A_TIME_UNITS timeUnits;
  PS5000A_RATIO_MODE ratioMode;
  int16_t retry = 0;
  int16_t powerChange = 0;
  uint32_t numStreamingValues = 0;
  uint cpt_ts = 0;
  double startts, endts;
  BUFFER_INFO bufferInfo;

  powerStatus = ps5000aCurrentPowerSource(unit->handle);
	
  for (i = 0; i < unit->channelCount; i++) 
    {
      if (i >= DUAL_SCOPE && unit->channelCount == QUAD_SCOPE && powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
	  // No need to set the channels C and D if Quad channel scope and power supply not connected.
	}
      else
	{
	  if (unit->channelSettings[i].enabled)
	    {
	      buffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	      buffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
			
	      status = ps5000aSetDataBuffers(unit->handle, (PS5000A_CHANNEL)i, buffers[i * 2], buffers[i * 2 + 1], sampleCount, 0, PS5000A_RATIO_MODE_NONE);

	      appBuffers[i * 2] = (int16_t*) calloc(sampleCount, sizeof(int16_t));
	      appBuffers[i * 2 + 1] = (int16_t*) calloc(sampleCount, sizeof(int16_t));

	      printf(status?"StreamDataHandler:ps5000aSetDataBuffers(channel %ld) ------ 0x%08lx \n":"", i, status);
	    }
	}
    }
	
  downsampleRatio = 1;
  //timeUnits = PS5000A_US;
  timeUnits = PS5000A_NS;
  sampleInterval = 100;
  ratioMode = PS5000A_RATIO_MODE_NONE;
  preTrigger = 0;
  postTrigger = 1000000;
  autostop = FALSE;
	
  bufferInfo.unit = unit;	
  bufferInfo.driverBuffers = buffers;
  bufferInfo.appBuffers = appBuffers;
  printf("\nStreaming Data continually.\n\n");
  g_autoStopped = FALSE;
  do
    {
      retry = 0;

      status = ps5000aRunStreaming(unit->handle, &sampleInterval, timeUnits, preTrigger, postTrigger, autostop, 
				   downsampleRatio, ratioMode, sampleCount);

      if (status != PICO_OK)
	{
	  // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
	  // PicoScope 524XD devices on non-USB 3.0 port
	  if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
	      status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE)		
	    {
	      status = changePowerSource(unit->handle, status, unit);
	      retry = 1;
	    }
	  else
	    {
	      printf("streamDataHandler:ps5000aRunStreaming ------ 0x%08lx \n", status);
	      printf("sampleInterval: %d\n", sampleInterval);
	      return;
	    }
	}
    }
  while (retry);
  startts = GetTimeStamp();
  printf("Streaming data...Press a key to stop\n");
    
  sprintf(streamFile, "%s-%f.txt", BaseSFile, GetTimeStamp());
  fopen_s(&fp, streamFile, "w");

  if (fp != NULL)
    {
      //uuid_t binuuid;
      //uuid_generate_random(binuuid);
      //char *uuid = malloc(38);
      //uuid_unparse(binuuid, uuid);
      //uuid[37] = 0;
      //printf("%s\n", uuid);
      fprintf(fp,"Streaming Data Log       <ts positions>                                                           \n\n");
      //fprintf(fp,"UUID:%s\n", uuid);
      fprintf(fp,"For each of the %d Channels, results shown are....\n",unit->channelCount);
      fprintf(fp,"ADC raw, conversion in mv: raw * range(A:%d,B:%d) / maxADCValue(%d)\n\n", unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit->channelSettings[PS5000A_CHANNEL_B + j].range, unit->maxADCValue);
      for (j = 0; j < unit->channelCount; j++) 
	fprintf(fp, "Channel %c range: %d ADC_Max:%d\n", (char)'A'+j,
		unit->channelSettings[PS5000A_CHANNEL_A + j].range, unit->maxADCValue);
      fprintf(fp, "Timebase used %d = %d ns sample interval %d\n", timebase, timeInterval, sampleInterval);
      fprintf(fp, "Resolution used: 14Bits\n");
      fprintf(fp, "\n");
      fprintf(fp, "ADC_A,ADC_B\n");
    }
	
  printf("entête complète lancement de l'acquisition\n");
  totalSamples = 0;

  while (!_kbhit() && !g_autoStopped)
    {
      /* Poll until data is received. Until then, GetStreamingLatestValues wont call the callback */
      g_ready = FALSE;

      status = ps5000aGetStreamingLatestValues(unit->handle, callBackStreaming, &bufferInfo);

      // PicoScope 5X4XA/B/D devices...+5 V PSU connected or removed or
      // PicoScope 524XD devices on non-USB 3.0 port
      if (status == PICO_POWER_SUPPLY_CONNECTED || status == PICO_POWER_SUPPLY_NOT_CONNECTED ||
	  status == PICO_USB3_0_DEVICE_NON_USB3_0_PORT || status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
	{
	  if (status == PICO_POWER_SUPPLY_UNDERVOLTAGE)
	    {
	      changePowerSource(unit->handle, status, unit);
	    }

	  printf("\n\nPower Source Change");
	  powerChange = 1;
	}

      if (g_ready && g_sampleCount > 0) /* Can be ready and have no data, if autoStop has fired */
	{
	  if (g_trig)
	    {
	      triggeredAt = totalSamples + g_trigAt;		// Calculate where the trigger occurred in the total samples collected
	    }

	  totalSamples += g_sampleCount;			
	  if (g_trig)
	    {
	      printf("Trig. at index %lu total %lu", g_trigAt, triggeredAt + 1);	// show where trigger occurred
				
	    }
			
	  for (i = g_startIndex; i < (int32_t)(g_startIndex + g_sampleCount); i++) 
	    {
				
	      if (fp != NULL)
		{
		  for (j = 0; j < unit->channelCount; j++) 
		    {
		      if (unit->channelSettings[j].enabled) 
			{
			  if (j)
			    fprintf(fp, ",%d", appBuffers[j * 2][i]);
			  else
			    fprintf(fp, "%d", appBuffers[j * 2][i]);
			}
		    }
		  fprintf(fp,"\n");
		}
	      else
		{
		  printf("Cannot open the file %s for writing.\n", streamFile);
		}
				
	    }
	}
    }

  printf("\n\n");
  ps5000aStop(unit->handle);
  endts = GetTimeStamp();
  int seek = fseek(fp, 23, SEEK_SET);
  printf("fseek %d \n", seek);
  fprintf(fp, " ts %f->%f ", startts, endts);
  fseek(fp, 0, SEEK_END);
  
  if (fp != NULL)
    {

      fclose (fp);
    }

  printf("\nData collection aborted\n");
  	
  for (i = 0; i < unit->channelCount; i++) 
    {
      if(unit->channelSettings[i].enabled)
	{
	  free(buffers[i * 2]);
	  free(appBuffers[i * 2]);

	  free(buffers[i * 2 + 1]);
	  free(appBuffers[i * 2 + 1]);
	}
    }

  clearDataBuffers(unit);
}


/****************************************************************************
 * setTrigger
 *
 * - Used to call all the functions required to set up triggering.
 *
 ***************************************************************************/
PICO_STATUS setTrigger(UNIT * unit,
		       PS5000A_TRIGGER_CHANNEL_PROPERTIES_V2 * channelProperties,
		       int16_t nChannelProperties,
		       PS5000A_CONDITION * triggerConditions,
		       int16_t nTriggerConditions,
		       PS5000A_DIRECTION * directions,
		       uint16_t nDirections,
		       struct tPwq * pwq,
		       uint32_t delay,
		       uint64_t autoTriggerUs)
{
  PICO_STATUS status;
  PS5000A_CONDITIONS_INFO info = PS5000A_CLEAR;
  PS5000A_CONDITIONS_INFO pwqInfo = PS5000A_CLEAR;

  int16_t auxOutputEnabled = 0; // Not used by function call

  status = ps5000aSetTriggerChannelPropertiesV2(unit->handle, channelProperties, nChannelProperties, auxOutputEnabled);

  if (status != PICO_OK) 
    {
      printf("setTrigger:ps5000aSetTriggerChannelPropertiesV2 ------ Ox%08lx \n", status);
      return status;
    }

  if (nTriggerConditions != 0)
    {
      info = (PS5000A_CONDITIONS_INFO)(PS5000A_CLEAR | PS5000A_ADD); // Clear and add trigger condition specified unless no trigger conditions have been specified
    }

  status = ps5000aSetTriggerChannelConditionsV2(unit->handle, triggerConditions, nTriggerConditions, info);

  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetTriggerChannelConditionsV2 ------ 0x%08lx \n", status);
      return status;
    }

  status = ps5000aSetTriggerChannelDirectionsV2(unit->handle, directions, nDirections);

  if (status != PICO_OK) 
    {
      printf("setTrigger:ps5000aSetTriggerChannelDirectionsV2 ------ 0x%08lx \n", status);
      return status;
    }

  status = ps5000aSetAutoTriggerMicroSeconds(unit->handle, autoTriggerUs);

  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetAutoTriggerMicroSeconds ------ 0x%08lx \n", status);
      return status;
    }

  status = ps5000aSetTriggerDelay(unit->handle, delay);
	
  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetTriggerDelay ------ 0x%08lx \n", status);
      return status;
    }

  // Clear and add pulse width qualifier condition, clear if no pulse width qualifier has been specified
  if (pwq->nPwqConditions != 0)
    {
      pwqInfo = (PS5000A_CONDITIONS_INFO)(PS5000A_CLEAR | PS5000A_ADD);
    }

  status = ps5000aSetPulseWidthQualifierConditions(unit->handle, pwq->pwqConditions, pwq->nPwqConditions, pwqInfo);

  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetPulseWidthQualifierConditions ------ 0x%08lx \n", status);
      return status;
    }

  status = ps5000aSetPulseWidthQualifierDirections(unit->handle, pwq->pwqDirections, pwq->nPwqDirections);

  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetPulseWidthQualifierDirections ------ 0x%08lx \n", status);
      return status;
    }

  status = ps5000aSetPulseWidthQualifierProperties(unit->handle, pwq->lower, pwq->upper, pwq->type);

  if (status != PICO_OK)
    {
      printf("setTrigger:ps5000aSetPulseWidthQualifierProperties ------ Ox%08lx \n", status);
      return status;
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
 * Select input voltage ranges for channels
 ****************************************************************************/
void setVoltages(UNIT * unit)
{
  PICO_STATUS powerStatus = PICO_OK;
  PICO_STATUS status = PICO_OK;
  PS5000A_DEVICE_RESOLUTION resolution = PS5000A_DR_8BIT;

  int32_t i, ch;
  int32_t count = 0;
  int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
  int16_t numEnabledChannels = 0;
  int16_t retry = FALSE;

  if (unit->channelCount == QUAD_SCOPE)
    {
      powerStatus = ps5000aCurrentPowerSource(unit->handle); 

      if (powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
	  numValidChannels = DUAL_SCOPE;
	}
    }

  // See what ranges are available... 
  for (i = unit->firstRange; i <= unit->lastRange; i++) 
    {
      printf("%d -> %d mV\n", i, inputRanges[i]);
    }

  do
    {
      count = 0;

      do
	{
	  // Ask the user to select a range
	  printf("Specify voltage range (%d..%d)\n", unit->firstRange, unit->lastRange);
	  printf("99 - switches channel off\n");
		
	  for (ch = 0; ch < numValidChannels; ch++) 
	    {
	      printf("\n");

	      do 
		{
		  printf("Channel %c: ", 'A' + ch);
		  fflush(stdin);
		  scanf_s("%hd", &(unit->channelSettings[ch].range));
			
		} while (unit->channelSettings[ch].range != 99 && (unit->channelSettings[ch].range < unit->firstRange || unit->channelSettings[ch].range > unit->lastRange));

	      if (unit->channelSettings[ch].range != 99) 
		{
		  printf(" - %d mV\n", inputRanges[unit->channelSettings[ch].range]);
		  unit->channelSettings[ch].enabled = TRUE;
		  count++;
		} 
	      else 
		{
		  printf("Channel Switched off\n");
		  unit->channelSettings[ch].enabled = FALSE;
		  unit->channelSettings[ch].range = PS5000A_MAX_RANGES-1;
		}
	    }
	  printf(count == 0? "\n** At least 1 channel must be enabled **\n\n":"");
	}
      while (count == 0);	// must have at least one channel enabled

      status = ps5000aGetDeviceResolution(unit->handle, &resolution);

      // Verify that the number of enabled channels is valid for the resolution set.

      switch (resolution)
	{
	case PS5000A_DR_15BIT:

	  if (count > 2)
	    {
	      printf("\nError: Only 2 channels may be enabled with 15-bit resolution set.\n");
	      printf("Please switch off %d channel(s).\n", numValidChannels - 2);
	      retry = TRUE;
	    }
	  else
	    {
	      retry = FALSE;
	    }
	  break;

	case PS5000A_DR_16BIT:

	  if (count > 1)
	    {
	      printf("\nError: Only one channes may be enabled with 16-bit resolution set.\n");
	      printf("Please switch off %d channel(s).\n", numValidChannels - 1);
	      retry = TRUE;
	    }
	  else
	    {
	      retry = FALSE;
	    }
				
	  break;

	default:

	  retry = FALSE;
	  break;
	}

      printf("\n");
    }
  while (retry == TRUE);

  setDefaults(unit);	// Put these changes into effect
}

/****************************************************************************
 * setTimebase
 * Select timebase, set time units as nano seconds
 * 
 ****************************************************************************/
void setTimebase(UNIT * unit, int entree)
{
  PICO_STATUS status = PICO_OK;
  PICO_STATUS powerStatus = PICO_OK;
  //int32_t timeInterval;
  int32_t maxSamples;
  int32_t ch;

  uint32_t shortestTimebase;
  double timeIntervalSeconds;
	
  PS5000A_CHANNEL_FLAGS enabledChannelOrPortFlags = (PS5000A_CHANNEL_FLAGS)0;
	
  int16_t numValidChannels = unit->channelCount; // Dependent on power setting - i.e. channel A & B if USB powered on 4-channel device
	
  if (unit->channelCount == QUAD_SCOPE)
    {
      powerStatus = ps5000aCurrentPowerSource(unit->handle);
		
      if (powerStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
	{
	  numValidChannels = DUAL_SCOPE;
	}
    }
	
  // Find the analogue channels that are enabled - if an MSO model is being used, this will need to be
  // modified to add channel flags for enabled digital ports
  for (ch = 0; ch < numValidChannels; ch++)
    {
      if (unit->channelSettings[ch].enabled)
	{
	  enabledChannelOrPortFlags = enabledChannelOrPortFlags | (PS5000A_CHANNEL_FLAGS)(1 << ch);
	}
    }
	
  // Find the shortest possible timebase and inform the user.
  status = ps5000aGetMinimumTimebaseStateless(unit->handle, enabledChannelOrPortFlags, &shortestTimebase, &timeIntervalSeconds, unit->resolution);
  
  if (status != PICO_OK)
    {
      printf("setTimebase:ps5000aGetMinimumTimebaseStateless ------ 0x%08lx \n", status);
      return;
    }


  if (entree==0)
    {
      printf("Shortest timebase index available %d (%.9f seconds).\n", shortestTimebase, timeIntervalSeconds);
      printf("Specify desired timebase: ");
      fflush(stdin);
      scanf_s("%lud", &timebase);
    }
  else
    {
      timebase = entree;
    }
  do 
    {
      status = ps5000aGetTimebase(unit->handle, timebase, BUFFER_SIZE, &timeInterval, &maxSamples, 0);

      if (status == PICO_INVALID_NUMBER_CHANNELS_FOR_RESOLUTION)
	{
	  printf("SetTimebase: Error - Invalid number of channels for resolution.\n");
	  return;
	}
      else if (status == PICO_OK)
	{
	  // Do nothing
	}
      else
	{
	  timebase++; // Increase timebase if the one specified can't be used. 
	}

    }
  while (status != PICO_OK);
	
  printf("Timebase used %lu = %ld ns sample interval\n", timebase, timeInterval);
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
 * setResolution
 * Set resolution for the device
 *
 ****************************************************************************/
void setResolution(UNIT * unit, PS5000A_DEVICE_RESOLUTION newResolution)
{
  int16_t value;
  int16_t i;
  int16_t numEnabledChannels = 0;
  int16_t retry;
  int32_t resolutionInput;

  PICO_STATUS status;
  PS5000A_DEVICE_RESOLUTION resolution;
	

  // Determine number of channels enabled
  for(i = 0; i < unit->channelCount; i++)
    {
      if(unit->channelSettings[i].enabled == TRUE)
	{
	  numEnabledChannels++;
	}
    }

  if(numEnabledChannels == 0)
    {
      printf("setResolution: Please enable channels.\n");
      return;
    }

  status = ps5000aGetDeviceResolution(unit->handle, &resolution);

  if(status == PICO_OK)
    {
      printf("Current resolution: ");
      printResolution(&resolution);
    }
  else
    {
      printf("setResolution:ps5000aGetDeviceResolution ------ 0x%08lx \n", status);
      return;
    }

  printf("\n");

  // Verify if resolution can be selected for number of channels enabled

  if (newResolution == PS5000A_DR_16BIT && numEnabledChannels > 1)
    {
      printf("setResolution: 16 bit resolution can only be selected with 1 channel enabled.\n");
    }
  else if (newResolution == PS5000A_DR_15BIT && numEnabledChannels > 2)
    {
      printf("setResolution: 15 bit resolution can only be selected with a maximum of 2 channels enabled.\n");
    }
  else if(newResolution < PS5000A_DR_8BIT && newResolution > PS5000A_DR_16BIT)
    {
      printf("setResolution: Resolution index selected out of bounds.\n");
    }
  else
    {
      retry = FALSE;
    }

	
  printf("\n");

  status = ps5000aSetDeviceResolution(unit->handle, (PS5000A_DEVICE_RESOLUTION) newResolution);

  if (status == PICO_OK)
    {
      unit->resolution = newResolution;

      printf("Resolution selected: ");
      printResolution(&newResolution);
		
      // The maximum ADC value will change if transitioning from 8 bit to >= 12 bit or vice-versa
      ps5000aMaximumValue(unit->handle, &value);
      unit->maxADCValue = value;
    }
  else
    {
      printf("setResolution:ps5000aSetDeviceResolution ------ 0x%08lx \n", status);
    }

}


/****************************************************************************
 * collectStreamingImmediate
 *  This function demonstrates how to collect a stream of data
 *  from the unit (start collecting immediately)
 ***************************************************************************/
void collectStreamingImmediate(UNIT * unit)
{
  PICO_STATUS status = PICO_OK;

  setDefaults(unit);

  printf("Collect streaming...\n");
  printf("Data is written to disk file (stream.txt)\n");

  /* Trigger disabled	*/
  status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_A, 0, PS5000A_RISING, 0, 0);
  status = ps5000aSetSimpleTrigger(unit->handle, 0, PS5000A_CHANNEL_B, 0, PS5000A_RISING, 0, 0);

  streamDataHandler(unit, 0);
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
      while(!_kbhit());
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
      if(unit->channelCount == QUAD_SCOPE && status == PICO_POWER_SUPPLY_NOT_CONNECTED && i >= DUAL_SCOPE)
	{
	  unit->channelSettings[i].enabled = FALSE;
	}
      else
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


  setResolution(unit, PS5000A_DR_14BIT);
  for (int i=3; i<30; i++)
    setTimebase(unit, i);
  setTimebase(unit, 0);
  while (ch != 'X')
    {
      displaySettings(unit);

      printf("\n\n");
      printf("Please select operation:\n\n");

      printf("S - Immediate streaming\n");

      printf("                                              X - Exit\n");
      printf("Operation:");

      ch = toupper(_getch());

      printf("\n\n");

      switch (ch) 
	{

	case 'S':
	  collectStreamingImmediate(unit);
	  break;

	case 'X':
	  break;

	default:
	  printf("Invalid operation\n");
	  break;
	}
    }
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
    
  printf("PicoScope 5000 Series (ps5000a) from Driver Example Program\n");
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
  else
    {
      // More than one unit
      printf("Found %d devices, initializing...\n\n", devCount);

      for (listIter = 0; listIter < devCount; listIter++)
	{
	  if (allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED)
	    {
	      set_info(&allUnits[listIter]);
	      openIter++;
	    }
	}
    }
	
  // None
  if (openIter == 0)
    {
      printf("Picoscope devices init failed\n");
      return 1;
    }
  // Just one - handle it here
  if (openIter == 1)
    {
      for (listIter = 0; listIter < devCount; listIter++)
	{
	  if (!(allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED))
	    {
	      break;
	    }
	}
		
      printf("One device opened successfully\n");
      printf("Model\t: %s\nS/N\t: %s\n", allUnits[listIter].modelString, allUnits[listIter].serial);
      status = handleDevice(&allUnits[listIter]);
		
      if (status != PICO_OK)
	{
	  printf("Picoscope device open failed, error code 0x%x\n", (uint32_t)status);
	  return 1;
	}
		
      mainMenu(&allUnits[listIter]);
      closeDevice(&allUnits[listIter]);
      printf("Exit...\n");
      return 0;
    }
  printf("Found %d devices, pick one to open from the list:\n", devCount);

  for (listIter = 0; listIter < devCount; listIter++)
    {
      printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
	     allUnits[listIter].modelString, allUnits[listIter].serial);
    }

  printf("ESC) Cancel\n");

  ch = '.';
	
  // If escape
  while (ch != 27)
    {
      ch = _getch();
		
      // If escape
      if (ch == 27)
	continue;
      for (listIter = 0; listIter < devCount; listIter++)
	{
	  if (ch == devChars[listIter])
	    {
	      printf("Option %c) selected, opening Picoscope %7s S/N: %s\n",
		     devChars[listIter], allUnits[listIter].modelString,
		     allUnits[listIter].serial);
				
	      if ((allUnits[listIter].openStatus == PICO_OK || allUnits[listIter].openStatus == PICO_POWER_SUPPLY_NOT_CONNECTED))
		{
		  status = handleDevice(&allUnits[listIter]);
		}
				
	      if (status != PICO_OK)
		{
		  printf("Picoscope devices open failed, error code 0x%x\n", (uint32_t)status);
		  return 1;
		}

	      mainMenu(&allUnits[listIter]);

	      printf("Found %d devices, pick one to open from the list:\n",devCount);
				
	      for (listIter = 0; listIter < devCount; listIter++)
		{
		  printf("%c) Picoscope %7s S/N: %s\n", devChars[listIter],
			 allUnits[listIter].modelString,
			 allUnits[listIter].serial);
		}
				
	      printf("ESC) Cancel\n");
	    }
	}
    }

  for (listIter = 0; listIter < devCount; listIter++)
    {
      closeDevice(&allUnits[listIter]);
    }

  printf("Exit...\n");
	
  return 0;
}

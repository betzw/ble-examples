/* mbed Microcontroller Library
 * Copyright (c) 2006-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>

#include "mbed-drivers/mbed.h"
#include "ble/BLE.h"

#define DBG_MCU
#ifdef DBG_MCU
/* betzw: enable debugging while using sleep modes */
#include "x-nucleo-common/DbgMCU.h"
static DbgMCU enable_dbg;
#endif // DBG_MCU

#define URI_BUFFER_SIZE (0x1000)  // in characters
#define URI_BUFFER_TH   (0x80)    // in characters
#define NR_URI_BUFFERS  (0x4)     // Number of URI buffers
static char uri_buffer[NR_URI_BUFFERS][URI_BUFFER_SIZE];
static unsigned int uri_buffer_pos[NR_URI_BUFFERS] = { 0 };
static unsigned int nr_filling_buf = 0;  // number of buffer to be filled
static unsigned int nr_printing_buf = 0; // number of buffer to be printed

static const int URI_MAX_LENGTH = 18;             // Maximum size of service data in ADV packets

#define PRINT_URIS_DELAY (5000)  // in milli-secons
static minar::callback_handle_t delayedPrintHandle = NULL; // handle of delayed callback

DigitalOut led1(LED1, 1);

// returns nr's old value
static inline unsigned int inc_buf_nr(unsigned int &nr) {
  unsigned int ret = nr;

  nr +=1;
  nr %= NR_URI_BUFFERS;

  return ret;
}
// Forward declaration
static void periodicPrintURIs(void);

#define NR_OF_CHARS_TO_PRINT (0x20)
static void periodicPrintCallback(void) {
  static unsigned int curr_buf_pos = 0; // current character position

  char tmp_buffer[NR_OF_CHARS_TO_PRINT];
  unsigned int curr_buf_len = strlen(uri_buffer[nr_printing_buf]);
  int ret = snprintf(tmp_buffer, NR_OF_CHARS_TO_PRINT, "%s", &uri_buffer[nr_printing_buf][curr_buf_pos]);

  printf("%s", tmp_buffer);

  if(ret > NR_OF_CHARS_TO_PRINT)
    curr_buf_pos += (NR_OF_CHARS_TO_PRINT-1);
  else
    curr_buf_pos += ret;

  if(curr_buf_pos >= curr_buf_len) { // we have printed the whole buffer
    // printf("\n\r"); // betzw - NOTE: should be commented!?!
    curr_buf_pos = 0;
    inc_buf_nr(nr_printing_buf);
    delayedPrintHandle = minar::Scheduler::postCallback(periodicPrintURIs).delay(minar::milliseconds(PRINT_URIS_DELAY)).getHandle();
  } else { // we need to continue printing
    minar::Scheduler::postCallback(periodicPrintCallback);
  }
}

static void periodicPrintURIs(void)
{
  delayedPrintHandle = NULL;
  uri_buffer_pos[inc_buf_nr(nr_filling_buf)] = 0;
  if(nr_filling_buf == nr_printing_buf) {
    error("==> FATAL ERROR: nr_filling_buf(%u) == nr_printing_buf(%u)!!! <==\n\r", nr_filling_buf, nr_printing_buf);
  }
  minar::Scheduler::postCallback(periodicPrintCallback);
}

static void periodicBlinkyCallback(void)
{
  led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */
}

static void decodeURI(const uint8_t* uriData, const size_t uriLen, const int8_t rssi)
{
  char tmp_buffer[URI_MAX_LENGTH];
  unsigned int tmp_buffer_pos = 0;

  const char *prefixes[] = {
    "http://www.",
    "https://www.",
    "http://",
    "https://",
    "urn:uuid:"
  };
  const size_t NUM_PREFIXES = sizeof(prefixes) / sizeof(char *);
  const char *suffixes[] = {
    ".com/",
    ".org/",
    ".edu/",
    ".net/",
    ".info/",
    ".biz/",
    ".gov/",
    ".com",
    ".org",
    ".edu",
    ".net",
    ".info",
    ".biz",
    ".gov"
  };
  const size_t NUM_SUFFIXES = sizeof(suffixes) / sizeof(char *);

  size_t index = 0;

  /* betzw: first check for buffer space */
  if((uri_buffer_pos[nr_filling_buf] + URI_BUFFER_TH) >= URI_BUFFER_SIZE) { // betzw: flush buffer
    if(delayedPrintHandle != NULL) {
      minar::Scheduler::cancelCallback(delayedPrintHandle);
      delayedPrintHandle = NULL;
    }
    periodicPrintURIs();
  }

  /* First byte is the URL Scheme. */
  if (uriData[index] < NUM_PREFIXES) {
    sprintf(&tmp_buffer[tmp_buffer_pos], "%s", prefixes[uriData[index]]);
    tmp_buffer_pos = strlen(tmp_buffer);
    index++;
  } else {
    printf("URL Scheme was not encoded!\n\r");
    return;
  }

  /* From second byte onwards we can have a character or a suffix */
  while(index < uriLen) {
    if (uriData[index] < NUM_SUFFIXES) {
      sprintf(&tmp_buffer[tmp_buffer_pos], "%s", suffixes[uriData[index]]);
      tmp_buffer_pos = strlen(tmp_buffer);
    } else {
      sprintf(&tmp_buffer[tmp_buffer_pos], "%c", uriData[index]);
      tmp_buffer_pos = strlen(tmp_buffer);
    }
    index++;
  }

  sprintf(&tmp_buffer[tmp_buffer_pos], " : %d\n\r", rssi);
  tmp_buffer_pos = strlen(tmp_buffer);

  if((uri_buffer_pos[nr_filling_buf] > 0) && (strstr(uri_buffer[nr_filling_buf], tmp_buffer) != NULL)) 
    return; // In case the URI is already in the list just return

  /* Copy tmp buffer to uri buffer */
  sprintf(&uri_buffer[nr_filling_buf][uri_buffer_pos[nr_filling_buf]], "%s", tmp_buffer);
  uri_buffer_pos[nr_filling_buf] = strlen(uri_buffer[nr_filling_buf]);
}

/*
 * This function is called every time we scan an advertisement.
 */
static void advertisementCallback(const Gap::AdvertisementCallbackParams_t *params)
{
  struct AdvertisingData_t {
    uint8_t                        length; /* doesn't include itself */
    GapAdvertisingData::DataType_t dataType;
    uint8_t                        data[0];
  } AdvDataPacket;

  struct ApplicationData_t {
    uint8_t applicationSpecificId[2];
    uint8_t frameType;
    uint8_t advPowerLevels;
    uint8_t uriData[URI_MAX_LENGTH];
  } AppDataPacket;

  const uint8_t BEACON_UUID[sizeof(UUID::ShortUUIDBytes_t)] = {0xAA, 0xFE};
  const uint8_t FRAME_TYPE_URL                              = 0x10;
  const uint8_t APPLICATION_DATA_OFFSET                     = sizeof(ApplicationData_t) + sizeof(AdvDataPacket.dataType) - sizeof(AppDataPacket.uriData);

  AdvertisingData_t *pAdvData;
  size_t index = 0;
  while(index < params->advertisingDataLen) {
    pAdvData = (AdvertisingData_t *)&params->advertisingData[index];
    if (pAdvData->dataType == GapAdvertisingData::SERVICE_DATA) {
      ApplicationData_t *pAppData = (ApplicationData_t *) pAdvData->data;
      if (!memcmp(pAppData->applicationSpecificId, BEACON_UUID, sizeof(BEACON_UUID)) && (pAppData->frameType == FRAME_TYPE_URL)) {
	decodeURI(pAppData->uriData, pAdvData->length - APPLICATION_DATA_OFFSET, params->rssi);
	break;
      }
    }
    index += (pAdvData->length + 1);
  }
}

void app_start(int, char *[])
{
  minar::Scheduler::postCallback(periodicBlinkyCallback).period(minar::milliseconds(500));
  delayedPrintHandle = minar::Scheduler::postCallback(periodicPrintURIs).delay(minar::milliseconds(PRINT_URIS_DELAY)).getHandle();

  BLE &ble = BLE::Instance();
  ble.init();
  ble.gap().setScanParams(1800 /* scan interval */, 1500 /* scan window */);
  ble.gap().startScan(advertisementCallback);
}

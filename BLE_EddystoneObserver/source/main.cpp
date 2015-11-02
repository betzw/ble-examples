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

#define URI_BUFFER_SIZE 0x1000  // in characters
#define URI_BUFFER_TH   128     // in characters
static char uri_buffer[URI_BUFFER_SIZE];
static unsigned int uri_buffer_pos = 0;

static const int URI_MAX_LENGTH = 18;             // Maximum size of service data in ADV packets

DigitalOut led1(LED1, 1);

static inline void printURIs(void)
{
	printf("%s\n\r", uri_buffer);
	uri_buffer_pos = 0;
}

static void periodicCallback(void)
{
    static unsigned int cnt = 0;

    led1 = !led1; /* Do blinky on LED1 while we're waiting for BLE events */

    if((++cnt >= 10) && (uri_buffer_pos > 0)) { // 5 secs
	    printURIs();
	    cnt = 0;
    }
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
    if((uri_buffer_pos + URI_BUFFER_TH) >= URI_BUFFER_SIZE) { // betzw: flush buffer
	    printURIs();
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

    if((uri_buffer_pos > 0) && (strstr(uri_buffer, tmp_buffer) != NULL)) 
	    return; // In case the URI is already in the list just return

    sprintf(&tmp_buffer[tmp_buffer_pos], " : %d\n\r", rssi);
    tmp_buffer_pos = strlen(tmp_buffer);

    /* Copy tmp buffer to uri buffer */
    sprintf(&uri_buffer[uri_buffer_pos], "%s", tmp_buffer);
    uri_buffer_pos = strlen(uri_buffer);
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
    minar::Scheduler::postCallback(periodicCallback).period(minar::milliseconds(500));

    BLE &ble = BLE::Instance();
    ble.init();
    ble.gap().setScanParams(1800 /* scan interval */, 1500 /* scan window */);
    ble.gap().startScan(advertisementCallback);
}

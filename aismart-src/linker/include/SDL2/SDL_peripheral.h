/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _SDL_peripheral_h
#define _SDL_peripheral_h

/**
 *  \file SDL_third.h
 *
 *  Header for the SDL third party management routines.
 */

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_video.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#define SDL_BLE_MAC_ADDR_BYTES	6

typedef enum {
	SDL_BleCharacteristicPropertyBroadcast = 0x01,
	SDL_BleCharacteristicPropertyRead = 0x02,
	SDL_BleCharacteristicPropertyWriteWithoutResponse = 0x04,
	SDL_BleCharacteristicPropertyWrite = 0x08,
	SDL_BleCharacteristicPropertyNotify = 0x10,
	SDL_BleCharacteristicPropertyIndicate = 0x20,
	SDL_BleCharacteristicPropertyAuthenticatedSignedWrites = 0x40,
	SDL_BleCharacteristicPropertyExtendedProperties = 0x80,
	SDL_BleCharacteristicPropertyNotifyEncryptionRequired = 0x100,
	SDL_BleCharacteristicPropertyIndicateEncryptionRequired = 0x200,
} SDL_BleCharacteristicProperties;

struct _SDL_BleService;

typedef struct {
	char* uuid;
	struct _SDL_BleService* service;
	uint32_t properties;
	void* cookie;
} SDL_BleCharacteristic;

typedef struct _SDL_BleService {
	char* uuid;
	SDL_BleCharacteristic* characteristics;
	int valid_characteristics;
	void* cookie;
} SDL_BleService;

typedef struct {
	char* name;
	char* uuid;
	uint8_t* manufacturer_data;
	int manufacturer_data_len;
	int rssi;
    uint8_t mac_addr[SDL_BLE_MAC_ADDR_BYTES];
	SDL_BleService* services;
	int valid_services;
   
    unsigned int receive_advertisement;
	void* cookie;
} SDL_BlePeripheral;

typedef struct {
    void (*discover_peripheral)(SDL_BlePeripheral* peripheral);
    void (*release_peripheral)(SDL_BlePeripheral* peripheral);
    void (*connect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
    void (*disconnect_peripheral)(SDL_BlePeripheral* peripheral, const int error);
	void (*discover_services)(SDL_BlePeripheral* peripheral, const int error);
	void (*discover_characteristics)(SDL_BlePeripheral* peripheral, SDL_BleService* service, const int error);
    void (*read_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const uint8_t* data, int len);
    void (*write_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
    void (*notify_characteristic)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
	void (*discover_descriptors)(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error);
} SDL_BleCallbacks;

extern DECLSPEC SDL_BleService* SDL_BleFindService(SDL_BlePeripheral* peripheral, const char* uuid);
extern DECLSPEC SDL_BleCharacteristic* SDL_BleFindCharacteristic(SDL_BlePeripheral* peripheral, const char* service_uuid, const char* uuid);

extern DECLSPEC void SDL_BleSetCallbacks(SDL_BleCallbacks* callbacks);
extern DECLSPEC void SDL_BleScanPeripherals(const char* uuid);
extern DECLSPEC void SDL_BleStopScanPeripherals(void);
extern DECLSPEC void SDL_BleReleasePeripheral(SDL_BlePeripheral* peripheral);
extern DECLSPEC void SDL_BleStartAdvertise(void);
extern DECLSPEC void SDL_BleConnectPeripheral(SDL_BlePeripheral* peripheral);
extern DECLSPEC void SDL_BleDisconnectPeripheral(SDL_BlePeripheral* peripheral);
extern DECLSPEC void SDL_BleGetServices(SDL_BlePeripheral* peripheral);
extern DECLSPEC void SDL_BleGetCharacteristics(SDL_BlePeripheral* peripheral, SDL_BleService* service);
extern DECLSPEC void SDL_BleReadCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic);
extern DECLSPEC void SDL_BleWriteCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const uint8_t* data, int size);
extern DECLSPEC void SDL_BleNotifyCharacteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, SDL_bool enable);
extern DECLSPEC void SDL_BleDiscoverDescriptors(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic);
extern DECLSPEC int SDL_BleAuthorizationStatus(void);
extern DECLSPEC SDL_bool SDL_BleUuidEqual(const char* uuid1, const char* uuid2);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* _SDL_timer_h */

/* vi: set ts=4 sw=4 expandtab: */

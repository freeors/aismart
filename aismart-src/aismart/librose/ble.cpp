/* $Id: title_screen.cpp 48740 2011-03-05 10:01:34Z mordante $ */
/*
   Copyright (C) 2008 - 2011 by Mark de Wever <koraq@xs4all.nl>
   Part of the Battle for Wesnoth Project http://www.wesnoth.org/

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY.

   See the COPYING file for more details.
*/

#define GETTEXT_DOMAIN "rose-lib"

#include "ble.hpp"

#include <time.h>
#include "gettext.hpp"
#include "wml_exception.hpp"
#include "sdl_mixer.h"

#include <iomanip>
#include <boost/bind.hpp>

#include <algorithm>

#include "base_instance.hpp"

static tble* singleten = NULL;

static void blec_discover_peripheral(SDL_BlePeripheral* peripheral)
{
	singleten->did_discover_peripheral(*peripheral);
}

static void blec_release_peripheral(SDL_BlePeripheral* peripheral)
{
	singleten->did_release_peripheral(*peripheral);
}

static void blec_connect_peripheral(SDL_BlePeripheral* peripheral, const int error)
{
	singleten->did_connect_peripheral(*peripheral, error);
}

static void blec_disconnect_peripheral(SDL_BlePeripheral* peripheral, const int error)
{
	singleten->did_disconnect_peripheral(*peripheral, error);
}
    
static void blec_discover_services(SDL_BlePeripheral* peripheral, const int error)
{
	singleten->did_discover_services(*peripheral, error);
}

static void blec_discover_characteristics(SDL_BlePeripheral* peripheral, SDL_BleService* service, const int error)
{
	singleten->did_discover_characteristics(*peripheral, *service, error);
}

static void blec_read_characteristic(SDL_BlePeripheral* device, SDL_BleCharacteristic* characteristic, const uint8_t* data, int len)
{
	singleten->did_read_characteristic(*device, *characteristic, data, len);
}

static void blec_write_characteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error)
{
	singleten->did_write_characteristic(*peripheral, *characteristic, error);
}
    
static void blec_notify_characteristic(SDL_BlePeripheral* peripheral, SDL_BleCharacteristic* characteristic, const int error)
{
    singleten->did_notify_characteristic(*peripheral, *characteristic, error);
}

bool tble::tdisable_reconnect_lock::require_reconnect = true;

tble::tstep::tstep(const std::string& service, const std::string& characteristic, int _option, const uint8_t* _data, const size_t _len)
	: service_uuid(service)
	, characteristic_uuid(characteristic)
	, option(_option)
	, data(NULL)
	, len(_len)
{

	if (option == option_write) {
		data = (uint8_t*)malloc(len);
		memcpy(data, _data, len);
	}
}

void tble::tstep::execute(tble& ble, SDL_BlePeripheral& peripheral) const
{
    SDL_Log("tstep::execute, task: %s, current_step_: %i\n", ble.current_task_->id().c_str(), ble.current_step_);
    
	ble.app_task_callback(*ble.current_task_, ble.current_step_, true);

	SDL_BleCharacteristic* characteristic = SDL_BleFindCharacteristic(&peripheral, service_uuid.c_str(), characteristic_uuid.c_str());
	if (option == option_write) {
		SDL_BleWriteCharacteristic(&peripheral, characteristic, data, len);
    } else if (option == option_notify) {
        SDL_BleNotifyCharacteristic(&peripheral, characteristic, SDL_TRUE);
    }
}

void tble::ttask::insert(int at, const std::string& service, const std::string& characteristic, int option, const uint8_t* data, const int len)
{
	VALIDATE(at == nposm, null_str);

	steps_.push_back(tstep(service, characteristic, option, data, len));
}

void tble::ttask::execute(tble& ble)
{
	VALIDATE(ble.peripheral_, null_str);

	SDL_BlePeripheral& peripheral = *ble.peripheral_;
	ble.current_task_ = this;
	ble.current_step_ = 0;

	const tstep& step = steps_.front();
	if (ble.make_characteristic_exist(step.service_uuid, step.characteristic_uuid)) {
		step.execute(ble, peripheral);
	}
}

const unsigned char tble::null_mac_addr[SDL_BLE_MAC_ADDR_BYTES] = {0};
std::string tble::peripheral_name(const SDL_BlePeripheral& peripheral, bool adjust)
{
	if (peripheral.name && peripheral.name[0] != '\0') {
		return peripheral.name;
	}
	if (!adjust) {
		return null_str;
	}
	const std::string null_name = "<null>";
	return null_name;
}

std::string tble::peripheral_name2(const std::string& name)
{
	if (!name.empty()) {
		return name;
	}
	const std::string null_name = "<null>";
	return null_name;
}

std::string tble::get_full_uuid(const std::string& uuid)
{
	const int full_size = 36; // 32 + 4

	int size = uuid.size();
	if (size == full_size) {
		return uuid;
	}
	if (size != 4) {
		return null_str;
	}
	return std::string("0000") + uuid + "-0000-1000-8000-00805f9b34fb";
}

tble* tble::singleton_ = NULL;

tble::tble(tconnector& connector)
	: connecting_peripheral_(NULL)
	, peripheral_(NULL)
	, connector_(&connector)
	, persist_scan_(false)
	, disable_reconnect_(false)
    , discover_connect_(true)
	, current_task_(NULL)
	, disconnect_threshold_(60000)
	, disconnect_music_("error.ogg")
	, background_scan_ticks_(0)
	, background_id_(INVALID_UINT32_ID)
{
	singleton_ = this;

	memset(&callbacks_, 0, sizeof(SDL_BleCallbacks));

	callbacks_.discover_peripheral = blec_discover_peripheral;
	callbacks_.release_peripheral = blec_release_peripheral;
	callbacks_.connect_peripheral = blec_connect_peripheral;
    callbacks_.disconnect_peripheral = blec_disconnect_peripheral;
    callbacks_.discover_services = blec_discover_services;
    callbacks_.discover_characteristics = blec_discover_characteristics;
	callbacks_.read_characteristic = blec_read_characteristic;
	callbacks_.write_characteristic = blec_write_characteristic;
	callbacks_.notify_characteristic = blec_notify_characteristic;
	SDL_BleSetCallbacks(&callbacks_);

	singleten = this;
}

tble::~tble()
{
	SDL_BleSetCallbacks(NULL);
	singleten = NULL;
}

tble::ttask& tble::insert_task(const std::string& id)
{
	for (std::vector<ttask>::const_iterator it = tasks_.begin(); it != tasks_.end(); ++ it) {
		const ttask& task = *it;
		VALIDATE(task.id() != id, null_str);
	}

	tasks_.push_back(ttask(id));
	return tasks_.back();
}

void tble::clear_background_connect_flag()
{
	background_scan_ticks_ = 0;
	background_id_ = INVALID_UINT32_ID;
}

// true: start reconnect detect when background.
// false: stop detect.
void tble::background_reconnect_detect(bool start)
{
	if (start) {
        SDL_Log("background_reconnect_detect, start\n");
		background_scan_ticks_ = SDL_GetTicks() + disconnect_threshold_;
        VALIDATE(background_id_ == INVALID_UINT32_ID, null_str);
        background_id_ = instance->background_connect(boost::bind(&tble::background_callback, this, _1));

	} else {
        SDL_Log("background_reconnect_detect, stop, background_scan_ticks_: %u\n", background_scan_ticks_);
		clear_background_connect_flag();
		if (peripheral_ && !SDL_strcasecmp(sound::current_music_file().c_str(), disconnect_music_.c_str())) {
            SDL_Log("background_reconnect_detect, will stop music\n");
			sound::stop_music();
		}
	}
}

bool tble::background_callback(uint32_t ticks)
{
	// set bachournd_id_ and background_scan_ticks_ is in other thread, to avoid race, use tow judge together.
	if (background_id_ == INVALID_UINT32_ID && background_scan_ticks_ == 0) {
		// connect peripherah returned, has stop detect.
		return true;

	} else if (background_scan_ticks_ && ticks >= background_scan_ticks_) {
		if (!Mix_PlayingMusic()) {
			sound::play_music_repeatedly(disconnect_music_);
		}
        VALIDATE(background_id_ != INVALID_UINT32_ID, null_str);
        clear_background_connect_flag();
        return true;
	}
	return false;
}

// true: foreground-->background
// false: background-->foreground
void tble::handle_app_event(bool background)
{
	if (!background) {
		clear_background_connect_flag();
	}

	// if don't in connecting or connected, start_scan again.
	if (!connecting_peripheral_ && !peripheral_) {
		start_scan();
	}
}

void tble::start_scan()
{
    std::vector<std::string> v;
	const char* uuid = NULL;
	if (!instance->foreground() && mac_addr_.valid()) {
		// Service UUID of advertisement packet maybe serval UUID, Now I only pass first.
		v = utils::split(mac_addr_.uuid.c_str());
		if (!v.empty()) {
			uuid = v.front().c_str();
		}
		if (!connecting_peripheral_ && !peripheral_) {
            // even if in reconnect detect, app maybe call start_scan again.
            if (background_id_ == INVALID_UINT32_ID) {
                background_reconnect_detect(true);
            }
		}
	}

    SDL_Log("------tble::start_scan------uuid: %s\n", uuid? uuid: "<nil>");
	SDL_BleScanPeripherals(uuid);
}

void tble::stop_scan()
{
    SDL_Log("------tble::stop_scan------\n");
	SDL_BleStopScanPeripherals();
}

void tble::start_advertise()
{
	SDL_BleStartAdvertise();
}

void tble::disconnect_peripheral()
{
    if (peripheral_) {
		SDL_BleDisconnectPeripheral(peripheral_);

    } else if (connecting_peripheral_) {
        connecting_peripheral_ = NULL;
    }
}

void tble::connect_peripheral(SDL_BlePeripheral& peripheral)
{
	VALIDATE(!connecting_peripheral_ && !peripheral_, null_str);

	// some windows os, connect is synchronous with did. set connecting_peripheral_ before connect.
	connecting_peripheral_ = &peripheral;
	SDL_BleConnectPeripheral(&peripheral);
	connector_->evaluate(peripheral);
}

void tble::did_discover_peripheral(SDL_BlePeripheral& peripheral)
{
#if (defined(__APPLE__) && TARGET_OS_IPHONE)
    // iOS cannot get macaddr normally. Call it to use private method to get it.
    app_calculate_mac_addr(peripheral);
#endif
    
	if (!connecting_peripheral_ && !peripheral_ && connector_->match(peripheral)) {
		// tble think there are tow connect requirement.
		// one is app call connect_peripheral manuly. 
		// the other is at here. reconnect automaticly when discover.
		connect_peripheral(peripheral);
	}
	app_discover_peripheral(peripheral);
}

void tble::did_release_peripheral(SDL_BlePeripheral& peripheral)
{
	// 1. !peripheral_. now no connected peripheral.
	// 2. peripheral_ != &peripheral. there is connected peripheral, not myself.
	// peripheral_ == &peripheral. must not, can not occur!
	VALIDATE(!peripheral_ || peripheral_ != &peripheral, null_str);

	SDL_Log("tble::did_release_peripheral, name: %s\n", peripheral.name);

    bool connecting = connecting_peripheral_ == &peripheral;
    if (connecting) {
		// Connect the ble peripheral required for a period of time, user release peripheral during this period. 
		// it is different from did_disconnect_peripheral. if disconnect one existed connection, did_disconnect_peripheral should be called before.
        connecting_peripheral_ = NULL;
    }
	app_release_peripheral(peripheral);
}

void tble::did_connect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
	SDL_Log("tble::did_connect_peripheral, name: %s, connecting_peripheral_: %p, error: %i, persist_scan_: %s\n", peripheral.name, connecting_peripheral_, error, persist_scan_? "true": "false");

	if (connecting_peripheral_) {
		if (!error) {
			connecting_peripheral_ = NULL;
			peripheral_ = &peripheral;
			if (!persist_scan_) {
				stop_scan();
			}
		}
		app_connect_peripheral(peripheral, error);

	} else if (!error) {
		// app call connect_peripheral, and call disconnect_peripheral at once.
		// step1.
		VALIDATE(!peripheral_, null_str);
		SDL_BleDisconnectPeripheral(&peripheral);
	}

	background_reconnect_detect(false);
}

void tble::did_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error)
{
	SDL_Log("tble::did_disconnect_peripheral, name: %s\n", peripheral.name);

	if (peripheral_) {
		VALIDATE(peripheral_ == &peripheral, null_str);
		peripheral_ = NULL;
		app_disconnect_peripheral(peripheral, error);
	} else if (!error) {
		// app call connect_peripheral, and call disconnect_peripheral at once.
		// step2.
		VALIDATE(!connecting_peripheral_, null_str);
	}

	// restart scan, persist_scan_ whether or not.
	if (!disable_reconnect_) {
		SDL_Log("tble::did_disconnect_peripheral, start_scan\n");
		start_scan();
	}
}

void tble::did_discover_services(SDL_BlePeripheral& peripheral, const int error)
{
	VALIDATE(peripheral.valid_services >= 1, null_str);

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (make_characteristic_exist(step.service_uuid, step.characteristic_uuid)) {
			step.execute(*this, peripheral);
		}
		return;
	}
	app_discover_services(peripheral, error);
}

void tble::did_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error)
{
	VALIDATE(service.valid_characteristics >= 1, null_str);

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		VALIDATE(make_characteristic_exist(step.service_uuid, step.characteristic_uuid), null_str);
		step.execute(*this, peripheral);
		return;
	}
	app_discover_characteristics(peripheral, service, error);
}

void tble::did_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len)
{
	SDL_Log("tble::did_read_characteristic(%s), current_task: %s, current_step_: %i, len: %i\n", characteristic.uuid, current_task_? current_task_->id().c_str(): "null", current_step_, len);
	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, step.characteristic_uuid.c_str()) && step.option == option_read) {
			SDL_Log("data: ");
			for (int i = 0; i < len; i ++) {
				SDL_Log(" 0x%2x", data[i]);
			}
			SDL_Log("\n");

			task_next_step(peripheral);
			return;
		}
	}
	app_read_characteristic(peripheral, characteristic, data, len);
}

void tble::did_write_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error)
{
    SDL_Log("tble::did_write_characteristic(%s), current_task: %s, current_step_: %i\n", characteristic.uuid, current_task_? current_task_->id().c_str(): "null", current_step_);
    
	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, current_task_->get_step(current_step_).characteristic_uuid.c_str()) && step.option == option_write) {
			task_next_step(peripheral);
			return;
		}
	}
	app_write_characteristic(peripheral, characteristic, error);
}

void tble::did_notify_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error)
{
	SDL_Log("tble::did_notify_characteristic(%s), current_task: %s, current_step_: %i\n", characteristic.uuid, current_task_? current_task_->id().c_str(): "null", current_step_);

	if (current_task_) {
		const tstep& step = current_task_->get_step(current_step_);
		if (SDL_BleUuidEqual(characteristic.uuid, current_task_->get_step(current_step_).characteristic_uuid.c_str())  && step.option == option_notify) {
			task_next_step(peripheral);
			return;
		}
	}
	app_notify_characteristic(peripheral, characteristic, error);
}

tble::ttask& tble::get_task(const std::string& id)
{
	for (std::vector<ttask>::iterator it = tasks_.begin(); it != tasks_.end(); ++ it) {
		ttask& task = *it;
		if (task.id() == id) {
			return task;
		}
	}

	VALIDATE(false, null_str);
	return tasks_.front();
}

bool tble::make_characteristic_exist(const std::string& service_uuid, const std::string& characteristic_uuid)
{
	VALIDATE(peripheral_, null_str);

	SDL_BlePeripheral* peripheral = peripheral_;
	if (!peripheral->valid_services) {
		SDL_BleGetServices(peripheral);
		return false;
	} else {
		SDL_BleService* service = SDL_BleFindService(peripheral, service_uuid.c_str());
        
        std::stringstream err;
        err << "can not file service: " << service_uuid;
		if (!service) {
			SDL_Log("make_characteristic_exist, %s\n", err.str().c_str());
		}
        VALIDATE(service, err.str());
		if (!service->valid_characteristics) {
			SDL_BleGetCharacteristics(peripheral, service);
			return false;
		}
	}
	return true;
}

void tble::task_next_step(SDL_BlePeripheral& peripheral)
{
    SDL_Log("tble::task_next_step, current_task: %s, finished_step_: %i\n", current_task_->id().c_str(), current_step_);
    
	if (!app_task_callback(*current_task_, current_step_, false) || ++ current_step_ == current_task_->steps().size()) {
		current_task_ = NULL;
		return;
	}
    
    const tstep& step = current_task_->get_step(current_step_);
    if (make_characteristic_exist(step.service_uuid, step.characteristic_uuid)) {
        current_task_->get_step(current_step_).execute(*this, peripheral);
    }
}

std::string mac_addr_uc6_2_str(const unsigned char* uc6)
{
	std::stringstream ss;
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
        ss << std::setfill('0') << std::setw(2) << std::setbase(16) << (int)(uc6[i]);
    }
	return utils::uppercase(ss.str());
}

std::string mac_addr_uc6_2_str2(const unsigned char* uc6)
{
	std::stringstream ss;
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		if (!ss.str().empty()) {
			ss << ":";
		}
        ss << std::setfill('0') << std::setw(2) << std::setbase(16) << (int)(uc6[i]);
    }
	return utils::uppercase(ss.str());
}

void mac_addr_str_2_uc6(const char* str, uint8_t* uc6, const char separator)
{
	char value_str[3];
	int i, at;
	int should_len = SDL_BLE_MAC_ADDR_BYTES * 2 + (separator? SDL_BLE_MAC_ADDR_BYTES - 1: 0);
	if (!str || SDL_strlen(str) != should_len) {
		SDL_memset(uc6, 0, SDL_BLE_MAC_ADDR_BYTES);
		return;
	}

	value_str[2] = '\0';
	for (i = 0, at = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		value_str[0] = str[at ++];
		value_str[1] = str[at ++];
		uc6[i] = SDL_strtol(value_str, NULL, 16);
		if (separator && i != SDL_BLE_MAC_ADDR_BYTES - 1) {
			if (str[at] != separator) {
				SDL_memset(uc6, 0, SDL_BLE_MAC_ADDR_BYTES);
				return;
			}
			at ++;
		}
	}
}

bool mac_addr_valid(const unsigned char* uc6)
{
	for (int i = 0; i < SDL_BLE_MAC_ADDR_BYTES; i ++) {
		if (uc6[i]) {
			return true;
		}
	}
	return false;
}
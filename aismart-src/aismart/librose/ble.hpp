#ifndef LIBROSE_BLE_HPP_INCLUDED
#define LIBROSE_BLE_HPP_INCLUDED

#include <set>
#include <vector>
#include <map>
#include <string>
#include "sdl_peripheral.h"
#include "wml_exception.hpp"
#include "serialization/string_utils.hpp"
#include "sound.hpp"

std::string mac_addr_uc6_2_str(const unsigned char* uc6);
std::string mac_addr_uc6_2_str2(const unsigned char* uc6);
void mac_addr_str_2_uc6(const char* str, uint8_t* uc6, const char separator);
bool mac_addr_valid(const unsigned char* uc6);
#define mac_addr_equal(addr1, addr2)	(!memcmp(addr1, addr2, sizeof(unsigned char[SDL_BLE_MAC_ADDR_BYTES])))

struct tmac_addr 
{
	// :, maybe used in mac_addr string.
	static const char connect_char = '&';
	tmac_addr()
		: uuid()
	{
		memset(mac_addr, 0, SDL_BLE_MAC_ADDR_BYTES);
	}

	tmac_addr(const std::string& pstr)
		: uuid()
	{
		set(pstr);
	}

	tmac_addr(const uint8_t* _mac_addr, const std::string& uuid)
		: uuid(uuid) 
	{
		memcpy(mac_addr, _mac_addr, SDL_BLE_MAC_ADDR_BYTES);
	}

	tmac_addr(const tmac_addr& that)
		: uuid(that.uuid)
	{
		memcpy(mac_addr, that.mac_addr, SDL_BLE_MAC_ADDR_BYTES);
	}

	tmac_addr& operator=(const tmac_addr& that)
	{
		memcpy(mac_addr, that.mac_addr, SDL_BLE_MAC_ADDR_BYTES);
		uuid = that.uuid;
		return *this;
	}

	bool operator<(const tmac_addr& that) const
	{
		for (int at = 0; at < SDL_BLE_MAC_ADDR_BYTES; at ++) {
			if (mac_addr[at] < that.mac_addr[at]) {
				return true;
			} else if (mac_addr[at] > that.mac_addr[at]) {
				return false;
			}
		}
		return false;
	}

	bool operator==(const tmac_addr& that) const
	{
		return mac_addr_equal(mac_addr, that.mac_addr);
	}

	bool operator!=(const tmac_addr& that) const
	{
		return !operator==(that);
	}

	bool valid() const { return mac_addr_valid(mac_addr); }
	void set(const uint8_t* _mac_addr, const std::string& _uuid) 
	{ 
		memcpy(mac_addr, _mac_addr, SDL_BLE_MAC_ADDR_BYTES); 
		uuid = _uuid;
	}

	// mac_addr: 00:12:34:56:78:9a, service uuid: 180a,2a0c ===> str: 00123456789A&180A,2A0C
	void set(const std::string& pstr) 
	{ 
		std::vector<std::string> vstr = utils::split(pstr, connect_char);
		if (vstr.size() >= 1 && vstr.front().size() == SDL_BLE_MAC_ADDR_BYTES * 2) {
			mac_addr_str_2_uc6(vstr.front().c_str(), mac_addr, '\0');
		} else {
			memset(mac_addr, 0, SDL_BLE_MAC_ADDR_BYTES);
		}
		if (vstr.size() >= 2) {
			uuid = vstr[1];
		} else {
			uuid.clear();
		}
	}
	void clear()
	{
		memset(mac_addr, 0, SDL_BLE_MAC_ADDR_BYTES);
		uuid.clear();
	}
	std::string str() const { return mac_addr_uc6_2_str(mac_addr); }
	std::string pstr() const 
	{
		std::stringstream ss;
		ss << mac_addr_uc6_2_str(mac_addr);
		if (!uuid.empty()) {
			ss << connect_char << uuid;
		}
		return ss.str(); 
	}

	uint8_t mac_addr[SDL_BLE_MAC_ADDR_BYTES];
	std::string uuid;
};

class tble
{
public:
	class tpersist_scan_lock
	{
	public:
		tpersist_scan_lock(tble& ble)
			: ble_(ble)
			, original_(ble.persist_scan_)
		{
			VALIDATE(!ble_.disable_reconnect_, "Must not scan when disable reconnecting");
			ble_.persist_scan_ = true;
            // in one scan, cannot discover one peripheral twice. even if it is disconnect--connect.
            // should force rescan every time.
			SDL_Log("tpersist_scan_lock::tpersist_scan_lock, start_scan\n");
            ble_.start_scan();
		}

		~tpersist_scan_lock()
		{
			ble_.persist_scan_ = original_;
			if (!ble_.persist_scan_ && ble_.peripheral_) {
				ble_.stop_scan();
                
			} else if (!ble_.peripheral_ && !ble_.disable_reconnect_) {
                // in one scan, cannot discover one peripheral twice. even if it is disconnect--connect.
                // force rescan, in order to find connected ever and now disconnected peripheral.
				SDL_Log("tpersist_scan_lock::~tpersist_scan_lock, start_scan\n");
                ble_.start_scan();
            }
		}

	private:
		tble& ble_;
		bool original_;
	};

	class tdisable_reconnect_lock
	{
	public:
		tdisable_reconnect_lock(tble& ble, bool _require_reconnect)
			: ble_(ble)
			, original_(ble.disable_reconnect_)
			, original_require_reconnect_(require_reconnect)
		{
			VALIDATE(!ble_.persist_scan_, "Must not disable reconnect when persist scaning!");
			require_reconnect &= _require_reconnect;
			if (original_) {
				// first lock.
			}
			ble_.disable_reconnect_ = true;
		}

		~tdisable_reconnect_lock()
		{
			ble_.disable_reconnect_ = original_;
			if (!ble_.disable_reconnect_ && require_reconnect) {
				SDL_Log("tdisable_reconnect_lock::~tdisable_reconnect_lock, start_scan\n");
				ble_.start_scan();
			}
			require_reconnect = original_require_reconnect_;
			VALIDATE(!original_ || require_reconnect, NULL);
		}

	private:
		static bool require_reconnect;

		tble& ble_;
		bool original_;
		bool original_require_reconnect_;
	};

	enum { option_read, option_write, option_notify};

	struct tstep
	{
		tstep(const std::string& service, const std::string& characteristic, int _option, const uint8_t* _data, const size_t _len);

		tstep(const tstep& that)
			: service_uuid(that.service_uuid)
			, characteristic_uuid(that.characteristic_uuid)
			, option(that.option)
            , data(NULL)
			, len(that.len)
		{
			if (len) {
				data = (uint8_t*)malloc(len);
				memcpy(data, that.data, len);
			}
		}

		~tstep()
		{
			if (data) {
				free(data);
			}
		}

        void set_data(uint8_t* _data, size_t _len)
        {
            if (data) {
                free(data);
                data = NULL;
            }
            len = _len;
            if (len) {
                data = (uint8_t*)malloc(len);
                memcpy(data, _data, len);
            }
        }
        
        void execute(tble& ble, SDL_BlePeripheral& peripheral) const;

		std::string service_uuid;
		std::string characteristic_uuid;
		int option;
		uint8_t* data;
		size_t len;
	};

	class ttask
	{
	public:
		ttask(const std::string& id)
			: id_(id)
		{}

		void insert(int at, const std::string& service, const std::string& characteristic, int option, const uint8_t* data = NULL, const int len = 0);
        const tstep& get_step(int at) const { return steps_[at]; }
        tstep& get_step(int at) { return steps_[at]; }
		const std::vector<tstep>& steps() const { return steps_; }
        
		void execute(tble& ble);

		const std::string& id() const { return id_; }
		bool valid() const { return !id_.empty() && !steps_.empty(); }

	private:
		std::string id_;
		std::vector<tstep> steps_;
	};

	class tconnector
	{
	public:
		tconnector()
			: name()
			, uuid()
			, manufacturer_data(NULL)
			, manufacturer_data_len(0)
		{
            memset(mac_addr, 0, SDL_BLE_MAC_ADDR_BYTES);
        }

		tconnector(SDL_BlePeripheral& peripheral)
			: name(peripheral_name(peripheral, false))
			, uuid(utils::cstr_2_str(peripheral.uuid))
			, manufacturer_data(NULL)
			, manufacturer_data_len(peripheral.manufacturer_data_len)
		{
			memcpy(mac_addr, peripheral.mac_addr, SDL_BLE_MAC_ADDR_BYTES);
			if (manufacturer_data_len) {
				manufacturer_data = (uint8_t*)malloc(manufacturer_data_len);
				memcpy(manufacturer_data, peripheral.manufacturer_data, manufacturer_data_len);
			}
		}

		virtual ~tconnector()
		{
			clear();
		}

		tconnector(const tconnector& that)
			: name(that.name)
			, uuid(that.uuid)
			, manufacturer_data(NULL)
			, manufacturer_data_len(that.manufacturer_data_len)
		{
			memcpy(mac_addr, that.mac_addr, SDL_BLE_MAC_ADDR_BYTES);
			if (manufacturer_data_len) {
				manufacturer_data = (uint8_t*)malloc(manufacturer_data_len);
				memcpy(manufacturer_data, that.manufacturer_data, manufacturer_data_len);
			}
		}

		void evaluate(const SDL_BlePeripheral& peripheral)
		{
			clear();
			name = peripheral_name(peripheral, false);
			uuid = utils::cstr_2_str(peripheral.uuid);
			manufacturer_data_len = peripheral.manufacturer_data_len;
			memcpy(mac_addr, peripheral.mac_addr, SDL_BLE_MAC_ADDR_BYTES);
			if (manufacturer_data_len) {
				manufacturer_data = (uint8_t*)malloc(manufacturer_data_len);
				memcpy(manufacturer_data, peripheral.manufacturer_data, manufacturer_data_len);
			}
		}

		// don't implete operator=, make app don't call it.

		virtual bool match(SDL_BlePeripheral& peripheral) const 
		{
            if (!valid()) {
                return false;
            }
			if (name != utils::cstr_2_str(peripheral.name)) {
				return false;
			}
			if (uuid != utils::cstr_2_str(peripheral.uuid)) {
				return false;
			}
			if (!mac_addr_equal(mac_addr, peripheral.mac_addr)) {
				return false;
			}
			if (manufacturer_data_len != peripheral.manufacturer_data_len) {
				return false;
			}
			if (memcmp(manufacturer_data, peripheral.manufacturer_data, manufacturer_data_len)) {
				return false;
			}
			return true;
		}

        bool valid() const { return !name.empty() || mac_addr[0] != '\0' || manufacturer_data; }
        
		virtual void clear()
		{
			name.clear();
			uuid.clear();
			mac_addr[0] = '\0';
			if (manufacturer_data) {
				free(manufacturer_data);
				manufacturer_data = NULL;
				manufacturer_data_len = 0;
			}
		}


		std::string name;
		std::string uuid;
		uint8_t* manufacturer_data;
		int manufacturer_data_len;
		uint8_t mac_addr[SDL_BLE_MAC_ADDR_BYTES];
	};

	static const unsigned char null_mac_addr[SDL_BLE_MAC_ADDR_BYTES];
	static std::string peripheral_name(const SDL_BlePeripheral& peripheral, bool adjust);
	static std::string peripheral_name2(const std::string& name);
	static std::string get_full_uuid(const std::string& uuid);

	tble(tconnector& connector);
	virtual ~tble();

	static tble* get_singleton() { return singleton_; }

	void did_discover_peripheral(SDL_BlePeripheral& peripheral);
	void did_release_peripheral(SDL_BlePeripheral& peripheral);
	void did_connect_peripheral(SDL_BlePeripheral& peripheral, const int error);
	void did_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error);
    void did_discover_services(SDL_BlePeripheral& peripheral, const int error);
	void did_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error);
	void did_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len);
	void did_write_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error);
	void did_notify_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error);

	void start_scan();
	void stop_scan();
	void start_advertise();
	bool persist_scan() const { return persist_scan_; }

	void connect_peripheral(SDL_BlePeripheral& peripheral);
	void disconnect_peripheral();
	void clear_connector() { connector_->clear(); }
	const tconnector& connector() const { return *connector_; }
    tconnector& connector() { return *connector_; }
	bool is_connecting() const { return !!connecting_peripheral_; }
	bool is_connected() const { return !!peripheral_; }
	const tmac_addr& current_mac_addr() const { return mac_addr_; }

	ttask& insert_task(const std::string& id);
	ttask& get_task(const std::string& id);

	// it is called by base_instance. app don't call it.
	void handle_app_event(bool background);

	bool background_callback(uint32_t ticks);

protected:
	virtual void app_discover_peripheral(SDL_BlePeripheral& peripheral) {}
	virtual void app_release_peripheral(SDL_BlePeripheral& peripheral) {}
	virtual void app_connect_peripheral(SDL_BlePeripheral& peripheral, const int error) {}
	virtual void app_disconnect_peripheral(SDL_BlePeripheral& peripheral, const int error) {}
    virtual void app_discover_services(SDL_BlePeripheral& peripheral, const int error) {}
	virtual void app_discover_characteristics(SDL_BlePeripheral& peripheral, SDL_BleService& service, const int error) {}
	virtual void app_read_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const unsigned char* data, int len) {}
	virtual void app_write_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error) {}
	virtual void app_notify_characteristic(SDL_BlePeripheral& peripheral, SDL_BleCharacteristic& characteristic, const int error) {}
	virtual bool app_task_callback(ttask& task, int step_at, bool start) { return true; }
    virtual void app_calculate_mac_addr(SDL_BlePeripheral& peripheral) { }
    
private:
	bool make_characteristic_exist(const std::string& service_uuid, const std::string& characteristic_uuid);
	void task_next_step(SDL_BlePeripheral& peripheral);

	void background_reconnect_detect(bool start);
	void clear_background_connect_flag();

public:
    SDL_BlePeripheral* connecting_peripheral_;
    SDL_BlePeripheral* peripheral_; // connected peripheral

protected:
	tmac_addr mac_addr_;
	SDL_BleCallbacks callbacks_;
    bool discover_connect_;

	bool persist_scan_;
	bool disable_reconnect_;

	std::vector<ttask> tasks_;
	ttask* current_task_;
	int current_step_;


	int disconnect_threshold_;
	std::string disconnect_music_;
	uint32_t background_scan_ticks_;
	uint32_t background_id_;

private:
	tconnector* connector_;
	static tble* singleton_;
};

#endif

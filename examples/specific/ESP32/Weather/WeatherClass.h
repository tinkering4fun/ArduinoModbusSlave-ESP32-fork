// SlaveApplicationClass.h
#ifndef SlaveApplicationClass_h
#define SlaveApplicationClass_h


#include "SlaveRtuKernelClass.h"



class WeatherClass : public SlaveRtuKernelClass {

public:

	// Modbus Input Registers for sensor data
	// -----------------------------------------------------------------
	enum {
		// DHT22
		inputRegDHT22Temp = 0,
		inputRegDHT22Hygro,
		
		// BME280
		inputRegBME280Temp,
		inputRegBME280Hygro,
		inputRegBME280Press,
		
		numInputRegs,
	};
	
	// Modbus Holding Registers for configuration
	// -----------------------------------------------------------------
	// Stored in EEPROM
	enum {
		// DHT22
		holdingRegDHT22Interval = 0,
		holdingRegBME280Interval,
		
		numHoldingRegs,
	};
	
	// EEPROM buffer structure, extending kernel configuration data
	struct ApplicationConfig : public KernelEeprom {
		
		uint16_t holdingValues[numHoldingRegs]; 
	};
	
	
public:

	WeatherClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId);
	void sensorDHT22UpdateCallback(uint16_t *regArray);
	void sensorBME280UpdateCallback(uint16_t *regArray);
	uint16_t *getHoldingRegs();

	
private:
	
	// RAM buffer
	// -----------------------------------------------------------------
	
	uint16_t _holdingRegs[numHoldingRegs];
	uint16_t _inputRegs[numInputRegs];
	ApplicationConfig _config;
	
	
	void _saveEeprom(void);
	
	// Instance methods connected to kernel callback	
	uint8_t cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length) override;
	uint8_t cbAccessInputRegisters(bool write, uint16_t address, uint16_t length) override;
	
	SemaphoreHandle_t _registerSemaphore;
	bool _semaphoreTakeFailure;
};


#endif
// eof

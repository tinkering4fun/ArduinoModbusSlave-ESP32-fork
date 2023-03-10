// WeatherClass.cc(.h)
//
// Modbus slave for weather Station sensor.
//
// This not only an example, it is already a small project.
// Can be seen as a proof of concept for the library functions.
//
// Works only on ESP32, as we use both cores and multitasking.
// The Sensor interface runs in a parallel thread / core, and
// reports new data with method call sensorUpdateXXXXCallback().
//
// This class maintains 
//
// Input registers
// ----------------------------------------------
//     0: Status Register
//     1: DHT22  Temperature (last digit == 0.1 ′C)
//     2: DHT22  Humidity    (last digit == 0.1 %)
//     3: BME280 Temperature (last digit == 0.01 ′C)
//     4: BME280 Humidity    (last digit == 0.01 %)
//     5: BME280 Pressure    (last digit == 0.01 hPa)
//
// Holding registers
// ----------------------------------------------
//     0: Sampling interval DHT22 sensor (sec)
//     1: Sampling interval DHT22 sensor (sec)
//
// It also inherits the features of the SlaveRtuKernelClass 
//
// Holding registers 
// 0x100: Slave Id
// 0x101: Baudrate
// 0x102: Communication Watchdog Timeout [ms]
// 0x103: Reboot request
//
// Werner Panocha, March 2023


#include <SlaveRtuKernelClass.h>
#include "WeatherClass.h"

// Constructor
WeatherClass::WeatherClass(Stream *serialStream, unsigned int baud, int transmissionControlPin, uint8_t slaveId) 
: SlaveRtuKernelClass(serialStream, baud, transmissionControlPin, slaveId, (uint8_t *)(&_config), sizeof(ApplicationConfig)) 
{
	
	DEBUG_PRINTLN ("SlaveApplicationClass()");

	dumpBytes("App: EEPROM #1", &_config, sizeof(ApplicationConfig));	/// debug
	
	// Verify EEPROM config
	if(eepromDefaultsRequired()){
		DEBUG_PRINT (F("App: Setting EEPROM defaults <====================\n"));
		
		// WTF is this ?
		// Without this output I could not see messages about EEPROM init ..
		for(int i =0; i < 5; i++){
			DEBUG_PRINT (F("~~  "));
			delay(500); 
		}
		DEBUG_PRINTLN ();

		
		// Populate config structure with factory default values
		_config.holdingValues[holdingRegDHT22Interval] = 10;
		_config.holdingValues[holdingRegBME280Interval] = 10;
		
		eepromWriteDefaults((uint8_t *)&_config, sizeof(ApplicationConfig));
	}

	dumpBytes("App: EEPROM #2", &_config, sizeof(ApplicationConfig)); /// debug
	
	// Populate Holding Registers from EEPROM
	for(int i = 0; i < numHoldingRegs; i++)
		_holdingRegs[i] = _config.holdingValues[i];

	// Create and release semaphore for register access
	_registerSemaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(_registerSemaphore);	

	// Enable the callbacks for the desired RTU messages to act on
	enableCallback(CB_READ_HOLDING_REGISTERS);
	enableCallback(CB_WRITE_HOLDING_REGISTERS);
	enableCallback(CB_READ_INPUT_REGISTERS);
	
	// Clear all input regs on startup  
	for(int i =0; i < numInputRegs; i++)
		_inputRegs[i] = 0;
	
	DEBUG_PRINT (F("WeatherClass(): initialized\n"));
}

// ---------------------------------------------------------------------
// Implement enabled Modbus callback methods
// ---------------------------------------------------------------------

// -- Input Registers
uint8_t WeatherClass::cbAccessInputRegisters(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessInputRegisters(): "));
	
	if ((address + length) > numInputRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;

    // Need to aquire semaphore to ensure a consistent readout without
    // interference by updates from sensor task!
    // We expect to wait only for a short time (5 ms)
    if(xSemaphoreTake(_registerSemaphore, 5 * portTICK_PERIOD_MS)){
		// Holding semaphore
		// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
			
		// Move data from register buffer to Modbus kernel send buffer
		for (int i = 0; i < length; i++){
			rtuKernel->writeRegisterToBuffer(i, _inputRegs[address + i]);
		}
	
		// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		// Release semaphore
		xSemaphoreGive(_registerSemaphore);
	}
	else {
		// We could not acquire the semaphore, which is an unexpected error
		Serial.printf("Core %d: >>> Failed to aquire semaphore for read\n", xPortGetCoreID());
		return STATUS_SLAVE_DEVICE_FAILURE;
	}
	
	return STATUS_OK;
};


// Receive updates for input registers from sensor task
void WeatherClass::sensorDHT22ErrorCallback(){
	_inputRegs[inputRegStatus] |= statusErrDHT22;	// Sensor Error
}

void WeatherClass::sensorDHT22UpdateCallback(uint16_t *regArray){
	
	Serial.printf("Core %d: DHT22 Sensor update received\n", xPortGetCoreID());
	
    // Need to aquire semaphore to ensure a consistent update 
    // We expect to wait only for a short time (5 ms)
    if(xSemaphoreTake(_registerSemaphore, 5 * portTICK_PERIOD_MS)){
		// Holding semaphore
		// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		
		// Move data ...
		_inputRegs[inputRegDHT22Temp]  = regArray[inputRegDHT22Temp];
		_inputRegs[inputRegDHT22Hygro] = regArray[inputRegDHT22Hygro];
		
		// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		// Release semaphore
		xSemaphoreGive(_registerSemaphore);
	}
	else {
		// We could not acquire the semaphore, which is an unexpected error
		Serial.printf("Core %d: >>> Failed to aquire semaphore for DHT22 update\n", xPortGetCoreID());
		_inputRegs[inputRegStatus] |= statusErrSemaphore;	// Semaphore Error
	}	
};

// Receive updates for input registers from sensor task
void WeatherClass::sensorBME280ErrorCallback(){
	_inputRegs[inputRegStatus] |= statusErrBME280;	// Sensor Error
}

void WeatherClass::sensorBME280UpdateCallback(uint16_t *regArray){
	
	Serial.printf("Core %d: BME280 Sensor update received\n", xPortGetCoreID());
	
    // Need to aquire semaphore to ensure a consistent update 
    // We expect to wait only for a short time (5 ms)
    if(xSemaphoreTake(_registerSemaphore, 5 * portTICK_PERIOD_MS)){
		// Holding semaphore
		// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		
		// Move data ...
		_inputRegs[inputRegBME280Temp]  = regArray[inputRegBME280Temp];
		_inputRegs[inputRegBME280Hygro] = regArray[inputRegBME280Hygro];
		_inputRegs[inputRegBME280Press] = regArray[inputRegBME280Press];
		
		// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
		// Release semaphore
		xSemaphoreGive(_registerSemaphore);
	}
	else {
		// We could not acquire the semaphore, which is an unexpected error
		Serial.printf("Core %d: >>> Failed to aquire semaphore for BME280 update\n", xPortGetCoreID());
		_inputRegs[inputRegStatus] |= statusErrSemaphore;	// Semaphore Error
	}	
};



// -- Holding Registers
uint8_t WeatherClass::cbAccessHoldingRegisters(bool write, uint16_t address, uint16_t length)
{
	DEBUG_PRINT (F("cbAccessHoldingRegisters(): "));
	if(write)
		DEBUG_PRINT (F("write\n"));
	else
		DEBUG_PRINT (F("read\n"));
	
	if ((address + length) > numHoldingRegs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		if(write)
			_holdingRegs[address + i] = rtuKernel->readRegisterFromBuffer(i);
		else
			rtuKernel->writeRegisterToBuffer(i, _holdingRegs[address + i]);
	}
	
	if(write) {	
		// Copy to config & EEPROM
		for(int i = 0; i < numHoldingRegs; i++)
			_config.holdingValues[i] = _holdingRegs[i];
		
		_saveEeprom();
	}
		
	return STATUS_OK;
};


uint16_t * WeatherClass::getHoldingRegs(){
	return _config.holdingValues;
}


void WeatherClass::_saveEeprom(void){
	DEBUG_PRINT (F("App: save EEPROM\n"));
	_eepromWrite((uint8_t *)&_config, sizeof(_config));
}

// eof

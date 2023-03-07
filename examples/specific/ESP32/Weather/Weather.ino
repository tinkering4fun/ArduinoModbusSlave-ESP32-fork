// Weather.ino
/*
    Modbus slave -  Weather Station

	Runs on ESP32 using both cores.
	
	Core 1 runs the Modbus task, and 
	Core 0 acts as sensor driver.

	See WeatherClass.cc.h for more details
	
	
    Werner Panocha, March 2023
*/

// Notice: WiFi will be disabled on setup
#include <WiFi.h>
#include <esp_wifi.h>
#include "driver/adc.h"

#include <ModbusSlave.h>

// Base class for a Modbus Slave, built on top of ModbusSlave
#include <SlaveRtuKernelClass.h>

// Application logic of a Modbus Slave
#include "WeatherClass.h"			// Header


// Required for DHT22
#include <DHT22.h>			// https://github.com/dvarrel/DHT22
#include <arduino-timer.h> 	// https://github.com/contrem/arduino-timer
#define DHT22_SDA_PIN SDA	// SDA pin for DHT22 sensor


// Dirty trick to use a C++ class in Arduino IDE w/o making a library ...
// Name the .cc Class file as .h Header file.
// The file will appear as a new Tabstrip in the Arduino IDE
#include "WeatherClass.cc.h"

// The Modbus slave hided in a class
WeatherClass *slaveInstance;


TaskHandle_t ModbusTask;
TaskHandle_t SensorDHT22Task;


#define SLAVE_ID 1

// RS485 Modbus Interface 
#define RS485_BAUDRATE 	9600 						// Baudrate for Modbus communication.
#define RS485_SERIAL 	Serial2   					// Serial port for Modbus communication.
#define RS485_CTRL_PIN	MODBUS_CONTROL_PIN_NONE 	// GPIO number for Control pin (optionally)





// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{
	// Use this for debug messages etc.
	Serial.begin(115200);
	Serial.print("\nModbus server - Weather Station example\n");

	
	// Disable WiFi, as a pure Modbus server has no need for it
	adc_power_off();
	WiFi.disconnect(true);  // Disconnect from the network
	WiFi.mode(WIFI_OFF);    // Switch WiFi off
	
	
	slaveInstance = new WeatherClass(&(RS485_SERIAL), RS485_BAUDRATE, RS485_CTRL_PIN, SLAVE_ID);
	
    // Create a separate task for the DHT22 sensor driver
    //
    // Notice:
    // DHT22 need higher priority, otherwise it may fail reading the
    // bits from the wire due to interrupts etc.
	xTaskCreatePinnedToCore(
		SensorDHT22, 	// Function to implement the task
		"DHT22", 		// Name of the task
		8192,  			// Stack size in words 
		slaveInstance, 	// Task parameter       <== pointer to Modbus slave!
		1,  			// Priority of the task <== DHT22 need high priority!
		&SensorDHT22Task,	// Task handle.
		0); 			// Core where the task should run


	// Arduino code runs per default in Core 1
	Serial.printf("Core %d: Modbus task, I will serve Modbus requests\n", xPortGetCoreID());
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
	// Call this periodically ...
	// It will run the ModbusSlave Library and also invoke any
	// enabled callback methods of the slave instance.
	// The slave instance will do whatever it takes, when a
	// Modbus entity was accessed.
    slaveInstance->poll();
}




// The following function is started in Core 0, so
// it does not interfere with the Modbus Server task
//
// Rem:
// The following code is taken from DHT22 example Interval.ino
//
// It is a benefit of the ESP32 treaded approach that it is easy to
// take over code from library examples etc.
// Even from 'unfriendly' libraries which use 'delay()' for timings.


// DHT22 support class (make it global accesible)
DHT22 dht22(DHT22_SDA_PIN); 
bool dht22UpdateRequested;
	
// ---------------------------------------------------------------------
void SensorDHT22( void * parameter) 
// ---------------------------------------------------------------------
{
	// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
	// This section is similar to Arduino setup() function
	
	Serial.printf("Core %d: Sensor task\n", xPortGetCoreID());
	
	// Parameter is a pointer to the Modbus slave class
	class WeatherClass *slave = (class WeatherClass *)parameter;
	uint16_t *holdingRegs = slave->getHoldingRegs();
	
	
	// A private copy of the input registers for the sensor task
	uint16_t inputRegs[WeatherClass::numInputRegs];
	
	// Create a timer with default settings
	auto timer = timer_create_default(); 
	
	
	// Startup  DHT22 ... until begin() returns OK
	while(dht22.begin() < 0)
		yield();

	
	// DHT22 conversion is already running after begin()
	dht22UpdateRequested = true;
	
	// Interval timer (load from holding register value)
	timer.every(holdingRegs[WeatherClass::holdingRegDHT22Interval] * 1000, newDHT22Conversion);
	
	Serial.println("DTH22 sensor initialized");
	// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	
	// Endless loop ...
	while(true){
		// >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
		// This section is similar to Arduino loop() function
		
		float t, h;
		
		// Run timer, will periodically request an update of DHT22 data
		timer.tick();	
		
		// If a request was made, wait for end of conversion
		if(dht22UpdateRequested && !dht22.conversionInProgress()){
			dht22UpdateRequested = false;
			
			// Read the sensor values
			int rc;
			if((rc = dht22.readSensor()) == DHT22::OK){
			
			
				h = dht22.getHumidity();
				t = dht22.getTemperature();
				
	
				// Print data
				Serial.print("h=");Serial.print(h,1);Serial.print("\t");
				Serial.print("t=");Serial.println(t,1);
				
				// Populate Modbus register buffer with data
				inputRegs[WeatherClass::inputRegDHT22Temp] 	= t * 10;
				inputRegs[WeatherClass::inputRegDHT22Hygro]	= h * 10;
				
				// Call slave to update
				slave->sensorUpdateCallback(inputRegs);
				Serial.printf("Core %d: Sensor update delivered\n", xPortGetCoreID());
			}
			else {
				Serial.printf("Core %d: ERROR: cannot read DHT22, RC: %d\n", xPortGetCoreID(), rc);
			}
		}	
		// <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
	}
}

// Periodic callback from DHT22 timer object
// ---------------------------------------------------------------------
bool newDHT22Conversion(void *) {
   
   Serial.printf("Core %d: newDHT22Conversion()\n", xPortGetCoreID());
   
	if(dht22.conversionInProgress()){
		// This should not happen, interval too short?!
		Serial.println("ERROR conversion already in progress, interval too short?");
		return false; // stop timer repetitions!
	}

	// Request a conversion (discarding the retrieved data for now)
	dht22.readSensor();
	// Set a flag, to make loop() aware of the new conversion
	dht22UpdateRequested = true;
	return true; // repeat? true
}

// eof

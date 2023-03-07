// Weather.ino
/*
    Modbus slave -  Weather Station

	Runs on ESP32 using both cores.
	
	Core 1 runs the Modbus task, and 
	Core 0 runs the tasks for DHT22 and BME280 sensors

	
	This example is a proof of concept on building 
	practical Modbus slaves with the SlaveRtuKernelClass.
	
	See WeatherClass.cc.h for more details about functions and registers.
	
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
							// using ==> https://github.com/tinkering4fun/DHT22-ESP32-fork
#define DHT22_SDA_PIN 19	// SDA pin for DHT22 sensor

// Required for BME280
#include "BlueDot_BME280.h"	// https://github.com/BlueDot-Arduino/BlueDot_BME280
#define BME280_I2C_ADDR 0x77	// Default 0x77 / alternate 0x76


// Dirty trick to use a C++ class in Arduino IDE w/o making a library ...
// Name the .cc Class file as .h Header file.
// The file will appear as a new Tabstrip in the Arduino IDE
#include "WeatherClass.cc.h"

// The Modbus slave hided in a class
WeatherClass *slaveInstance;
#define SLAVE_ID 1


// Taskhandles
TaskHandle_t SensorDHT22Task;
TaskHandle_t SensorBME280Task;



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
		2,  			// Priority of the task <== DHT22 need high priority!
		&SensorDHT22Task,	// Task handle.
		0); 			// Core where the task should run
 

    // Create a separate task for the BME280 sensor driver
    //
	xTaskCreatePinnedToCore(
		SensorBME280, 	// Function to implement the task
		"BME280", 		// Name of the task
		8192,  			// Stack size in words 
		slaveInstance, 	// Task parameter       <== pointer to Modbus slave!
		1,  			// Priority of the task  
		&SensorBME280Task,	// Task handle.
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



// ---------------------------------------------------------------------
// The following sensor functions are started in Core 0, so they do not
// interfere with the Modbus Server task.
//
// Notice:
// The tasks need to be nice to each other and shall not block longer
// than necessary.
// Therefore we use vTaskDelay() to suspend the tasks when applicable.
// Updates to the Modbus Register Buffer are performed by callbacks 
// to WeatherClass, which takes care about Semaphore locking.
// ---------------------------------------------------------------------

// DHT22 support class (make it global accesible)
DHT22 dht22(DHT22_SDA_PIN); 
	
// ---------------------------------------------------------------------
void SensorDHT22( void * parameter) 
// ---------------------------------------------------------------------
{
	// This section is similar to Arduino setup() function
	
	Serial.printf("Core %d: DHT22 Sensor task\n", xPortGetCoreID());
	
	// Parameter is a pointer to the Modbus slave class
	class WeatherClass *slave = (class WeatherClass *)parameter;
	uint16_t *holdingRegs = slave->getHoldingRegs();
	
	
	// A private copy of the input registers for the sensor task
	uint16_t inputRegs[WeatherClass::numInputRegs];
	
	// Startup  DHT22 ... until begin() returns OK
	while(dht22.begin() < 0)
		vTaskDelay( 100 / portTICK_PERIOD_MS);

	// DHT22 conversion is already running after begin()
	
	Serial.println("DTH22 sensor initialized");
	
	// Endless loop ...
	while(true){
		// This section is similar to Arduino loop() function
		int rc;
		float t, h;
		
		// Suspend task while the DHT22 conversion is running ...
		vTaskDelay( DHT22::cSamplingTime / portTICK_PERIOD_MS);

		// Read the sensor values
		if((rc = dht22.readSensor()) == DHT22::OK){
		
			h = dht22.getHumidity();
			t = dht22.getTemperature();		
			
			// Populate Modbus register buffer with data
			inputRegs[WeatherClass::inputRegDHT22Temp] 	= t * 10;
			inputRegs[WeatherClass::inputRegDHT22Hygro]	= h * 10;
			
			// Call slave to update
			slave->sensorDHT22UpdateCallback(inputRegs);
			
			Serial.printf("Core %d: DHT22   t: %5.2f  h: %5.2f\n" , xPortGetCoreID(), t, h);
		}
		else {
			Serial.printf("Core %d: ERROR: cannot read DHT22, RC: %d\n", xPortGetCoreID(), rc);
		}

		// Suspend task until start of next conversion
		vTaskDelay( ((holdingRegs[WeatherClass::holdingRegDHT22Interval] *1000) - DHT22::cSamplingTime) / portTICK_PERIOD_MS);

		// Perform readSensor() only to start a new conversion, 
		// ignoring any data read at this time
		if((rc = dht22.readSensor()) != DHT22::OK){
			Serial.printf("Core %d: ERROR: cannot request DHT22 SOC, RC: %d\n", xPortGetCoreID(), rc);
		}
	}
}


// Global access
BlueDot_BME280 bme280 = BlueDot_BME280();

// ---------------------------------------------------------------------
void SensorBME280( void * parameter) 
// ---------------------------------------------------------------------
{
	Serial.printf("Core %d: BME280 Sensor task\n", xPortGetCoreID());
	
	// Parameter is a pointer to the Modbus slave class
	class WeatherClass *slave = (class WeatherClass *)parameter;
	uint16_t *holdingRegs = slave->getHoldingRegs();
	
	
	// A private copy of the input registers for the sensor task
	uint16_t inputRegs[WeatherClass::numInputRegs];
	
	
	bme280.parameter.communication = 0;   			// I2C
	bme280.parameter.I2CAddress = BME280_I2C_ADDR;	// address
	bme280.parameter.sensorMode = 0b11;             //Choose sensor mode
	bme280.parameter.IIRfilter = 0b100;             //Setup for IIR Filter
	bme280.parameter.humidOversampling = 0b101;     //Setup Humidity Oversampling
	bme280.parameter.tempOversampling = 0b101;      //Setup Temperature Ovesampling
	bme280.parameter.pressOversampling = 0b101;     //Setup Pressure Oversampling 
	bme280.parameter.pressureSeaLevel = 1013.25;    //default value of 1013.25 hPa
	bme280.parameter.tempOutsideCelsius = 15;       //default value of 15°C
	
	// Check Chip ID
	if (bme280.init() != 0x60){  
		 
		Serial.printf("Core %d: BME280 ERROR: Sensor not found, task halted\n", xPortGetCoreID());
		while(true)
			vTaskDelay( 100 / portTICK_PERIOD_MS);
	}
	
	Serial.printf("Core %d: BME280  Sensor ready\n", xPortGetCoreID());
	// Endless loop ...
	while(true){
		
		float t, h, p;
		
		t = bme280.readTempC();			// Celsius
		vTaskDelay( 100 / portTICK_PERIOD_MS);
		h = bme280.readHumidity();		// %
		vTaskDelay( 100 / portTICK_PERIOD_MS);
		p = bme280.readPressure();		// hPa
		
		// Populate Modbus register buffer with data
		inputRegs[WeatherClass::inputRegBME280Temp] 	= t * 100;
		inputRegs[WeatherClass::inputRegBME280Hygro]	= h * 100;
		inputRegs[WeatherClass::inputRegBME280Press]	= p * 10;
			
		// Call slave to update
		slave->sensorBME280UpdateCallback(inputRegs);
			
		Serial.printf("Core %d: BME280  t: %5.2f  h: %5.2f  p: %5.2f\n" , xPortGetCoreID(), t, h, p);
		
		// Suspend task until start of next conversion
		vTaskDelay( 5000 - 200/ portTICK_PERIOD_MS);
		vTaskDelay( ((holdingRegs[WeatherClass::holdingRegBME280Interval] *1000) - 200) / portTICK_PERIOD_MS);
	}
}
// eof

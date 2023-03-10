// DHT22.ino
/*
    Modbus slave, for a DHT22 sensor
    
    This example makes use of ESP32 multithreading, which keeps
	the Modbus Slave responsive while the DHT22 data reading takes place.
	 
	
	Just to demonstrate that the Modbus Slave is responsive while a
	a parallel task performs blocking sensor code ...
	A big benefit of the ESP32 to solve challenges with some sensors etc.
	
	And best of all, it is quite simple ...
	
	Many credits to Rui Santos for writing this great tuturial:
	==> "How to use ESP32 Dual Core with Arduino IDE"
	see https://randomnerdtutorials.com/esp32-dual-core-arduino-ide/
 
 
	
    Werner Panocha, February 2023

 

    https://github.com/yaacov/ArduinoModbusSlave
*/

//#include <ModbusSlave.h>

// DHT22 sensor is supported by library "DHT22 by dvarrel, Version 1.0.2"
// see https://github.com/dvarrel/DHT22
#include <DHT22.h>
#define DHT22_DATA_PIN	23	// GPIO23

#define data 23

DHT22 dht22(data); 


#if NIX

// Sensor driver in own task
TaskHandle_t SensorTask;
DHT22 *sensor;

#define SLAVE_ID 1

// RS485 Modbus Interface 
#define RS485_BAUDRATE 	9600 						// Baudrate for Modbus communication.
#define RS485_SERIAL 	Serial2   					// Serial port for Modbus communication.
#define RS485_CTRL_PIN	MODBUS_CONTROL_PIN_NONE 	// GPIO number for Control pin (optionally)

// Pointer to Modbus slave object
Modbus *slave;

// A set of input registers with sensor data
uint16_t input_regs[] = {0};

// Number of available registers
const uint8_t num_input_regs = sizeof(input_regs) / sizeof(input_regs[0]);

// ---------------------------------------------------------------------
// Handle the function code Read Input Registers (FC=04) 
// ---------------------------------------------------------------------
uint8_t readInputRegs(uint8_t fc, uint16_t address, uint16_t length, void *context)
{
	Serial.printf("Core %d: Modbus read Input Registers (callback)\n", xPortGetCoreID());
	if ((address + length) > num_input_regs)
		return STATUS_ILLEGAL_DATA_ADDRESS;
    
	for (int i = 0; i < length; i++){
		slave->writeRegisterToBuffer(i, input_regs[address + i]);
	}
	return STATUS_OK;
}
#endif



// ---------------------------------------------------------------------
void setup()
// ---------------------------------------------------------------------
{
	
  Serial.begin(115200); //1bit=10µs
  Serial.println("\ntest capteur DTH22");
  
#if NIX
	// Use this for debug messages etc.
	Serial.begin(115200);
	Serial.print("\nModbus server - DHT22 sensor\n");
	
	
	// Sensor
	sensor = new DHT22(DHT22_DATA_PIN);
	return;
	
	// Initialize slave instance
	slave = new Modbus(RS485_SERIAL, SLAVE_ID, RS485_CTRL_PIN);
	
	// Minimalistic slave functionality
	slave->cbVector[CB_READ_INPUT_REGISTERS] 	= readInputRegs;
	
	
	// Set the serial port and slave to the given baudrate.
	RS485_SERIAL.begin(RS485_BAUDRATE);
	slave->begin(RS485_BAUDRATE);

    // Create a separate task for the Sensor's stuff
	xTaskCreatePinnedToCore(
		SensorFunction, // Function to implement the task
		"Sensor", 		// Name of the task
		10000,  		// Stack size in words 
		input_regs, 	// Task input parameter ==> pointer to registers
		0,  			// Priority of the task 
		&SensorTask,	// Task handle.
		0); 			// Core where the task should run


	// Arduino code runs per default in Core 1
	Serial.printf("Core %d: Modbus task, I will serve Modbus requests\n", xPortGetCoreID());
	#endif
}

// ---------------------------------------------------------------------
void loop()
// ---------------------------------------------------------------------
{
  
	// dht22.debug();
	
	dht22.measureTimings();
	delay(2500);
	uint8_t err = dht22.readSensor();
	
	
  float t = dht22.getTemperature();
  float h = dht22.getHumidity();

  Serial.print("h=");Serial.print(h,1);Serial.print("\t");
  Serial.print("t=");Serial.println(t,1);
  delay(2500);
  return;
  
 #if NIX 
	// Listen for modbus requests on the serial port.
	// Will trigger registered callback if appropriate message was received
	slave->poll();
#endif
}

#if NIX
// The following function is started in other core, so
// it does not interfere with the Modbus Server task
// ---------------------------------------------------------------------
void SensorFunction( void * parameter) 
// ---------------------------------------------------------------------
{
	Serial.printf("Core %d: DHT22 sensor\n", xPortGetCoreID());
	

	
	// Assume the parameter is a pointer to our input register array
	uint16_t *inputs = (uint16_t *)parameter;
	
	// DHT22 sensor
	float t, h;
	

	// Try to access the sensor
	while( (t = sensor->getTemperature()) == -273.0){
		// Zero Kelvin is interpreted as Error
		Serial.printf("Core %d: DHT22 not found, %f\n", xPortGetCoreID(), t);
		delay(5000);
	}
	
	Serial.printf("Sensor found\n");
	// Endless loop ...
	while(true){
   
		t = sensor->getTemperature();
		h = sensor->getHumidity();
		Serial.print("h=");Serial.print(h,1);Serial.print("\t");
		Serial.print("t=");Serial.println(t,1);
		
		delay(5000);
	}
  
}
#endif
// eof

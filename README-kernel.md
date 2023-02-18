# ModbusSlave - Kernel Extension

#### SlaveRTUKernelClass (a facade for the ModbusSlave library)

The basic idea was to provide some core features you may eventually like to have in every custom Modbus slave ..

* Holding Registers backed up in EEPROM
* Set Modbus Interface parameters via Holding Registers
	* Slave ID
	* Baudrate
* Communication Failure Watchdog (optional)
* Restart command

**Holding Registers** provided by this class start ** at offset 0x0100**

| Address   | Function                       |  
|-----------|:-------------------------------|  
| 0x0100    | Slave ID                       |  
| 0x0101    | Baud Rate                      |  
| 0x0102    | Comm. Watchdog Timeout [ms]    |  
| 0x0103    | Restart request                |  

#### Access methods for the Modbus Entity types

* Holding Registers (read/write)
* Coils (read/write)
* Discrete Inputs (read only)
* Input Registers (read only)

*Notice:  
All entity have their own address range starting at 0x0000.*

A derived application class shall overwrite the applicable virtual entity access method(s).

Refer to example ```kernel/Simple.ino``` for an illustration of the concept.  
The application logic is contained in ```SlaveApplicationClass.cc.h``` 
which appears in a separate Tab of the Arduino IDE.  

_Notice the naming trick for the class file, to get things compiled in Arduino IDE._   





#### Communication Failure Watchdog 
Allows to implement safety measures in case of communication loss due due broken wires or some 
failure of the Modbus Master MCU (software).

If the feature is enabled by Holding Register #0x0102, this register needs to be periodically polled
in the programmed interval.  
Otherwise the Watchdog will trigger an the virtual method  **```cbCommunicationLost()```** will be called once.  
If a poll is received again **```cbCommunicationReestablished()```** will be called once.

The application slave class can make use of the Watchdog by overwriting the above methods.


#### Restart Command  

If the value 0xFFFF was written to Holding Register #0x0103, a software restart executed with the next ```poll()```  



-------------------------------------------------------------

_To be continued / completed ..._     
_Werner Panocha, Februray 2023_  


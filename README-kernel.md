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

| Addr.     | Function                       |
|-----------|:-------------------------------|  
| 0x0100    | __Slave ID__                   |  
| 0x0101    | __Baud Rate__                  |  
| 0x0102    | __Comm. Watchdog Timeout__ [ms]|  
| 0x0103    | __Restart request__            |  

## A word on Modbus Entity types and Addresses (RTU Mode)

_Just a brief description because I felt some confusion about Modbus addressing._   
_On the other hand there is plenty of good documentation of this pretty old Modbus RTU protocol._

A Modbus Slave is addressed by an 8-bit __Slave ID__ which must be __unique on the RS485 bus__.

The Modbus Protocol knows  __4 different entity types__:

* Holding Registers (read/write)
* Coils (read/write)
* Discrete Inputs (read only)
* Input Registers (read only)

A __Modbus slave may serve any combination and number of entities__ from the above collection.  
_Thus slaves are nowadays also named **Modbus Server** and the master is named **Modbus Client**._  

On Modbus Protocol level an entity address is a 16-bit word.  
Be aware that those __addresses are not to be considered as unique__ on a slave.  
Because __each entity type spans its own 16-bit address range__ starting at 0x0000.  
Thus a slave may present a Coil with address 0x0000 as well as a Holding Register with address 0x0000 etc.

This works because a Modbus Request Message consists of an 8-bit function code (FC).  
This __FC selects the entity type and desired operation__ on the addressed entity item(s).

Now comes the funny / confusing part ...  
Modbus was originally invented in 1979 as extension bus for PLCs. And in the PLC world's documentation and software one may find a __5-digit decimal addressing convention__, where the 1st digit implies the entity type.

| Addr           | Entity Type (operation)        |
|:---------------|:-------------------------------|  
|00001 - 09999   | Coils (read/write)             |
|10001 - 19999   | Discrete Inputs (read only)    |
|30001 - 39999   | Input Registers (read only)    |
|40001 - 49999   | Holding Registers (read/write) |

Note:  
The __5-digit addresses are 1-based__, meaning each X0001 - X9999 range translates in the 16-bit address range 0x0000 - 0x270E.

Assuming you built a slave with a Holding Register 0x0005, and you are testing with some __PLC oriented Modbus Master Software__ dealing with 5-digit addresses.  
In this case it may require to specify e.g. address 40006 (5 + 40001) to make the software using the Modbus FC's applicable for Holding Register access and set entity address to 0x0005.  



## Access methods for the Modbus Entity types
A derived application class shall overwrite the applicable virtual entity access method(s).

Refer to example ```kernel/Simple.ino``` for an illustration of the concept.  
The application logic is contained in ```SlaveApplicationClass.cc.h``` 
which appears in a separate Tab of the Arduino IDE.  

_Notice the naming trick for the class file, to get things compiled in Arduino IDE._   





## Communication Failure Watchdog 
Allows to implement safety measures in case of communication loss due due broken wires or some 
failure of the Modbus Master MCU (software).

If the feature is enabled by Holding Register #0x0102, this register needs to be periodically polled
in the programmed interval.  
Otherwise the Watchdog will trigger an the virtual method  **```cbCommunicationLost()```**  will be called once.  
If a poll is received again **```cbCommunicationReestablished()```** will be called once.

The application slave class can make use of the Watchdog by overwriting the above methods.


## Restart Command  

If the value 0xFFFF was written to Holding Register #0x0103, a software restart executed with the next ```poll()```  



-------------------------------------------------------------

_To be continued / completed ..._     
_Werner Panocha, Februray 2023_  


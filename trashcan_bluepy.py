import binascii
import struct
import time
import signal
from bluepy import btle
from threading import Event
 
U1 = btle.UUID(0xA003)		#U1: humidity
U2 = btle.UUID(0xA004)		#U2: temperature
U3 = btle.UUID(0xA005)		#U3: proximity
U4 = btle.UUID(0xA008)		#U4: stench
Uthrow = btle.UUID(0xA006)	#send back the result to STM32

Ubutton = btle.UUID(0xA001)

exit = Event()
EMPTY = True			#the condtion of trash can and BLE connection
t = 0
connection = True	
busy = False

class STM(btle.Peripheral):
    def _connect(self, addr, addrType="random", iface=None):	#STM inherite Periphral in btle
        self._startHelper(iface)				#Rpi will connect again instead of
        self.addr = addr					#raising BTLEDisconnectError
        self.addrType = addrType				
        self.iface = iface
        global connection
        if iface is not None:
           self._writeCmd("conn %s %s %s\n" % (addr, addrType, "hci"+str(iface$
        else:
            self._writeCmd("conn %s %s\n" % (addr, addrType))
        rsp = self._getResp('stat')
        while rsp['state'][0] == 'tryconn':
            rsp = self._getResp('stat')
        if rsp['state'][0] != 'conn':
            self._stopHelper()
            print "connection failed\n"
            connection = False
            
def main():						#recieve data every 7 second
    global t
    while not exit.is_set():
        t = connectToSTM32(t)
        exit.wait(7)
        
def quit(signo, _frame):
    print("Interrupted by %d, shutting down" % signo)
    exit.set()

def get_value(ch, data):				#decode the data from STM32
    val = binascii.b2a_hex(ch.read())
    val = binascii.unhexlify(val)
    val = struct.unpack(data, val)[0]
    return val

def speedUpTheClock(hum, tem, odd, prox, t):		#the condition of trash can
    if prox<200 or odd>0.13:				
        return t+1000A					

    if tem>20 and odd>0 and hum>65:			
        if tem>30:					
            return t+0.06*(hum-65)+max(0, 0.2*(hum-80))
        return t+1+0.03*(hum-65)+max(0, 0.1*(hum-80))
    if tem<10:						
        return t					
    
    return t+1

def toThrowTheTrash(t, prox, channel):			#decide to throw the trash or not
    global EMPTY					
    if prox>240 and not EMPTY:
        #channel.write("\x00")
        EMPTY = True
        print "reset t"
        return 0
    if t>30 and not busy:
        channel.write("\x01")
        print "THROWTHROWTHROWTHROWTHROW"
        EMPTY = False
        return t

    #channel.write("\x00")
    return t

def toWashTheCan(prox, odd, channel):			#decide to wash the can or not
    global EMPTY
    if prox>240 and odd>0.1:
        channel.write("\x02")
        print "WASHWSAHWASHWASHWASHWASH"
        EMPTY = False
        return  
def connectToSTM32(t):   				#receive data from STM32
    global EMPTY, connection   
    p = STM("fc:0a:95:12:dd:71", "random")
    if not connection:
        connection = True
        return t

    ch1 = p.getCharacteristics(uuid=U1)[0]
    ch2 = p.getCharacteristics(uuid=U2)[0]    
    ch3 = p.getCharacteristics(uuid=U3)[0]
    ch4 = p.getCharacteristics(uuid=U4)[0]
    chthrow = p.getCharacteristics(uuid=Uthrow)[0]
    chbutton = p.getCharacteristics(uuid=Ubutton)[0]

    val1 = get_value(ch1, "f")				#decode
    val2 = get_value(ch2, "f")
    val3 = get_value(ch3, "f")
    val4 = get_value(ch4, "f")
    but = get_value(chbutton, "b")

    toWashTheCan(val4, val3, chthrow)			
    t = speedUpTheClock(val1, val2, val3, val4, t)
    t = toThrowTheTrash(t, val4, chthrow)
    print EMPTY
    p.disconnect()
    
    print t
    print val1, val2, val3, val4
    print ""

    return t

if __name__ == '__main__':				

    for sig in ('TERM', 'HUP', 'INT'):
        signal.signal(getattr(signal, 'SIG'+sig), quit);

    main()

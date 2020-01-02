import binascii
import struct
import time
from bluepy import btle
 
U1 = btle.UUID(0xA003)
U2 = btle.UUID(0xA004)
U3 = btle.UUID(0xA005)
 
p = btle.Peripheral("fc:0a:95:12:dd:71", "random")

try:
    ch1 = p.getCharacteristics(uuid=U1)[0]
    ch2 = p.getCharacteristics(uuid=U2)[0]
    ch3 = p.getCharacteristics(uuid=U3)[0]
    if (ch1.supportsRead() or ch2.supportsRead() or ch3.supportsRead()):
        while 1:
            val1 = binascii.b2a_hex(ch1.read())
            val1 = binascii.unhexlify(val1)
            val1 = struct.unpack('f', val1)[0]
            
            val2 = binascii.b2a_hex(ch2.read())
            val2 = binascii.unhexlify(val2)
            val2 = struct.unpack('f', val2)[0]

            val3 = binascii.b2a_hex(ch3.read())
            val3 = binascii.unhexlify(val3)
            val3 = struct.unpack('f', val3)[0]

	    print str(val1) + " deg C"
            print str(val2) + " %"
            print str(val3) + " %"
            time.sleep(1)
 
finally:
    p.disconnect()



import traceback
import time,sys
from udpwkpf import WuClass, Device
from twisted.internet import reactor
from math import log
from udpwkpf_io_interface import *

PIN = 3 #Analog pin 0
REFRESH_RATE = 0.5 

class Temperature_sensor(WuClass):
    def __init__(self,pin):
        self.ID = 1013
        self.refresh_rate = REFRESH_RATE
        reactor.callLater(self.refresh_rate,self.refresh)
        self.celsius = 0
        print "temperature sensor init!"

    def update(self,obj,pID,value):
        pass

    def refresh(self):
        try:
            self.celsius = temp_read(PIN)
            print "WKPFUPDATE(Temperature): %d degrees Celsius" %self.celsius
            reactor.callLater(self.refresh_rate,self.refresh)
        except IOError:
            print ("Error")
            reactor.callLater(self.refresh_rate,self.refresh)
          
class MyDevice(Device):
    def __init__(self,addr,localaddr):
        Device.__init__(self,addr,localaddr)

    def init(self):
        m = Temperature_sensor(PIN)
        self.addClass(m,1)
        self.obj_temperature_sensor = self.addObject(m.ID)
        reactor.callLater(0.5,self.loop)
    
    def loop(self):
        #print "WKPFUPDATE(Temperature): %d degrees Celsius" % self.m.celsius
        self.obj_temperature_sensor.setProperty(0, self.obj_temperature_sensor.cls.celsius)
        reactor.callLater(0.5,self.loop)

if len(sys.argv) <= 2:
        print 'python udpwkpf.py <ip> <port>'
        print '      <ip>: IP of the interface'
        print '      <port>: The unique port number in the interface'
        print ' ex. python udpwkpf.py 127.0.0.1 3000'
        sys.exit(-1)

d = MyDevice(sys.argv[1],sys.argv[2])

reactor.run()


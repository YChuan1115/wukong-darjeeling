import os, sys , gevent
import unittest
import random
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..'))
from test_environment_device import WuTest
from configuration import *
from math import ceil

class TestOneBlock(unittest.TestCase):
    def setUp(self):
        self.test = WuTest(False, False)

    def test_basic_one_block(self):        
	self.test.deviceResume(DEVICE2)
	self.test.deviceResume(DEVICE3)
	gevent.sleep(1)
	self.test.setLocation(3, "/WuKong/slider")
	self.test.setLocation(2, "/WuKong/binary")
	self.test.setLocation(4, "/WuKong/sound")
        nodes_info = self.test.discovery()
        self.test.loadApplication("applications/one_device_to_three_devices") 
    	self.test.mapping(nodes_info)
    	self.test.deploy_with_discovery()
	gevent.sleep(10)
	
	slider_node_id = 3
	slider_port = 2
	slider_wuclass_id = 15
	slider_property_number = 2
        slider_datatype = 'short'
        slider_value = 50
        self.test.setProperty(slider_node_id, slider_port, slider_wuclass_id, slider_property_number, slider_datatype, slider_value)

	
        binary_sensor_node_id = 2
        binary_sensor_port = 1
        binary_sensor_wuclass_id = 10
        binary_property_number = 1
        binary_datatype = 'boolean'
        binary_value = 1
        self.test.setProperty(binary_sensor_node_id, binary_sensor_port, binary_sensor_wuclass_id, binary_property_number, binary_datatype, binary_value)

        sound1_node_id = 2
        sound2_node_id = 3
        sound3_node_id = 4

        sound_port = 3
        sound_wuclass_id = 204
        sound_property_number = 0
        
        for loop in xrange(0,TotalWaitTime):
                gevent.sleep(0.1)
                res = self.test.getProperty(sound1_node_id, sound_port, sound_wuclass_id, sound_property_number)
                if res == (binary_value == 1):
                        break
        self.assertNotEqual(loop, TotalWaitTime - 1)
        for loop in xrange(0,TotalWaitTime):
                gevent.sleep(0.1)
                res = self.test.getProperty(sound2_node_id, sound_port, sound_wuclass_id, sound_property_number)
                if res == (binary_value == 1):
                        break
        self.assertNotEqual(loop, TotalWaitTime - 1)	
        for loop in xrange(0,TotalWaitTime):
                gevent.sleep(0.1)
                res = self.test.getProperty(sound3_node_id, sound_port, sound_wuclass_id, sound_property_number)
                if res == (binary_value == 1):
                        break
        self.assertNotEqual(loop, TotalWaitTime - 1)

        self.test.deviceWait(DEVICE2)
	binary_value = 0
        self.test.setProperty(binary_sensor_node_id, binary_sensor_port, binary_sensor_wuclass_id, binary_property_number, binary_datatype, binary_value)

	for loop in xrange(0,TotalWaitTime):
		gevent.sleep(0.1)
		res = self.test.getProperty(sound1_node_id, sound_port, sound_wuclass_id, sound_property_number)
       		if res == (binary_value == 1):
			break	
	self.assertNotEqual(loop, TotalWaitTime - 1)
        #res = self.test.getProperty(sound2_node_id, sound_port, sound_wuclass_id, sound_property_number)
        #self.assertEqual(res, binary_value)
        for loop in xrange(0,TotalWaitTime):
		gevent.sleep(0.1)
		res = self.test.getProperty(sound3_node_id, sound_port, sound_wuclass_id, sound_property_number)
        	if res == (binary_value == 1):
			break
	self.assertNotEqual(loop, TotalWaitTime - 1)
	self.test.deviceResume(DEVICE2)
	gevent.sleep(1)

    def test_one_block(self):        
	gevent.sleep(1)
        self.test.setLocation(3, "/WuKong/slider")
        self.test.setLocation(2, "/WuKong/binary")
        self.test.setLocation(4, "/WuKong/sound")
	gevent.sleep(2)
        nodes_info = self.test.discovery()
        self.test.loadApplication("applications/one_device_to_three_devices") 
    	self.test.mapping(nodes_info)
    	self.test.deploy_with_discovery()
	gevent.sleep(10)
	
	slider_node_id = 3
	slider_port = 2
	slider_wuclass_id = 15
	slider_property_number = 2
        slider_datatype = 'short'
        slider_value = 50
        self.test.setProperty(slider_node_id, slider_port, slider_wuclass_id, slider_property_number, slider_datatype, slider_value)

        binary_sensor_node_id = 2
        binary_sensor_port = 1
        binary_sensor_wuclass_id = 10
        binary_property_number = 1
        binary_datatype = 'boolean'
        binary_value = 1
        self.test.setProperty(binary_sensor_node_id, binary_sensor_port, binary_sensor_wuclass_id, binary_property_number, binary_datatype, binary_value)

        sound1_node_id = 2
        sound2_node_id = 3
        sound3_node_id = 4

        sound_port = 3
        sound_wuclass_id = 204
        sound_property_number = 0

	for i in xrange(TEST_PROPAGATE_STRENGTH_NUMBER):
		print "loop: %d" % i
		binary_value = random.randint (0,1)
		block_id = random.randint(1,2)
		block_device = TEST_DEVICES[block_id]
		self.test.deviceWait(block_device)
		print "\033[44m round %d: set to %d\033[44m" % (i, binary_value)
        	self.test.setProperty(binary_sensor_node_id, binary_sensor_port, binary_sensor_wuclass_id, binary_property_number, binary_datatype, binary_value)
	        for loop in xrange(0,TotalWaitTime):
        	        gevent.sleep(0.1)
               		res = self.test.getProperty(sound1_node_id, sound_port, sound_wuclass_id, sound_property_number)
                	if res == (binary_value == 1):
                        	break
	        self.assertNotEqual(loop, TotalWaitTime - 1)
		gevent.sleep(0.1)
		if block_id != 1:
			for loop in xrange(0,TotalWaitTime):
                        	gevent.sleep(0.1)
                        	res = self.test.getProperty(sound2_node_id, sound_port, sound_wuclass_id, sound_property_number)
                        	if res == (binary_value == 1):
                                	break
                	self.assertNotEqual(loop, TotalWaitTime - 1)
		if block_id != 2:
                        for loop in xrange(0,TotalWaitTime):
                                gevent.sleep(0.1)
                                res = self.test.getProperty(sound2_node_id, sound_port, sound_wuclass_id, sound_property_number)
                                if res == (binary_value == 1):
                                        break
                        self.assertNotEqual(loop, TotalWaitTime - 1)
		self.test.deviceResume(block_device)
		gevent.sleep(1)


if __name__ == '__main__':
	unittest.main()


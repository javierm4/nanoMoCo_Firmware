/*

Common Line Handler Library

OpenMoco nanoMoCo Core Engine Libraries 

See www.openmoco.org for more information

(c) 2008-2011 C.A. Church / Dynamic Perception LLC

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.


*/

#ifndef	OM_COMHANDLER_H
#define OM_COMHANDLER_H


#include <inttypes.h>

 // pins for COM lines on nanoMoCo Board
 
#define OM_COM1 2
#define OM_COM2 3
#define OM_COM3 19

/**

  @brief
  Common Line Handler
  
  The Common Line Handler class allows for handling of the three common
  lines designed for triggering different behaviors between nodes, and 
  for synchronizing timing between a 'master'-timing node and 'slave' nodes
  in a network of Motion Axis devices.
  
  This class handles the required functionalitty for both the slave and the
  master node.
  
  There must only be one instance of this class in existance at a time, you
  cannot use two instances of this class.
  
  
  Please note that OMComHandler uses Arduino pins 2, 3, and 19 (analog 5) - it 
  is NOT possible to change this!
  
  Master Node Example:
  
   @code
  
  // Master Node
  
  
  OMComHandler ComMgr;
  
  void setup() {
    ComMgr = OMComHandler();
    	// configure line handler as master
    ComMgr.master(true);
  }
  
  void loop() {
  
    // do something
    
    ..
    
    // trigger a complete signal
    ComMgr.masterSignal();
    
  }
  @endcode
  
  Slave Node Example:
  
  @code
  
  // Slave Node
  
  
  OMComHandler ComMgr;
  
  void setup() {
    ComMgr = OMComHandler();
    	// configure line handler as slave
    ComMgr.master(false);
  }
  
  void loop() {

    // received a complete signal
    if( ComMgr.slaveClear() == true ) {
  
      // do something
    
      ..
    }
    
    
  }
  @endcode
  
  @author C. A. Church

  (c) 2011 C. A. Church / Dynamic Perception
  
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

  */
  
class OMComHandler
{
	
private:
	
	bool m_isMaster;
	static bool s_isTripped;
	
	void _masterFollow();
	void _masterLead();
	static void _slaveTripped();
	
public:
	
	OMComHandler(); // constructor
	
	bool master();
	void master(bool);
	void masterSignal();
	static bool slaveClear();
	
	
	
};


#endif

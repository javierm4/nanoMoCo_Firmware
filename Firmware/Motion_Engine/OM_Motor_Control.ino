/*


Motion Engine

See dynamicperception.com for more information


(c) 2008-2012 C.A. Church / Dynamic Perception LLC

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


/* 

 =========================================
 Motor control functions
 =========================================
 
*/

const byte MT_COM_DIR1 = 50;
const byte MT_COM_DIR2 = 100;

void move_motor() {

    // do we revert back to "ready" or "waiting" if there
    // is nothing to do, or block for? - Based on master/slave status
    
   byte continue_state = (ComMgr.master() == true) ? ST_CLEAR : ST_BLOCK;
   
     // motor disabled? Do nothing.
   if( ! motor[0].enable() && !motor[1].enable() && !motor[2].enable() ) {
     Engine.state(continue_state);
     return;
   }
   
   for(int i = 0; i < MOTOR_COUNT; i++){
	   //only check the motors that are enable
	   if( motor[i].enable()){
		   //check to see if there's a shot delay for the motor
		   if (!(motor[i].planLeadIn() > 0 && ((camera_fired <= motor[i].planLeadIn() && motor[i].planType() == SMS) || (motor[i].planType() != SMS && run_time <= motor[i].planLeadIn())))){
				motor[i].programMove();
				if( motor[i].planType()  == SMS ) {
					// planned SMS move
					 //motor[i].planRun();
					// block camera while motor is moving
					Engine.state(ST_RUN);
				}
				else  {
					// planned continuous move
					Engine.state(continue_state);
				}
			}
		} 
   }
   //Start interrupt service routine to start motors moving
   startISR();
}


 // callback handler for OMComHandler class to handle trips on the common line 
 // (if we're watching a common line for movement trips)
 
void motor_com_line(unsigned int p_time) {
  if( p_time > MT_COM_DIR1 && p_time < MT_COM_DIR2 ){
    motor[0].move(0, 1);
	motor[1].move(0, 1);
	motor[2].move(0, 1);
  }
  else if( p_time > MT_COM_DIR2 ){
    motor[0].move(1, 1);
    motor[1].move(1, 1);
    motor[2].move(1, 1);
  }
}

/*
byte motorSleep()

Returns the sleep state of all motors, where the state of each motor is 
indicated by the boolean state of bits 0, 1, and 2 of the returned byte.

*/

byte motorSleep() {
	
	byte sleep_state = B000;

	for (byte i = 0; i < MOTOR_COUNT; i++) {
		if (motorSleep(i) == true)
			sleep_state |= (1 << i);
	}

	return sleep_state;
}

/*
void motorSleep(byte p_motor, bool p_sleep)

Sets and saves motor sleep state

p_motor: Which motor to set the sleep state (0,1,2)
p_sleep: Sleep state (true/false)

*/

void motorSleep(byte p_motor, bool p_sleep) {
	motor[p_motor].sleep(p_sleep);
	OMEEPROM::write(EE_SLEEP_0 + (p_motor * EE_MOTOR_MEMORY_SPACE), p_sleep);
}

/*
byte motorSleep(byte p_motor)

Returns the sleep state of the selected motor

p_motor: Which motor to query the sleep state of (0,1,2)

*/

byte motorSleep(byte p_motor) {
	return motor[p_motor].sleep();
}


/*
	void startProgramCom()

	Runs all pre-program checks after a start program command is received from a master device. 
	Actual motor movement is started by calling startProgram() at the end of this function.
*/

void startProgramCom() {
	key_move = false;
	bool was_pause = pause_flag;
	pause_flag = false;

	// Don't perform backlash checks or other start of move operations if the program was paused or is currently running
	if (!running && !was_pause) {

		// Reset the shot counter to 0. If the user presses the "Fire Camera" button in the joystick screen of the app, it may be a non-zero number.
		camera_fired = 0;

		// Reset the program completion flag
		program_complete = false;

		//USBSerial.println("Start Break 1");

		uint8_t wait_required = false;

		// Check each motor to see if it needs backlash compensation
		for (byte i = 0; i < MOTOR_COUNT; i++) {
			if (motor[i].programBackCheck() == true && motor[i].backlash() > 0) {

				// Indicate that a brief pause is necessary after starting the motors
				wait_required = true;

				// Set the motor microsteps to low resolution and increase speed for fastest takeup possible
				motor[i].ms(4);
				motor[i].contSpeed(MOT_DEFAULT_MAX_STEP);

				// Determine the direction of the programmed move
				uint8_t dir = (motor[i].stopPos() - motor[i].startPos()) > 0 ? 1 : 0;

				// Move the motor 1 step in that direction to force the backlash takeup
				motor[i].move(dir, 1);
				startISR();
			}
		}

		if (wait_required) {
			unsigned long time = millis();
			while (millis() - time < MILLIS_PER_SECOND){
				// Wait a second for backlash takeup to finish
			}
		}

		// Re-set all the motors to their proper microstep settings
		for (byte i = 0; i < MOTOR_COUNT; i++) {
			msAutoSet(i);

			// Print debug info if proper flag is set
			if (usb_debug & DB_FUNCT){
				USBSerial.print("Microsteps: ");
				USBSerial.println(motor[i].ms());
			}
		}

		// When starting an SMS move, if we're only making small moves, set each motor's speed no faster than necessary to produce the smoothest motion possible
		if (motor[1].planType() == SMS) {
			////USBSerial.println("Start Break 4");
			// Determine the max time in seconds allowed for moving the motors
			float max_move_time = (Camera.interval - Camera.triggerTime() - Camera.delayTime() - Camera.focusTime()) / MILLIS_PER_SECOND;
			// If there's lots of time for moving, only use 1 second so we don't waste battery life getting to the destination
			if (max_move_time > 0.5)
				max_move_time = 0.5;
			// Determine the maximum number of steps each motor needs to move. For short move, throttle the speed to avoid jerking of the rig.
			for (byte i = 0; i < MOTOR_COUNT; i++) {
				int steps_per_move = motor[i].getTopSpeed();
				if (steps_per_move < 500) {
					// Only use 50% of the maximum move time to allow for accel and decel phases
					motor[i].contSpeed((float)steps_per_move / (max_move_time * 0.5));
				}
			}
		}

		// If we're starting a video move, fire the camera trigger pin to start the video camera
		if (motor[0].planType() == CONT_VID) {
			Camera.expose();
			unsigned long time = millis();
			while (millis() - time < (MILLIS_PER_SECOND * 1.5))
			{
				// Wait a second and a half for the video to start before starting the move.
			}
		}
	}//end if (!running && !was_pause)

	// Don't start a new program if one is already running
	if (!running) {

		if (usb_debug & DB_GEN_SER){
			USBSerial.println("Motor distances:");
			for (byte i = 0; i < MOTOR_COUNT; i++){
				USBSerial.println(motor[i].stopPos() - motor[i].currentPos());
			}
			USBSerial.println("Motor start:");
			for (byte i = 0; i < MOTOR_COUNT; i++){
				USBSerial.println(motor[i].startPos());
			}
			USBSerial.println("Motor stop:");
			for (byte i = 0; i < MOTOR_COUNT; i++){
				USBSerial.println(motor[i].stopPos());
			}
			USBSerial.println("Motor current:");
			for (byte i = 0; i < MOTOR_COUNT; i++){
				USBSerial.println(motor[i].currentPos());
			}
			USBSerial.println("Motor travel:");
			for (byte i = 0; i < MOTOR_COUNT; i++){
				USBSerial.println(motor[i].planTravelLength());
			}
		}

		//if it was paused and not SMS then recalculate move from pause time
		if (was_pause && motor[0].planType() != SMS){
			for (byte i = 0; i < MOTOR_COUNT; i++){
				if (motor[i].enable())
					motor[i].resumeMove();
				//USBSerial.print("New Run Time: ");
				//USBSerial.println(motor[i].planTravelLength());
			}
		}

		startProgram();
	}
}

/*
byte validateProgram()

Checks all motors to see if they can achieve the speed required given the current program parameters.

*/

byte validateProgram() {

	byte validation = B00000000;

	// If a motor is valid, mark that bit true
	for (byte i = 0; i < MOTOR_COUNT; i++) {
		if (validateProgram(i) == true)
			validation |= (1 << i);
	}

	// Return byte indicating status of all three motors
	return validation;
}

/*
byte validateProgram(byte p_motor)

Checks specified motor to see if it can achieve the speed required given the current program parameters.

p_motor: Motor to check (0 through MOTOR_COUNT)

*/

byte validateProgram(byte p_motor) {
	return validateProgram(p_motor, false);
}


/*
byte validateProgram(byte p_motor, bool p_autosteps)

Checks specified motor to see if it can achieve the speed required given the current program parameters.

p_motor:		Motor to check (0 through MOTOR_COUNT)
p_autosteps:	Optional parameter. Function will return required microstep setting instead of true on success and 255 instead of false upon failure.

*/

byte validateProgram(byte p_motor, bool p_autosteps) {
	// The microstepping cutoff values below are in 16th steps
	const int MAX_CUTOFF = 16000;
	const int QUARTER_CUTOFF = 8000;
	const int EIGHTH_CUTOFF = 4000;
	float comparison_speed;
	
	// For time lapse SMS mode
	if (motor[p_motor].planType() == SMS) {

		// Max time in seconds
		float max_time_per_move = (float)(Camera.interval - Camera.delayTime() - Camera.triggerTime() - Camera.focusTime()) / MILLIS_PER_SECOND;


		// The "topSpeed" variable in SMS mode is actually the number of steps per move during the constant speed segment
		float steps_per_move = motor[p_motor].getTopSpeed();

		comparison_speed = steps_per_move / (float)max_time_per_move;

	}

	// For time lapse continuous mode and video continuous mode
	else if (motor[p_motor].planType() != SMS) {
		comparison_speed = motor[p_motor].getTopSpeed();
	}

	// USB print the debug value, if necessary
	if (usb_debug & DB_FUNCT){
		USBSerial.print("Top speed requested: ");
		USBSerial.println(comparison_speed);
	}

	// Check the comparison speed against the cutoff values and select the appropriate microstepping setting
	// If the requested speed is too high, send error value, don't change microstepping setting
	if (comparison_speed >= MAX_CUTOFF ) {
		//USBSerial.println("Excessive speed requested");
		return 0;
	}
	else {
		// Return the appropriate microstep value if called from msAutoSet(), otherwise return true for OK
		if (p_autosteps) {
			if (comparison_speed >= QUARTER_CUTOFF && comparison_speed < MAX_CUTOFF)
				return QUARTER;
			else if (comparison_speed < QUARTER_CUTOFF && comparison_speed > EIGHTH_CUTOFF)
				return EIGHTH;
			else
				return SIXTHEENTH;
		}
		else
			return 1;
	}

}

/*
byte msAutoSet(uint8_t p_motor_number)

Set the appropriate microstep value for the motor based upon currently set program move parameters.

p_motor_number: motor to modify microstepping

*/

byte msAutoSet(uint8_t p_motor) {

	unsigned long time = millis();
	byte microsteps;
	
	// Don't change the microstep value if the motor or program is running
	if (!running && !motor[p_motor].running()) {
		
		// Get program validation info
		microsteps = validateProgram(p_motor, true);

		// Return an error code of 255 if called from msAutoSet(), since a 0 error is given when the routine cannot be run
		if (microsteps == false)
			return 255;
		
		// Otherwise set the appropraite microstep setting and report the new value back to the master device
		else
			motor[p_motor].ms(microsteps);

		// Save the microstep settings
		OMEEPROM::write(EE_MS_0 + (p_motor * EE_MOTOR_MEMORY_SPACE), microsteps);

		// USB print the debug value, if necessary
		if (usb_debug & DB_GEN_SER){
			USBSerial.print("Requested Microsteps: ");
			USBSerial.println(microsteps);
			USBSerial.println("Microsteps successfully set");
		}
		return microsteps;
		
	}	

	// If the motor or program is running and a report is requested, return 0 to indicate that the auto-set routine was not completed
	else {
		if (usb_debug & DB_GEN_SER)
				USBSerial.println("Motors are running, can't auto-set microsteps");
			return false;
	}
}

/*
void joystickSet(byte p_input)

Sets joystick mode on or off. If turning off joystick mode, watchdog mode is also turned off automatically.

p_input: True or false setting.

*/

void joystickSet(byte p_input) {
	joystick_mode = p_input;

	if (usb_debug & DB_GEN_SER) {
		USBSerial.print("Joystick: ");
		USBSerial.println(joystick_mode);
	}

	// Set the speed of all motors to zero when turning on joystick mode to prevent runaway motors
	if (joystick_mode){
		for (byte i = 0; i < MOTOR_COUNT; i++) {
			motor[i].contSpeed(0);
		}
	}
	// If we're exiting joystick mode, turn off the joystick watchdog mode
	else if (!joystick_mode)
		watchdog = false;
}

/**
byte joystickSet()

Returns joystick mode status

*/

byte joystickSet() {
	return joystick_mode;
}


/**
void pingPongMode(byte p_input)

Sets ping-pong mode on or off.

p_input: True or false setting.

*/

void pingPongMode(byte p_input) {

	// Ignore non-true/false input
	if (p_input != 0 && p_input != 1)
		return;

	ping_pong_mode = p_input;
}


/**
byte pingPongMode()

Returns ping-pong mode status

*/

byte pingPongMode() {
	return ping_pong_mode;
}



      






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

boolean kf_okForSmsMove;
int kf_curSmsFrame;
boolean kf_auxFired = false;
boolean kf_auxDone = false;
boolean kf_focusFired = false;
boolean kf_focusDone = false;
boolean kf_shutterFired = false;
boolean kf_shutterDone = false;
boolean kf_forceShotInProgress = false;

void kf_printKeyFrameData(){

	// General program parameters
	USBSerial.print("~~~~~~ General Program Params ~~~~~~");
	USBSerial.println("");

	USBSerial.print("Max camera time: ");
	USBSerial.println(kf_getMaxCamTime());
	USBSerial.print("Max move time: ");
	USBSerial.println(kf_getMaxMoveTime());
	USBSerial.print("Run status: ");
	USBSerial.println(kf_getRunState());
	
	if (kf_getRunState != 0){
		USBSerial.print("Current run time: ");
		USBSerial.println(kf_getRunTime());
		USBSerial.print("Percent complete: ");
		USBSerial.println(kf_getPercentDone());		
	}
	USBSerial.print("Program mode: ");
	USBSerial.println(Motors::planType());
	USBSerial.print("Cont. video time: ");
	USBSerial.println(KeyFrames::getContVidTime());

	for (byte i = 0; i < KeyFrames::getAxisCount(); i++){
		// Indicate the current axis
		USBSerial.print("~~~~~~ AXIS ");
		USBSerial.print(i);
		USBSerial.println(" ~~~~~~");
		USBSerial.println("");

		// Print abscissas
		USBSerial.println("*** Abscissas ***");
		for (byte j = 0; j < kf[i].getKFCount(); j++){
			USBSerial.println(kf[i].getXN(j));
		}
		USBSerial.println("");

		// Print Positions
		USBSerial.println("*** Positions ***");
		for (byte j = 0; j < kf[i].getKFCount(); j++){
			USBSerial.println(kf[i].getFN(j));
		}
		USBSerial.println("");

		// Print velocities
		USBSerial.println("*** Velocities ***");
		for (byte j = 0; j < kf[i].getKFCount(); j++){
			USBSerial.println(kf[i].getDN(j));
		}
		USBSerial.println("");
		USBSerial.println("");
	}

}

void kf_startProgram(){
	kf_startProgram(false);
}

void kf_startProgram(boolean isBouncePass){
		
	// If resuming
	if (kf_paused){
		// Add the time of the last pause to the total pause time counter
		kf_pause_time += kf_this_pause;
	}
	// If starting a new program
	else{
		debug.funct("Starting new type ");
		debug.funct((int)Motors::planType());
		debug.functln(" KF program");		

		// Reset completion flag
		program_complete = false; 

		// Reset the SMS vars
		kf_okForSmsMove = false;
		kf_curSmsFrame = 0;

		// Reset the total pause time counter
		kf_pause_time = 0;

		// Make sure the pause flag is off
		kf_paused = false;		

		// Reset the camera vars
		kf_last_shot_tm = 0;		
		kf_auxFired = false;
		kf_auxDone = false;
		kf_focusFired = false;
		kf_focusDone = false;
		kf_shutterFired = false;
		kf_shutterDone = false;
		kf_forceShotInProgress = false;
		camera_fired = 0;

		// Reset ping pong vals, if necessary
		if (!isBouncePass){
			ping_pong_shots = 0;
			ping_pong_time = 0;
		}
				
		// Prep the movement and camera times
		kf_getMaxMoveTime();
		kf_getMaxCamTime();
		if (!isBouncePass)
			clearShotCounter();

		// SMS Moves
		if (Motors::planType() == SMS){
			// Convert from "frames" to real milliseconds, based upon the camera interval						
			Camera.intervalTime();			
			kf_getMaxMoveTime();
			kf_getMaxCamTime();
			// Make sure joystick mode is off, then set move speed for the motors
			joystickSet(false);
		}
		// Cont TL and Vid moves
		else{			
			// Turn on joystick mode
			joystickSet(true);

			// Set the initial motor speeds			
			for (byte i = 0; i < MOTOR_COUNT; i++){
				// Don't touch motors that don't have any key frames
				if (kf[i].getKFCount() > 0){					
					// If the first key frame isn't at x == 0 (i.e. there is a lead-in), set velocity to 0
					if (kf[i].getXN(0) == 0){
						setJoystickSpeed(i, kf[i].vel(0) * MILLIS_PER_SECOND);
					}
					else{
						setJoystickSpeed(i, 0);
					}
				}
			}
		}

		// Initialize the run timers
		kf_run_time = 0;
		kf_start_time = millis();
		kf_last_update = millis();		
	}

	// Turn on the key frame program flag and turn the paused flag off
	kf_running = true;
	kf_paused = false;
	kf_just_started = true;	
}

void kf_pauseProgram(){

	debug.funct("PAUSING KF PROGRAM");

	// Stop all motors
	for (byte i = 0; i < MOTOR_COUNT; i++){	
		debug.funct("Stopping motor ");
		debug.functln(i);	
		setJoystickSpeed(i, 0);
	}

	// Set the pause flag
	kf_paused = true;

	// Reset the current pause duration counter
	kf_this_pause = 0;

	// Log the start time of the pause
	kf_pause_start = millis();	
}

void kf_stopProgram(){
	kf_stopProgram(false);
}

void kf_stopProgram(boolean savePingPongVals){

	debug.funct("STOPPING KF PROGRAM");
	
	// Make sure all motors are stopped
	for (byte i = 0; i < MOTOR_COUNT; i++){
		setJoystickSpeed(i, 0);
	}

	// Disable joystick mode
	joystickSet(false);

	// Turn off the key frame program flag
	kf_running = false;
	kf_paused = false;	
	still_shooting_flag = false;

	if (!savePingPongVals){
		ping_pong_flag = false;
		kf_ping_pong_time = 0;
		ping_pong_shots = 0;
	}

	// If it's a video move, trigger the camera once to stop the recording
	if (Motors::planType() == CONT_VID)
		Camera.expose();
}

void kf_updateProgram(){
	unsigned long cur_time = millis();
	static unsigned long last_blink;
	const int BLINK_DELAY = 500;
	if (kf_run_time == 0){
		last_blink = cur_time;
	}

	// If the program is paused, just keep track of the pause time
	if (kf_paused){
		kf_this_pause = millis() - kf_pause_start;
		debug.funct("Pause length: ");
		debug.functln(kf_this_pause);		
		return;
	}

	// Update run_time, don't include time spent paused
	kf_run_time = millis() - kf_start_time - kf_pause_time;
	
	//debug.funct("Run time: ");
	//debug.functln(kf_run_time);	

	// Adding a small delay seems to keep the controller from randomly locking. I don't know why...
	int time_delay = 500;
	unsigned long  start_wait = micros();
	while (micros() - start_wait < time_delay){
		// Wait for the delay to finish
	}

	// Don't do anything else until the start delay is done (but skip if on a ping-pong pass)
	if (kf_run_time < start_delay && !ping_pong_flag){
		delay_flag = true;
		if (cur_time - last_blink > BLINK_DELAY){
			debugToggle();
			last_blink = cur_time;
		}
		return;
	}

	// Set proper post-delay debug LED status
	if (external_intervalometer)
		debugOn();
	else
		debugOff();

	delay_flag = false;
		

	// If this is the beginning of the program, just after the start delay...
	if (kf_just_started){
		// ...trigger the camera once to start the recording if it's a video move.
		if (Motors::planType() == CONT_VID)
			Camera.expose();
		kf_just_started = false;
	}

	
	// Continuous move update (don't update in keep-alive phase)
	if (Motors::planType() != SMS && !still_shooting_flag){
		kf_updateContSpeed();
	}

	// Check whether the camera needs to fire (but not for video mode)
	if (Motors::planType() != CONT_VID)
		kf_CameraCheck();

	// SMS move update (don't update in keep-alive phase)
	if (Motors::planType() == SMS && !still_shooting_flag){
		kf_updateSMS();
	}

	// Check to see if the program is done
	long totalTime = kf_getMaxCamTime();
	// Only wait for the start delay on the first pass
	if (!ping_pong_flag)
		totalTime += start_delay;
	if (kf_run_time > totalTime){
		// Make sure the motors are stopped, but let the program continue running
		if (keep_camera_alive){
			debug.serln("Starting keep alive phase");
			still_shooting_flag = true;

			// Make sure all motors are stopped
			for (byte i = 0; i < MOTOR_COUNT; i++){
				setJoystickSpeed(i, 0);
			}

			// Disable joystick mode
			joystickSet(false);
		}
		else{
			debug.serln("Stopping kf program");									
			// If ping-pong mode is active, reverse and start a new program
			if (pingPongMode()){
				// The shot count and run time are reset when starting a new program, 
				// so add the shots and them to separate counters for serial reporting
				// and indicate to the stop function that they should not be reset
				kf_ping_pong_time += kf_run_time;
				ping_pong_shots += camera_fired;
				kf_stopProgram(true);
				debug.serln("Starting ping-pong phase");
				ping_pong_flag = true;
				debug.serln("Reversing key points");
				reverseStartStop();
				debug.serln("Starting new kf bounce program");
				kf_startProgram(true);			
			}
			else{
				kf_stopProgram();
				program_complete = true;
			}
		}		
	}
}

void kf_updateContSpeed(){

	// If the update time has elapsed, update the motor speed
	if (millis() - kf_last_update > KeyFrames::updateRate()){
		for (byte i = 0; i < MOTOR_COUNT; i++){
			// Determine the maximum run time for this axis
			float thisAxisMaxTime = kf[i].getXN(kf[i].getKFCount() - 1);
			if (Motors::planType() == SMS)
				thisAxisMaxTime = thisAxisMaxTime * Camera.intervalTime();

			// Set the approriate speed, but don't touch motors that don't have any key frames
			if (kf[i].getKFCount() > 0){
				float speed;
				if (kf_run_time > thisAxisMaxTime + start_delay)
					speed = 0;
				else{
					// If the time is before the first key frame or after the last, it's a lead-in/out and speed should be 0
					if (kf_run_time < kf[i].getXN(0) + start_delay || kf_run_time > kf[i].getXN(kf[i].getKFCount() - 1) + start_delay)
						speed = 0;
					else
						speed = kf[i].vel((float)kf_run_time - start_delay) * MILLIS_PER_SECOND; // Convert from steps/millisecond to steps/sec
				}					
				setJoystickSpeed(i, speed);
			}
		}
		kf_last_update = millis();
	}
}

void kf_updateSMS(){
	
	// If we're not ready for a move (i.e. the camera is busy or we just finished one), don't do anything
	if (!kf_okForSmsMove || kf_run_time > kf_getMaxMoveTime() + start_delay){
		return;
	}

	// Send the motors to their next locations
	debug.functln("Sending motors to new locations");
	for (int i = 0; i < MOTOR_COUNT; i++){		

		// Make sure there is a point to actually query
		if (kf[i].getKFCount() < 2 || kf_curSmsFrame + 1 > kf[i].getXN(kf[i].getKFCount() - 1))
			continue;

		float nextPos = kf[i].pos(kf_curSmsFrame + 1);
	
		debug.funct("About to send to location #: ");
		debug.functln(kf_curSmsFrame + 1);
		debug.funct("Sending to ");
		debug.functln(nextPos);
		debug.functln("");
	
		sendTo(i, (long)nextPos);				
	}		
	kf_curSmsFrame++;
	kf_okForSmsMove = false;
}

void kf_CameraCheck() {

	const String KF_CAM_CHECK = "kf_CameraCheck() - ";

	int auxPreShotTime = 0;

	// If this is the last shot of a pass during ping-pong mode, skip it
	if (pingPongMode() && camera_fired == Camera.getMaxShots()){
		return;
	}

	// If in external interval mode, don't do anything if a force shot isn't registered
	if (altExtInt && !altForceShot && !kf_forceShotInProgress) {		
		debug.functln(KF_CAM_CHECK + "Skipping shot, waiting for external trigger");		
		return;
	}

	// If there's an aux event before the camera events, set the time needed for that
	if (ALT_OUT_BEFORE == altInputs[0] || ALT_OUT_BEFORE == altInputs[1]){
		auxPreShotTime = altBeforeDelay;
	}

	boolean auxLongerThanInt = auxPreShotTime >= Camera.intervalTime();
	boolean timeForNewShot = (kf_run_time - kf_last_shot_tm) >= (Camera.intervalTime() - auxPreShotTime);

	// If it's time, start a new shot
	if (((auxLongerThanInt || timeForNewShot) && !altBlock) || altForceShot){  //Camera.intervalTime() is less than the altBeforeDelay, go as fast as possible
		
		kf_last_shot_tm = kf_run_time;
		
		kf_auxFired = false;
		kf_auxDone = false;
		kf_focusFired = false;
		kf_focusDone = false;
		kf_shutterFired = false;
		kf_shutterDone = false;
		
		if (altForceShot){
			kf_forceShotInProgress = true;
			altForceShot = false;
		}
	}	

	// Trigger any outputs that need to go before the exposure
	if ((ALT_OUT_BEFORE == altInputs[0] || ALT_OUT_BEFORE == altInputs[1]) && cycleShotOK(true) && !kf_auxDone) {
		if (!kf_auxFired){
			altBlock = ALT_OUT_BEFORE;
			altOutStart(ALT_OUT_BEFORE);
			debug.functln(KF_CAM_CHECK + "Bailing from camera cycle at point 2");
			kf_auxFired = true;
			return;
		}
		
		// If not enough time for the aux function has elapsed, return
		if (kf_run_time < kf_last_shot_tm + auxPreShotTime){
			return;
		}
		kf_auxDone = true;
	}

	altBlock = ALT_OFF;
	altForceShot = false;
		
	// Trigger the focus
	if (!kf_focusDone){
		if (!kf_focusFired){
			debug.funct("Time at focus: ");
			debug.functln(kf_run_time);			
			Camera.focus();
			kf_focusFired = true;
			return;
		}

		// If not enough time for the focus has elapsed, return
		if (kf_run_time < kf_last_shot_tm + auxPreShotTime + Camera.focusTime()){						
			return;
		}
		kf_focusDone = true;
	}
	
	// Trigger the exposure
	if (!kf_shutterDone){
		if (!kf_shutterFired){
			debug.funct("Time at exposure: ");
			debug.functln(kf_run_time);
			Camera.expose();
			kf_shutterFired = true;
		}

		// If not enough time for the exposure has elapsed, return
		if (kf_run_time < kf_last_shot_tm + auxPreShotTime + Camera.focusTime() + Camera.triggerTime() + Camera.delayTime()){						
			return;
		}		
		kf_shutterDone = true;
		kf_forceShotInProgress = false;

		// One the camera functions are complete, it's okay to make the next SMS move		
		debug.functln("Shutter done, ready for move");		
		kf_okForSmsMove = true;
	}

}

void kf_auxCycleStart(byte p_mode) {

	//Pins for the I/O
	const byte           AUX_RING = 24;				//PD0
	const byte            AUX_TIP = 25;				//PD1

	uint8_t altStarted = false;

	unsigned int adelay = p_mode == ALT_OUT_BEFORE ? altBeforeMs : altAfterMs;

	for (byte i = 0; i < 2; i++) {
		if (p_mode == altInputs[i]) {

			// note that alt 3 is on a different register..
			if (i == 0)
				digitalWrite(AUX_RING, altOutTrig);
			else
				digitalWrite(AUX_TIP, altOutTrig);

			altStarted = true;
		}
	}

	if (altStarted) {
		MsTimer2::set(adelay, altOutStop);
		MsTimer2::start();
		Engine.state(ST_BLOCK);
	}
	else if (p_mode == ALT_OUT_BEFORE) {
		//clear to shoot
		Engine.state(ST_CLEAR);
	}
	else {
		// clear to move
		Engine.state(ST_MOVE);
	}

}

/*
	This function compares the max SMS speeds for each
	axis against the maximum possible speed for each axis.
	If at least one of them exceeds the axis' max speed,
	the function returns false.
*/
boolean kf_ValidateSMSProgram(){
	for (byte i = 0; i < MOTOR_COUNT; i++){
		float thisMaxSpeed = kf_MaxSMSSpeed(i);

		// Return false if one of the segments requires a speed higher than is possible
		if (thisMaxSpeed > motor[i].maxSpeed()){
			return false;
		}
	}
	// If everything checks out, return true
	return true;
}

/*
	This function finds the maximum steps per second required 
	for the requested axis' SMS key frame program. Only use if  
	an SMS key frame program has already been set!
*/
float kf_MaxSMSSpeed(int axis){
		
	float maxTime = kf[axis].getXN(kf[axis].getKFCount() - 1);
	int kfCount = kf[axis].getKFCount();
	float timeInc = maxTime / kfCount;
	float maxSpeed = 0;

	// For each key frame for that axis
	for (byte i = 0; i < kfCount; i++){

		// Find step difference for this segment
		float startTime = timeInc * i;
		float stopTime = timeInc * (i + 1);
		float startStep = kf[axis].pos(startTime);
		float stopStep = kf[axis].pos(stopTime);

		float stepsPerSec = (stopStep - startStep) / timeInc;

		if (stepsPerSec > maxSpeed)

		// Return false if one of the segments requires a speed higher than is possible
		if (stepsPerSec > maxSpeed){
			maxSpeed = stepsPerSec;
		}
	}
	return maxSpeed;
}

long kf_getMaxMoveTime(){

	long move_time = 0;	
	
	// SMS and cont. TL mode 
	if (Motors::planType() != CONT_VID){			
		move_time = Camera.getMaxShots() * Camera.intervalTime();
		if (!kf_running)
			debug.funct("Getting TL move time: ");			
	}
	// Continuous video mode
	else{			
		move_time = KeyFrames::getContVidTime();			
		if (!kf_running)
			debug.funct("Getting vid move time: ");			
	}		
	if (!kf_running){
		debug.funct(move_time);
		debug.functln("ms");
	}
	
	return move_time;
}


int kf_getRunState(){

	const int STOPPED = 0;
	const int RUNNING = 1;
	const int PAUSED = 2;	

	int ret;
	if (!kf_running){
		ret = STOPPED;
	}
	else if (kf_running && !kf_paused){
		ret = RUNNING;
	}
	// This is weird: if I don't explicitly compare the kf_paused value in the else if below, I get the
	// following build error: Motion_Engine.ino:12: error: expected unqualified-id before 'else'
	// Strange...
	else if (kf_running && kf_paused ==  true){ 
		ret = PAUSED;
	}

	return ret;
}


long kf_getRunTime(){
	return kf_run_time + kf_ping_pong_time;
}


long kf_getMaxProgramTime(){
	if (Motors::planType() == CONT_VID)
		return kf_getMaxMoveTime() + start_delay;
	else
		return kf_getMaxCamTime() + start_delay;
}


long kf_getMaxCamTime(){
	long ret = kf_getMaxMoveTime() + Camera.focusTime() + Camera.triggerTime();
	return ret;
}


int kf_getPercentDone(){
	int ret;
	
	if (Motors::planType() == CONT_VID)
		ret = ((float)kf_run_time / (kf_getMaxMoveTime() + start_delay)) * PERCENT_CONVERT;
	else
		ret = ((float)kf_run_time / (kf_getMaxCamTime() + start_delay)) * PERCENT_CONVERT;

	return ret;
}

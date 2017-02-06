/*
 Name:		Drone_Flight_Controller.ino
 Created:	11/10/2016 8:56:34 PM
 Author:	SIDUS
 Description: This is the main code for Drone_Flight_Controller Project
*/


//Use ledc functions for motor pwm usage
#include "esp32-hal-ledc.h"

//Local include class and files
#include "Local_Agenda.h"
#include "Config.h"
#include "cMsgCoWorkerTx.h"
#include "cMsgT01.h"
#include "cMsgR01.h"
#include "cMsgUdpT01.h"
#include "cSerialParse.h"
#include "Local_PID_v1.h"
#include "cRxFilter.h"


//Global Class Definitions
Agenda scheduler;
cMsgT01 MsgT01;
cMsgR01 MsgR01;
cSerialParse serialParse(sizeof(MsgT01.message), MsgT01.message.startChar1, MsgT01.message.startChar2, MsgT01.message.endChar);
//HardwareSerial Serial2(2);

PID pidRatePitch(&pidVars.ratePitch.sensedVal, &pidVars.ratePitch.output, &pidVars.ratePitch.setpoint);
PID pidRateRoll(&pidVars.rateRoll.sensedVal, &pidVars.rateRoll.output, &pidVars.rateRoll.setpoint);

cRxFilter filterRxThr, filterRxPitch, filterRxRoll, filterRxYaw;

int hataCount = 0;
// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(921600);
	//Serial2.begin(921600);
	
	
	//Configure all PINs
	pinMode(PIN_LED, OUTPUT);
	pinMode(PIN_RX_THR, INPUT);
	pinMode(PIN_RX_PITCH, INPUT);
	pinMode(PIN_RX_ROLL, INPUT);
	pinMode(PIN_RX_YAW, INPUT);
	
	setupMotorPins();

	attachInterrupt(PIN_RX_THR, isrTHR, CHANGE);
	attachInterrupt(PIN_RX_PITCH, isrPITCH, CHANGE);
	attachInterrupt(PIN_RX_ROLL, isrROLL, CHANGE);
	attachInterrupt(PIN_RX_YAW, isrYAW, CHANGE);

	//Insert all tasks into scheduler
	scheduler.insert(test_task, 1000000);
	scheduler.insert(mapRxDCtoCmd, 20000);
	scheduler.insert(serialCheck, 19000);
	scheduler.insert(serialTransmit, 18000);
	scheduler.insert(processPID, 20000);
	scheduler.insert(runMotors, 20000);
	scheduler.insert(checkRxStatus, 1000000);
	scheduler.insert(checkMode, 310000);

	initVariables();
	initPIDtuning();


}
// the loop function runs over and over again until power down or reset
void loop() {
	//Just call scheduler update and let it do all the process
	scheduler.update();	
}

void initVariables()
{
	modeQuad = modeQuadSAFE;
	statusRx = statusType_NotInitiated;

	startTime_Thr = micros();
	startTime_Pitch = micros();
	startTime_Roll = micros();
	startTime_Yaw = micros();

	//PID related variable initializations
	pidVars.ratePitch.Kp = PID_RATE_PITCH_KP;
	pidVars.ratePitch.Ki = PID_RATE_PITCH_KI;
	pidVars.ratePitch.Kd = PID_RATE_PITCH_KD;
	pidVars.ratePitch.outputLimitMin = PID_RATE_PITCH_OUTMIN;
	pidVars.ratePitch.outputLimitMax = PID_RATE_PITCH_OUTMAX;
	pidVars.ratePitch.f1 = 0.5;
	pidVars.ratePitch.f2 = 0.5;

	pidVars.rateRoll.Kp = PID_RATE_ROLL_KP;
	pidVars.rateRoll.Ki = PID_RATE_ROLL_KI;
	pidVars.rateRoll.Kd = PID_RATE_ROLL_KD;
	pidVars.rateRoll.outputLimitMin = PID_RATE_ROLL_OUTMIN;
	pidVars.rateRoll.outputLimitMax = PID_RATE_ROLL_OUTMAX;
	pidVars.rateRoll.f1 = 0.5;
	pidVars.rateRoll.f2 = 0.5;

}
void initPIDtuning()
{
	pidRatePitch.SetOutputLimits(pidVars.ratePitch.outputLimitMin, pidVars.ratePitch.outputLimitMax);
	pidRatePitch.SetTunings(pidVars.ratePitch.Kp, pidVars.ratePitch.Ki, pidVars.ratePitch.Kd);
	pidRatePitch.SetF1(pidVars.ratePitch.f1);
	pidRatePitch.SetF2(pidVars.ratePitch.f2);

	pidRateRoll.SetOutputLimits(pidVars.rateRoll.outputLimitMin, pidVars.rateRoll.outputLimitMax);
	pidRateRoll.SetTunings(pidVars.rateRoll.Kp, pidVars.rateRoll.Ki, pidVars.rateRoll.Kd);
	pidRateRoll.SetF1(pidVars.rateRoll.f1);
	pidRateRoll.SetF2(pidVars.rateRoll.f2);

}

void setupMotorPins()
{
	ledcSetup(M_FL_CHANNEL, PWM_FREQ, PWM_DEPTH);
	ledcSetup(M_FR_CHANNEL, PWM_FREQ, PWM_DEPTH);
	ledcSetup(M_BR_CHANNEL, PWM_FREQ, PWM_DEPTH);
	ledcSetup(M_BL_CHANNEL, PWM_FREQ, PWM_DEPTH);
	ledcAttachPin(PIN_M_FL, M_FL_CHANNEL);
	ledcAttachPin(PIN_M_FR, M_FR_CHANNEL);
	ledcAttachPin(PIN_M_BR, M_BR_CHANNEL);
	ledcAttachPin(PIN_M_BL, M_BL_CHANNEL);
}

void pwmMicroSeconds(int _pwm_channel, int _microseconds)
{
	ledcWrite(_pwm_channel, _microseconds*PWM_MICROSECONDS_TO_BITS);
}

float mapping(float input, float inputMin, float inputMax, float outputMin, float outputMax)
{
	float result = 0;

	if (input <= inputMin)
	{
		result = outputMin;
	}
	else if (input >= inputMax)
	{
		result = outputMax;
	}
	else
	{
		result = outputMin + (input - inputMin) / (inputMax - inputMin) * (outputMax - outputMin);
	}
	return result;
}
//Interrupt Service Routine Functions, used for rx pwm inputs
void isrTHR()
{
	if (digitalRead(PIN_RX_THR) == HIGH)
	{
		startTime_Thr = micros();
	}
	else
	{
		if(micros() - startTime_Thr < RX_MAX_PULSE_WIDTH)
			dutyCycle_Thr = micros() - startTime_Thr;
		rxLastDataTime = millis();  //we need to define this for each isr in order to fully get status of rx
	}
}

void isrPITCH()
{
	if (digitalRead(PIN_RX_PITCH) == HIGH)
	{
		startTime_Pitch = micros();
	}
	else
	{
		if (micros() - startTime_Pitch < RX_MAX_PULSE_WIDTH)
			dutyCycle_Pitch = micros() - startTime_Pitch;
	}
}
void isrROLL()
{
	if (digitalRead(PIN_RX_ROLL) == HIGH)
	{
		startTime_Roll = micros();
	}
	else
	{
		if (micros() - startTime_Roll < RX_MAX_PULSE_WIDTH)
			dutyCycle_Roll = micros() - startTime_Roll;
	}
}
void isrYAW()
{
	if (digitalRead(PIN_RX_YAW) == HIGH)
	{
		startTime_Yaw = micros();
	}
	else
	{
		if (micros() - startTime_Yaw < RX_MAX_PULSE_WIDTH)
			dutyCycle_Yaw = micros() - startTime_Yaw;
	}
}

void test_task()
{
	test_task_counter++;
	if (test_task_counter % 2 == 0)
	{
		digitalWrite(PIN_LED, HIGH);
		
		
		//Serial.print("Mpu Pitch:");
		//Serial.print(MsgT01.message.coWorkerTxPacket.mpuPitch*180/M_PI);
		//Serial.print("   Compass Hdg:");
		//Serial.print(MsgT01.message.coWorkerTxPacket.compassHdg*180/M_PI);
		//Serial.print("   Baro Alt:");
		//Serial.print(MsgT01.message.coWorkerTxPacket.baroAlt);
		//Serial.print("   Baro Temp:");
		//Serial.println(MsgT01.message.coWorkerTxPacket.baroTemp);
		
		
		/*
		Serial.print("Thr:");
		Serial.print(dutyCycle_Thr);
		Serial.print("   Pitch:");
		Serial.print(dutyCycle_Pitch);
		Serial.print("   Roll:");
		Serial.print(dutyCycle_Roll);
		Serial.print("   Yaw:");
		Serial.println(dutyCycle_Yaw);
		*/
		/*
		Serial.print("Thr:");
		Serial.print(MsgR01.message.rxThrottle);
		Serial.print("   Pitch");
		Serial.println(MsgR01.message.rxPitch);
		*/
		
		//Serial.print("Thr:");
		//Serial.print(cmdThr);
		//Serial.print("   Pitch:");
		//Serial.print(cmdPitch);
		//Serial.print("   Roll:");
		//Serial.print(cmdRoll);
		//Serial.print("   Yaw:");
		//Serial.println(cmdYaw);
		


	}
	else
	{
		digitalWrite(PIN_LED, LOW);
	}
	/*
	if (abs(cmdThr - 1200) > 10)
	{
		Serial.println("HATA!!");
	}
	Serial.println("");
	Serial.println(hataCount);
	Serial.println(cmdThr);
	*/
}

void serialCheck()
{
	int numberofbytes = Serial.available();
	if (numberofbytes > 0)
	{
		//If available number of bytes is less than our buffer size, normal case
		if (numberofbytes <= sizeof(MsgT01.message) * 3)
		{
			unsigned char buffer[sizeof(MsgT01.message) * 3];
			Serial.readBytes(buffer, numberofbytes);
			serialParse.Push(buffer, numberofbytes);
			if (serialParse.getParsedData(MsgT01.dataBytes, sizeof(MsgT01.message)))
			{
				MsgT01.setPacket();
				updateMessageVariables();
			}
		}
		//Else if buffer overflow, abnormal case
		else
		{
			//Just read it
			unsigned char buffer[sizeof(MsgT01.message) * 3];
			Serial.readBytes(buffer, sizeof(MsgT01.message) * 3);
		}
	}
}

void serialTransmit()
{
	MsgR01.message.modeQuad = modeQuad;
	MsgR01.message.statusRx = statusRx;

	MsgR01.message.rxThrottle = cmdThr;
	MsgR01.message.rxPitch = cmdPitch;
	MsgR01.message.rxRoll = cmdRoll;
	MsgR01.message.rxYaw = cmdYaw;

	MsgR01.message.pidRatePitchKp = pidVars.ratePitch.Kp * 100;
	MsgR01.message.pidRatePitchKi = pidVars.ratePitch.Ki * 1000;
	MsgR01.message.pidRatePitchKd = pidVars.ratePitch.Kd * 1000;	
	
	MsgR01.message.pidRatePitchOutput = pidVars.ratePitch.output;
	MsgR01.message.pidRatePitchPresult = pidRatePitch.Get_P_Result();
	MsgR01.message.pidRatePitchIresult = pidRatePitch.Get_I_Result();
	MsgR01.message.pidRatePitchDresult = pidRatePitch.Get_D_Result();

	MsgR01.message.pidRatePitchF1 = pidVars.ratePitch.f1 * 100;
	MsgR01.message.pidRatePitchF2 = pidVars.ratePitch.f2 * 100;

	MsgR01.getPacket();
	Serial.write(MsgR01.dataBytes, sizeof(MsgR01.dataBytes));

}

void updateMessageVariables()
{

	/// !!! Do not forget to add multiplication to following identities with the correct resolution value

	switch (MsgT01.message.udpT01RelayPacket.pidCommandState)
	{
	case pidCommandApplyRatePitch:
		pidVars.ratePitch.Kp = MsgT01.message.udpT01RelayPacket.pidRatePitchKp / 100.0;
		pidVars.ratePitch.Ki = MsgT01.message.udpT01RelayPacket.pidRatePitchKi / 1000.0;
		pidVars.ratePitch.Kd = MsgT01.message.udpT01RelayPacket.pidRatePitchKd / 1000.0;
		pidVars.ratePitch.f1 = MsgT01.message.udpT01RelayPacket.pidRatePitchF1 / 100.0;
		pidVars.ratePitch.f2 = MsgT01.message.udpT01RelayPacket.pidRatePitchF2 / 100.0;
		pidRatePitch.SetTunings(pidVars.ratePitch.Kp, pidVars.ratePitch.Ki, pidVars.ratePitch.Kd);
		pidRatePitch.SetF1(pidVars.ratePitch.f1);
		pidRatePitch.SetF2(pidVars.ratePitch.f2);
		//Serial.println("PID Rate Pitch variables updated");
		break;

	//case pidCommandApplyRateRoll:
	//	pidVars.rateRoll.Kp = MsgT01.message.udpT01RelayPacket.pidRateRollKp;
	//	pidVars.rateRoll.Ki = MsgT01.message.udpT01RelayPacket.pidRateRollKi;
	//	pidVars.rateRoll.Kd = MsgT01.message.udpT01RelayPacket.pidRateRollKd;
	//	pidRateRoll.SetTunings(pidVars.rateRoll.Kp, pidVars.rateRoll.Ki, pidVars.rateRoll.Kd);
	//	break;
	//case pidCommandApplyRateYaw:
	//	pidVars.rateYaw.Kp = MsgT01.message.udpT01RelayPacket.pidRateYawKp;
	//	pidVars.rateYaw.Ki = MsgT01.message.udpT01RelayPacket.pidRateYawKi;
	//	pidVars.rateYaw.Kd = MsgT01.message.udpT01RelayPacket.pidRateYawKd;
	//	//pidRateYaw.SetTunings(pidVars.rateYaw.Kp, pidVars.rateYaw.Ki, pidVars.rateYaw.Kd);
	//	break;
	//case pidCommandApplyAnglePitch:
	//	pidVars.anglePitch.Kp = MsgT01.message.udpT01RelayPacket.pidAnglePitchKp;
	//	pidVars.anglePitch.Ki = MsgT01.message.udpT01RelayPacket.pidAnglePitchKi;
	//	pidVars.anglePitch.Kd = MsgT01.message.udpT01RelayPacket.pidAnglePitchKd;
	//	//pidAnglePitch.SetTunings(pidVars.anglePitch.Kp, pidVars.anglePitch.Ki, pidVars.anglePitch.Kd);
	//	break;
	//case pidCommandApplyAngleRoll:
	//	pidVars.angleRoll.Kp = MsgT01.message.udpT01RelayPacket.pidAngleRollKp;
	//	pidVars.angleRoll.Ki = MsgT01.message.udpT01RelayPacket.pidAngleRollKi;
	//	pidVars.angleRoll.Kd = MsgT01.message.udpT01RelayPacket.pidAngleRollKd;
	//	//pidAngleRoll.SetTunings(pidVars.angleRoll.Kp, pidVars.angleRoll.Ki, pidVars.angleRoll.Kd);
	//	break;
	//case pidCommandApplyAngleYaw:
	//	pidVars.angleYaw.Kp = MsgT01.message.udpT01RelayPacket.pidAngleYawKp;
	//	pidVars.angleYaw.Ki = MsgT01.message.udpT01RelayPacket.pidAngleYawKi;
	//	pidVars.angleYaw.Kd = MsgT01.message.udpT01RelayPacket.pidAngleYawKd;
	//	//pidAngleYaw.SetTunings(pidVars.angleYaw.Kp, pidVars.angleYaw.Ki, pidVars.angleYaw.Kd);
	//	break;
	default:break;

	}

}

void runMotors()
{

	if (modeQuad == modeQuadARMED)
	{
		if (cmdThr > CMD_THR_ARM_START)
		{
			pwmMicroSeconds(M_FL_CHANNEL, cmdThr + pidVars.ratePitch.output);
			pwmMicroSeconds(M_FR_CHANNEL, cmdThr + pidVars.ratePitch.output);
			pwmMicroSeconds(M_BR_CHANNEL, cmdThr - pidVars.ratePitch.output);
			pwmMicroSeconds(M_BL_CHANNEL, cmdThr - pidVars.ratePitch.output);
		}
		else
		{
			pwmMicroSeconds(M_FL_CHANNEL, CMD_THR_MIN);
			pwmMicroSeconds(M_FR_CHANNEL, CMD_THR_MIN);
			pwmMicroSeconds(M_BR_CHANNEL, CMD_THR_MIN);
			pwmMicroSeconds(M_BL_CHANNEL, CMD_THR_MIN);
		}
	}
	else if (modeQuad == modeQuadDirCmd)
	{ 
		pwmMicroSeconds(M_FL_CHANNEL, cmdThr);
		pwmMicroSeconds(M_FR_CHANNEL, cmdThr);
		pwmMicroSeconds(M_BR_CHANNEL, cmdThr);
		pwmMicroSeconds(M_BL_CHANNEL, cmdThr);
	}
	else
	{
		pwmMicroSeconds(M_FL_CHANNEL, CMD_THR_MIN);
		pwmMicroSeconds(M_FR_CHANNEL, CMD_THR_MIN);
		pwmMicroSeconds(M_BR_CHANNEL, CMD_THR_MIN);
		pwmMicroSeconds(M_BL_CHANNEL, CMD_THR_MIN);
	}
}

void checkRxStatus()
{
	if (millis() - rxLastDataTime > RX_DATATIME_THESHOLD)
	{
		statusRx = statusType_Fail;
	}
	else
	{
		statusRx = statusType_Normal;
	}
}

void mapRxDCtoCmd()
{
	if (statusRx == statusType_Normal)
	{
		cmdPitch = mapping(filterRxPitch.process(dutyCycle_Pitch), DC_PITCH_MIN, DC_PITCH_MAX, CMD_PITCH_MIN, CMD_PITCH_MAX);
		cmdRoll = mapping(filterRxRoll.process(dutyCycle_Roll), DC_ROLL_MIN, DC_ROLL_MAX, CMD_ROLL_MIN, CMD_ROLL_MAX);
		cmdYaw = mapping(filterRxYaw.process(dutyCycle_Yaw), DC_YAW_MIN, DC_YAW_MAX, CMD_YAW_MIN, CMD_YAW_MAX);
		cmdThr = mapping(filterRxThr.process(dutyCycle_Thr), DC_THR_MIN, DC_THR_MAX, CMD_THR_MIN, CMD_THR_MAX);
	}
	else
	{
		cmdPitch = 0;
		cmdRoll = 0;
		cmdYaw = 0;
		cmdThr = CMD_THR_MIN;
	}
}

void checkMode()
{

	if (statusRx != statusType_Normal)
	{
		modeQuad = modeQuadSAFE;
		cmdPitch = 0;
		cmdRoll = 0;
		cmdYaw = 0;
		cmdThr = CMD_THR_MIN;
		//Serial.println("QUAD is in SAFE Mode Check Rx!");
		return;
	}

	if (cmdThr < CMD_THR_MIN + 50 && cmdYaw<CMD_YAW_MIN + 10 && cmdRoll > CMD_ROLL_MAX - 10 && cmdPitch>CMD_PITCH_MAX - 10)
	{
		modeQuad = modeQuadARMED;
		//Serial.println("QUAD is ARMED");
	}
	else if (cmdThr < CMD_THR_MIN + 50 && cmdYaw < CMD_YAW_MIN + 10 && cmdRoll < CMD_ROLL_MIN + 10 && cmdPitch>CMD_PITCH_MAX - 10)
	{
		modeQuad = modeQuadSAFE;
		//Serial.println("QUAD is in SAFE Mode");
	}
	else if (cmdThr < CMD_THR_MIN + 50 && cmdYaw < CMD_YAW_MIN + 10 && cmdRoll < CMD_ROLL_MIN + 10 && cmdPitch<CMD_PITCH_MIN + 10)
	{
		//Left for spare usage
	}
	else if (cmdThr < CMD_THR_MIN + 50 && cmdYaw < CMD_YAW_MIN + 10 && cmdRoll>CMD_ROLL_MAX - 10 && cmdPitch<CMD_PITCH_MIN + 10)
	{
		modeQuad = modeQuadDirCmd;
	}
}

void processPID()
{

	pidVars.ratePitch.setpoint = cmdPitch;
	pidVars.ratePitch.sensedVal = MsgT01.message.coWorkerTxPacket.mpuGyroY;  //no need to change LSB to deg/sec
	pidRatePitch.Compute();

	
	//Serial.print(cmdPitch);
	//Serial.print("  P:");
	//Serial.print(pidRatePitch.Get_P_Result());
	//Serial.print("  I:");
	//Serial.print(pidRatePitch.Get_I_Result());
	//Serial.print("  D:");
	//Serial.print(pidRatePitch.Get_D_Result());
	//Serial.print("  ");
	//Serial.println(pidVars.ratePitch.output);
	
	//pidVars.rateRoll.sensedVal = MsgT01.message.coWorkerTxPacket.mpuGyroX / MPU_GYRO_DEG_SEC_TO_LSB;
	//pidVars.rateRoll.setpoint = cmdRoll;
	//pidRateRoll.Compute();

}
#include "motor.h"
#include "Arduino.h"
#include <math.h>


//__ Constructor ____________________________________________________________________________
motor::motor(int pwmPin, int dirPin, bool lock){
    flock = lock; //set motor lock
    
    _pwmPin = pwmPin;                 // Assign PWM pin
    pinMode(pwmPin, OUTPUT);
    
    if( dirPin >= 0 ){                // Set Communication protocal
          _dirPin = dirPin;
          pinMode(dirPin, OUTPUT);
          
          _coms = singleMag;
        }
    else _coms = locked_antiphase;
}

//New Function for tele
void motor::tele(int val){
    //int signal = constrain(val, 0,255);  // Make sure the signal is constrained to viable numbers

    int dir = LOW;
    if( val < 0 ) dir = HIGH;
    val = abs(val);
    
    digitalWrite(_dirPin,dir);
    analogWrite(_pwmPin,val);
}


//___ Srt PID gains ________________________________________________________________________
void motor::setPID(float Kp, float Ki, float Kd){
  _Kp = Kp;
  _Ki = Ki;
  _Kd = Kd;
  }


//___ Enable use of encoder _________________________________________________________________
void motor::encoder(int chA, int chB, float encodInc){
  _useEncoder = true;
  _encodInc = encodInc;
  _chA = chA;
  _chB = chB;

  pinMode(chA,  INPUT_PULLUP);
  pinMode(chB,  INPUT_PULLUP);
//  attachInterrupt(digitalPinToInterrupt(chA), callBackGlue, CHANGE);
//  attachInterrupt(digitalPinToInterrupt(chB), callBackGlue, CHANGE);
}


//___ Set motor to a certain angular velocity ______________________________________________
void motor::angular_speed(float target_omega){  
  _current_omega = velRamp( _current_omega, target_omega );
  setMotor( _current_omega );      
}


//__ Set motors to a certain angle __________________________________________________________
bool motor::to_theta(float theta, bool set){

  if(set) _odomStart = odom;      // Lock in our freferance points
         
  float current = odom - _odomStart;      // Calculate relative angle
  debug = _odomStart;
/*
  // Debugging----------------------------------------
  Serial.print("Ref Angle: ");
  Serial.println(_odomStart);
  Serial.print("Odom: ");
  Serial.println(odom);
  Serial.print("Current Angle: ");
  Serial.println(current);
*/
    
  if (abs(current - theta) <= 0.017 ){ setMotor(0.0);
                                      return true;        //Check for Goal
  }
  else { 
      float pidval = pid(theta, current );
      if(pidval>.01) pidval = pidval + 2;
      setMotor( pidval ); // Send comand to motoer
         return false;
        }
}
  



//___ Read Encoders ___________________________________________________________________________
void motor::updateOdom(){ 
  
  int QEM [16] = {0,-1,1,2,1,0,2,-1,-1,2,0,1,2,1,-1,0};
  
  int Old = _New;
  _New = digitalRead (_chA) * 2 + digitalRead (_chB); // Convert binary input to decimal value

  int Out = QEM [Old * 4 + _New];

  odom += dir * Out *  _encodInc;

/*
  // Debugging----------------------------------------
  Serial.println(_chA);
  Serial.println(_chB);
  Serial.println(digitalRead (_chA));
  Serial.println(digitalRead (_chB));
  Serial.println(Old);
  Serial.println(New);
*/  
}


/* Privat Functions ================================================================
===================================================================================
================================================================================*/

//___ Set motor with feedback ____________________________________________________________________
void motor::setMotor(float target){

  Serial.print("Speed: ");
  Serial.println(target);
  
  if( _useEncoder ){ float actualSpeed = UpDateVelocities();
                      if(isnan(_diff)) _diff = 0.0;
                     _diff = _diff + 0.1*(target-actualSpeed);
                     target = _diff;
  }

 
  if( _coms == singleMag){

    float micro_s = (target) / maxVel  * 255.0;  // PWM control signal 
    int signal = abs(micro_s)/1;   // Truncate PWM signal to an int
    signal = constrain(signal, 0,255);  // Make sure the signal is constrained to viable numbers

    int dir = HIGH;
    if( micro_s < 0 && flock == false) dir = LOW;
    
    digitalWrite(_dirPin,dir);
    analogWrite(_pwmPin,signal);
/*
    // Debugging----------------------------------------
    Serial.print("micro: ");
    Serial.print(micro_s);
    Serial.print("     Signal: ");
    Serial.println(signal);
    Serial.print("  ");
*/  
  }
}

//___ Update Velocity _____________________________________________________
float motor::UpDateVelocities(){
  float omegach = 0.0;
  
  float dt = (millis() - _UPV_lastTime) * 0.001;
  _UPV_lastTime = millis();

  omega = (odom - _lastOdom)/dt;

  _lastOdom  = odom;

  /*
  // Debugging----------------------------------------
  Serial.print("Odom: ");
  Serial.print(odom/(2*PI));
  Serial.print(" [rad]      Vel:  ");
  Serial.print(omega);
  Serial.print(" [rad/s]      ");
  Serial.print(omega/(2*PI));
  Serial.println(" [RPS]");
*/ 
Serial.print(" [rad]      Vel:  ");
  Serial.print(omega);
  return omega;
}


//___ Velocity Ramper _____________________________________________________________________________________
float motor::velRamp( float velocity, float target ) { // Velocity Ramp function --> Called in Drive()

  float dt = (millis() - _VR_lastTime) * 0.001;
  _VR_lastTime = millis();
  
  float sign = -1.0; 
  
  float Step = maxAlpha * dt;       // Calculate step size
  float error = velocity - target;   // Calculate the difference between the target velocity and the current velocity


  /*/ Debugging----------------------------------------
  Serial.print("\nVelocity: "), Serial.print(velocity);
  Serial.print("\tTarget: "),   Serial.println(target);
  Serial.print("dT: "),         Serial.print(dt,6);
  Serial.print("\tStep: "),     Serial.print(Step);
  Serial.print("\tError: "),    Serial.println(error);*/


  if (abs(Step) > abs(error)) return target;      // If the distance to he target velocity is less than the step size return the target velocity
  
  if (velocity < target) sign = 1.0;                // Calculate the direction of the velocity step

  return velocity + sign * Step;
  
}

//___ PID ________________________________________________________________________________
float motor::pid(float setPoint, float currentVal){

  float dt = (millis() - _pid_lastTime) * 0.001;
  _pid_lastTime = millis();

  float err = setPoint - currentVal ;

  if( isnan(err)) return 0.0;
  
  _integral   = _integral + err * dt;
  
  float derivative = (err - _error) / dt;

  _error = err;

/*
  // Debugging----------------------------------------
  Serial.print("\nPID Set: ");
  Serial.println(setPoint);
  Serial.print("PID Current:  ");
  Serial.println( currentVal );
  Serial.print("PID err: ");
  Serial.println(err);
  Serial.print("PID int:  ");
  Serial.println( _integral );
  Serial.print("PID deriv:  ");
  Serial.println(derivative );
  Serial.print("PID out:  ");
  Serial.println(_Kp*err + _Ki *_integral + _Kd * derivative);
  Serial.println( );
*/  
  return _Kp*err + _Ki *_integral + _Kd * derivative;
}

//___ ISR Stuff ______________________________________________________________
/*
motor * motor::object;
void motor::callBackGlue(){object->updateOdom(); };
*/

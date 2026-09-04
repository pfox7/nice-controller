#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include "config.h"
#include <Arduino.h>

enum State { IDLE, MOVING_FORWARD, MOVING_REVERSE, OBSTACLE_BACKWARD };

extern State currentState;
extern unsigned long moveStartTime;
extern bool testMode;

void motorSetup();
void startForward();
void startReverse();
void stopMotor();
String getStatusString();
String getPositionString();
void handleObstacle(String direction);

void testForward();
void testReverse();
void testStop();
bool readFCA();
bool readFCC();

void handleStartRelay();

#endif
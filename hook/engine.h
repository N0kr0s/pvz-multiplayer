#pragma once
#include "lan.h"
#include "match-ui.h"
struct Graphics;
bool engineInstall(LanService*,void (*log)(const char*));
void enginePoll(); // UI-thread timer, outside any widget Update/Draw.
void engineRequestStart();
bool engineActive();
bool engineStarting();
void engineLeave();
bool engineInput(unsigned message,unsigned key,int x,int y);
void engineDraw(Graphics*);
const char* engineStatus();
const Match::StateData* engineState();
int engineRole();

bool engineHover(MatchUI::Cell*);
bool engineMenuHover();

bool enginePreparing();
unsigned engineCardCount();
int engineCardType(unsigned);
int engineSelected();
int engineCardCost(unsigned);
unsigned engineCardWait(unsigned);

bool engineCoinVisible(void*);
void engineRecordCoinDraw(void*,float,float);
bool engineInstallCoinDraw();

bool engineResultReady();
bool engineResultHover();

float engineCameraOffset();
bool engineRoadView();
unsigned engineRoadCount();
void* engineRoadVisual(unsigned,float*,int*);

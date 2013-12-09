/*
 * =====================================================================================
 *
 *       Filename:  command.c
 *
 *    Description:  G代码解析与执行
 *
 *        Version:  
 *        Created:  
 *       Revision:  
 *       Compiler:  
 *
 *         Author:  zhangyuxiang
 *   Organization:  
 *
 * =====================================================================================
 */
#include "common.h"
#include "command.h"
#include "move.h"
#include "motor.h"
#include "heatbed.h"
#include "extruder.h"
#include "gfiles.h"
#include "gcode.h"

//最大的代码行长度,超过部分将被丢弃
#define MAX_LINE_LENGTH 63
//G代码中使用mm,而程序中使用um
#define UNIT_CONV(x) (1000*(x))

//G代码行缓冲
static char linebuf[MAX_LINE_LENGTH+1];
//G代码中的几个参数
static int X, Y, Z, F, E;
//是否正在打印
static bool isPrinting;
static uint16_t currentState;

static void Command_doNext(void);

static void resetGcodeParams(void)
{
	X = Y = Z = E = F = 0;
}

void Command_Init(void)
{
	isPrinting = false;
	currentState = MACH_STATE_READY;
}

bool Command_StartPrinting(const char * file)
{
	if(isPrinting){
		ERR_MSG("Printing now", 0);
		return false;
	}
	
	if(!FileManager_OpenGcode(file)){
		ERR_MSG("Failed to open G code file!", 0);
		return false;
	}
	isPrinting = true;
	resetGcodeParams();

	return true;
}

void Command_Task(void)
{
	if(!isPrinting)
		return;

	switch(currentState) {
		case MACH_STATE_READY:
			Command_doNext();
			break;
		case MACH_STATE_HOMING:
			if(Move_XYZ_Ready()){
				DBG_MSG("Operation \"Homing\" Done!", 0);
				currentState = MACH_STATE_READY;
				Motor_PowerOff();
			}
			break;
		case MACH_STATE_WAIT_HEAT:
			// DBG_MSG("MACH_STATE_WAIT_HEAT", 0);
			if(Extruder_TempReached() && HeatBed_TempReached()) {
				DBG_MSG("Temperature Reached!", 0);
				currentState = MACH_STATE_READY;
			}
			break;
		case MACH_STATE_DRAWING:
			if(Move_XYZ_Ready()){
				DBG_MSG("Operation \"Drawing\" Done!", 0);
				currentState = MACH_STATE_READY;
			}
			break;
		case MACH_STATE_ENDED:
			DBG_MSG("Printing Finished!", 0);
			isPrinting = false;
			currentState = MACH_STATE_READY;
			FileManager_Close();
			break;
	}
}


static void Command_doNext__(void)
{
	static int i = 0, xyza[4];
	switch(i){
		case 0:
			DBG_MSG("Command %d", 0);
			Motor_PowerOn();
			Move_Home(X_Axis);
			Move_Home(Y_Axis);
			Move_Home(Z_Axis);
			currentState = MACH_STATE_HOMING;
			break;
		case 1:
			DBG_MSG("Command %d", 1);
			Extruder_Start_Heating();
			HeatBed_Start_Heating();
			currentState = MACH_STATE_WAIT_HEAT;
			break;
		case 2:
			DBG_MSG("Command %d", 2);
			Motor_PowerOn();
			// Move_AbsoluteMove(1000*000*20, 1000*000*20);
			// Extruder_Extrude(1);
			xyza[0] = 19600;
			xyza[1] = 0;
			xyza[2] = 2000;
			xyza[3] = 957;
			Move_AbsoluteMove(xyza);

			currentState = MACH_STATE_DRAWING;
			break;
		case 3:
			DBG_MSG("Command %d", 3);
			// Move_AbsoluteMove(1000*000*20, 1000*000*20);
			// Extruder_Extrude(1);
			xyza[0] = 19600;
			xyza[1] = 19600;
			xyza[2] = 2000;
			xyza[3] = 957*2;
			Move_AbsoluteMove(xyza);

			currentState = MACH_STATE_DRAWING;
			break;
		case 4:
			DBG_MSG("Command %d", 4);
			// Move_AbsoluteMove(1000*000*20, 1000*000*20);
			// Extruder_Extrude(1);
			xyza[1] = 0;
			xyza[0] = 0;
			xyza[2] = 2000;
			xyza[3] = 957*3;
			Move_AbsoluteMove(xyza);

			currentState = MACH_STATE_DRAWING;
			break;
		case 5:
			DBG_MSG("Command %d", 5);
			Motor_PowerOff();
			break;
		case 6:
			currentState = MACH_STATE_ENDED;
			break;
	}
	i++;
}

void doDrawingCmd()
{
	int xyza[4];
	xyza[0] = X + 40000;
	xyza[1] = Y + 40000;
	xyza[2] = Z + 720;
	xyza[3] = E;

	Move_AbsoluteMove(xyza);
}

//用于解析指令的三个辅助函数
static int getnum(char **p);
static float getfloat(char **p);
static bool getparam(char **p, char *sym, float *value);

//解析并执行下一条指令
void Command_doNext()
{
	char *p;
	char sym;
	float value;
	int cmd;

	if(!FileManager_GetLine(linebuf, MAX_LINE_LENGTH)){
		currentState = MACH_STATE_ENDED;
		return;
	}
	p = linebuf;
	if(*p == 'G'){
		p++;
		cmd = getnum(&p);
		switch(cmd){
			case G0_RAPID_MOVE:
				while(getparam(&p, &sym, &value)){
					if(sym == 'X')
						X = UNIT_CONV(value);
					else if(sym == 'Y')
						Y = UNIT_CONV(value);
					else if(sym == 'Z')
						Z = UNIT_CONV(value);
				}
				DBG_MSG("G0_RAPID_MOVE X=%d Y=%d Z=%d\n",
					X, Y, Z);
				Motor_PowerOn();
				doDrawingCmd();
				currentState = MACH_STATE_DRAWING;
				break;
			case G1_CONTROLLED_MOVE:
				while(getparam(&p, &sym, &value)){
					if(sym == 'X')
						X = UNIT_CONV(value);
					else if(sym == 'Y')
						Y = UNIT_CONV(value);
					else if(sym == 'Z')
						Z = UNIT_CONV(value);
					else if(sym == 'F')
						F = (value);
					else if(sym == 'E')
						E = UNIT_CONV(value);
				}
				DBG_MSG("G1_CONTROLLED_MOVE X=%d Y=%d Z=%d F=%d E=%d\n",
					X, Y, Z, F, E);
				Motor_PowerOn();
				doDrawingCmd();
				currentState = MACH_STATE_DRAWING;
				break;
			case G161_HOME_MINIMUM:
				DBG_MSG("G161_HOME_MINIMUM\n", 0);

				Motor_PowerOn();
				Move_Home(X_Axis);
				Move_Home(Y_Axis);
				Move_Home(Z_Axis);
				currentState = MACH_STATE_HOMING;
				break;
			default:
				break;
		}
	}else if(*p == 'M'){
		int T = 0, P = 0;
		p++;
		cmd = getnum(&p);
		switch(cmd){
			case M6_WAIT_FOR_TOOL:
				if(getparam(&p, &sym, &value) && sym == 'T')
					T = value;
				DBG_MSG("M6_WAIT_FOR_TOOL T=%d\n", T);

				Extruder_Start_Heating();
				HeatBed_Start_Heating();
				currentState = MACH_STATE_WAIT_HEAT;
				break;
			case M18_DISABLE_MOTORS:
				DBG_MSG("M18_DISABLE_MOTORS\n", 0);

				Motor_PowerOff();
				break;
			case M73_SET_PROGRESS:
				if(getparam(&p, &sym, &value) && sym == 'P')
					P = value;
				DBG_MSG("M73_SET_PROGRESS P=%d\n", P);
				break;
			default:
				break;
		}
	}else{
		//may be comments or empty line, do nothing

	}
}


static int getnum(char **p)
{
	int ret = 0;
	if(**p < '0' || **p > '9')
		return -1;
	while(**p >= '0' && **p <= '9'){
		ret = ret*10 + (**p) - '0';
		(*p)++;
	}
	return ret;
}

static float getfloat(char **p)
{
	float ret = 0;
	if((**p < '0' || **p > '9') && **p != '.')
		return -1;
	while(**p >= '0' && **p <= '9'){
		ret = ret*10 + (**p) - '0';
		(*p)++;
	}
	if(**p == '.') {
		float t = 10;
		(*p)++;
		while(**p >= '0' && **p <= '9'){
			ret += ((**p) - '0')/t;
			t *= 10;
			(*p)++;
		}
	}
	return ret;
}

static bool getparam(char **p, char *sym, float *value)
{
	float tmp;
	bool neg = false;
	while((**p) == ' ')
		(*p)++;
	if(**p < 'A' || **p > 'Z')
		return false;
	*sym = **p;
	(*p)++;
	if(**p == '-'){
		neg = true;
		(*p)++;
	}
	tmp = getfloat(p);
	if(tmp < 0)
		return false;
	*value = neg ? -tmp : tmp;

	return true;
}

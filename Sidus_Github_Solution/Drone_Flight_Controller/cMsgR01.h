#pragma once
#include <string.h>
using namespace std;

#pragma pack(push, 1)
struct structMsgR01
{
	char startChar1;
	char startChar2;

	unsigned char modeQuad;
	unsigned char statusRx;
	short rxThrottle;
	short rxPitch;
	short rxRoll;
	short rxYaw;
	unsigned char pidRatePitchKp;
	unsigned char pidRatePitchKi;
	unsigned char pidRatePitchKd;
	short pidRatePitchOutput;
	short pidRatePitchPresult;
	short pidRatePitchIresult;
	short pidRatePitchDresult;
	unsigned char pidRatePitchF1;
	unsigned char pidRatePitchF2;


	//unsigned char pidraterollkp;
	//unsigned char pidraterollki;
	//unsigned char pidraterollkd;
	//unsigned char pidrateyawkp;
	//unsigned char pidrateyawki;
	//unsigned char pidrateyawkd;
	//unsigned char pidanglepitchkp;
	//unsigned char pidanglepitchki;
	//unsigned char pidanglepitchkd;
	//unsigned char pidanglerollkp;
	//unsigned char pidanglerollki;
	//unsigned char pidanglerollkd;
	//unsigned char pidangleyawkp;
	//unsigned char pidangleyawki;
	//unsigned char pidangleyawkd;


	char endChar;
};
#pragma pack(pop)


class cMsgR01
{
public:
	cMsgR01();
	structMsgR01 message;
	unsigned char dataBytes[sizeof(message)];
	void getPacket();
	void setPacket();
	~cMsgR01();
};

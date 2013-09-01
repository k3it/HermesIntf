/* Hermes class definition which implements basic control functions of the HPSDR */


#include <winsock2.h>


#define WIN32_LEAN_AND_MEAN

//Metis sync byte
#define SYNC 0x7F


// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#pragma once



namespace HermesIntf
{

	class Hermes
	{
	public:

		//typedef struct {
			char *devname;
			char mac[25];
			char *status;
			int  ver;
			char *ip_addr;
			int max_recvrs;
			int rxCount;
			SOCKET sock;
			struct sockaddr_in Hermes_addr;
		//} HermesInfo, *PHermesInfo;

		//PHermesInfo PHInfo;

		Hermes(void);
		~Hermes(void);
		int StopCapture(void);
		int StartCapture(int RxCount, int SampleRate);
		int Discover(void);
		int SetLO(int RecvrID, int Frequency);

		//Attenuator control functions
		void SetAtt(int AttDb);
		void SetMaxAtt(void);
		void IncrAtt(void);
		void DecrAtt(void);

	private:	
		WSADATA wsaData;  /* Structure for WinSock setup communication */
		//struct sockaddr_in Hermes_addr;
		struct sockaddr_in bcast_addr;
		struct sockaddr_in Desired_addr;
		struct sockaddr_in Skimmer_addr; 
		unsigned int seq_no;
		char Desired_mac[2];
		
		unsigned int NextSeq();
		void ResetSeq();

		//Attenuator variables
		int Att;
		int MaxAtt;
		
		int setBcastDest(void);
		int setMacDest(void);

		struct {
			char C0;
			char C1;
			char C2;
			char C3;
			char C4;
		} cfgBytes;
	};


}


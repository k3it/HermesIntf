/* Hermes class definition which implements basic control functions of the HPSDR */


#include <winsock2.h>



#define WIN32_LEAN_AND_MEAN
#define WINSOCK_DEPRECATED_NO_WARNINGS

#include <signal.h>
#include <stdlib.h>
#include <tchar.h>
#include <csignal>
#include <iostream>
#include <cstdlib>

//Metis sync byte
#define SYNC 0x7F


// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#pragma once

#define MAX_RX_COUNT  8
#define LONG_MSG  1444
#define SHORT_MSG 60

// port definitions from host
#define GENERAL_REGISTERS_FROM_HOST_PORT 1024
#define RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT 1025
#define HIGH_PRIORITY_FROM_HOST_PORT 1027

// port definitions to host
#define COMMAND_RESPONSE_TO_HOST_PORT 1024
#define HIGH_PRIORITY_TO_HOST_PORT 1025
#define MICROPHONE_DATA_TO_HOST 1026
#define RX_IQ_TO_HOST_PORT_0 1035
#define RX_IQ_TO_HOST_PORT_1 1036
#define RX_IQ_TO_HOST_PORT_2 1037
#define RX_IQ_TO_HOST_PORT_3 1038
#define RX_IQ_TO_HOST_PORT_4 1039
#define RX_IQ_TO_HOST_PORT_5 1040
#define RX_IQ_TO_HOST_PORT_6 1041
#define RX_IQ_TO_HOST_PORT_7 1042

namespace HermesIntf
{

	class Hermes
	{
	public:

		//typedef struct {
			char *devname;
			char emulation_id[9];
			char mac[25];
			char *status;
			int  ver;
			int  prot_ver;
			int  clock;
			int  sample_rates[3];
			char *ip_addr;
			int max_recvrs;
			int rxCount;
			char recvMSG[LONG_MSG];
			SOCKET sock;
			struct sockaddr_in Hermes_addr;
			struct sockaddr_in Recv_addr;
			struct sockaddr_in HP_addr;
			struct sockaddr_in Data_addr[MAX_RX_COUNT];

			// keep track of the selected parameters
			int selected_LO_freqs[MAX_RX_COUNT];
			int selected_sample_rate;
			int selected_recv_count;
			bool SlaveMode;

		//} HermesInfo, *PHermesInfo;

		//PHermesInfo PHInfo;

		Hermes(void);
		~Hermes(void);
		int StopCapture(void);
		int StartCapture(int RxCount, int SampleRate);
		int StopCapture2(void);
		int StartCapture2(int RxCount, int SampleRate);
		int Discover(void);
		void Ping(void);
		int SetLO(int RecvrID, int Frequency);
		int SetLO2(int RecvrID, int Frequency);

		//Attenuator control functions
		void SetAtt(int AttDb);
		void SetAtt2(int AttDb);
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
		unsigned char Desired_mac[2];
		HANDLE hMutex;
		
		unsigned int NextSeq();
		void ResetSeq();

		//determine slave mode
		void IsSlave();

		//signal handler for POSIX
		void WINAPI SignalHandler(int signal);
	

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


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
			int  clock;
			int  sample_rates[3];
			char *ip_addr;
			int max_recvrs;
			int rxCount;
			SOCKET sock;
			struct sockaddr_in Hermes_addr;

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


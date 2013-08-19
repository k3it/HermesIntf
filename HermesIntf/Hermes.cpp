

#include "stdafx.h"
#include "Hermes.h"
#include "HermesIntf.h"

#include <assert.h>
//#include <Ws2tcpip.h>
#include <stdio.h>
#include <string>

namespace HermesIntf 
{
	Hermes::Hermes(void)
	{
		#define MYPORT 1024  //port on which hermes sends and receives UDP packets
		
		//reset sequence number
		ResetSeq();

		//---------------------------------------
		// Initialize Winsock
		int iResult = WSAStartup(MAKEWORD(2,2), &wsaData); /* Load Winsock 2.0 DLL */
	    if (iResult != NO_ERROR) {
		    rt_exception("Error at WSASTartup()");
			return;
		}

		//SOCKET sock;
		sock = socket(AF_INET,SOCK_DGRAM,0);
		
		char broadcast = '1';
		
		setsockopt(sock,SOL_SOCKET,SO_BROADCAST,&broadcast,sizeof(broadcast));

		//set a 2 second timeout
		DWORD iTimeout;
		iTimeout = 2000;
		setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,(const char*)&iTimeout,sizeof(iTimeout));

		int const buff_size = 65536;
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)(&buff_size), sizeof(buff_size));

		Hermes_addr.sin_family       = AF_INET;         
		Hermes_addr.sin_port         = htons(MYPORT);    
		Hermes_addr.sin_addr.s_addr  = INADDR_BROADCAST; 

		bcast_addr = Hermes_addr;
		
		Skimmer_addr.sin_family       = AF_INET;         
		Skimmer_addr.sin_port         = htons(0);    
		Skimmer_addr.sin_addr.s_addr  = INADDR_ANY; 
		
		if (bind(sock,(sockaddr*)&Skimmer_addr, sizeof (Skimmer_addr)) != 0) 
		{
			rt_exception("UDP Socket Bind Error");
			return;
		}



		//write_text_to_log_file("constructor called");
	}

	Hermes::~Hermes(void)
	{
		//write_text_to_log_file("destructor called");
		StopCapture();
		closesocket(sock);
		WSACleanup();
	}

	unsigned int Hermes::NextSeq()
	{
		return seq_no++;
	}

	void Hermes::ResetSeq()
	{
		seq_no = 0;
	}

	int Hermes::StopCapture(void)
	{
		//stop RX command
		char sendMSG[64] = {0};
		sendMSG[0] = (char) 0xEF;
		sendMSG[1] = (char) 0xFE;
		sendMSG[2] = (char) 0x04;
		sendMSG[3] = (char) 0x00;

		sendto(sock,sendMSG,sizeof(sendMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));

		//reset frame seq num
		ResetSeq();

		return 0;
	}


	int Hermes::StartCapture(int RxCount, int SampleRate)
	{

		rxCount = RxCount;
		//start RX command
		char sendMSG[64] = {0};
		sendMSG[0] = (char) 0xEF;
		sendMSG[1] = (char) 0xFE;
		sendMSG[2] = (char) 0x04;
		sendMSG[3] = (char) 0x01;

		
		//Configuration packet
		char cfgMSG[1032] = {0};
		
		//Metis packet hdr
		cfgMSG[0] = (char) 0xEF;
		cfgMSG[1] = (char) 0xFE;
		cfgMSG[2] = (char) 0x01;
		
		//End point EP2
		cfgMSG[3] = (char) 0x02;
		
		//next 4 bytes is the sequence num
		unsigned int seq = NextSeq();
		cfgMSG[4] = seq >> 24 & 0xFF;
		cfgMSG[5] = seq >> 16 & 0xFF;
		cfgMSG[6] = seq >> 8 & 0xFF;
		cfgMSG[7] = seq & 0xFF;

		//now three sync packets
		cfgMSG[8] = (char) SYNC;
		cfgMSG[9] = (char) SYNC;
		cfgMSG[10] = (char) SYNC;

		//five configuration packets C0-C4

		//C0 MOX False, Round Robin 0x00
		cfgBytes.C0 = 0x00;
		
		//C1:, Speed: RATE_xxxkHz, 10MHz Reference: Mercury, 122.88MHz Reference: Mercury, Config: Mercury, Mic Source: Janus
		cfgBytes.C1 = 0x5A;

		if (SampleRate == RATE_48KHZ) {  
			cfgBytes.C1 = 0x58;
		} else if (SampleRate == RATE_96KHZ) {
			cfgBytes.C1 = 0x59; 
		} else if (SampleRate == RATE_192KHZ) {
			cfgBytes.C1 = 0x5A;
		} 

		//C2: 0x00
		cfgBytes.C2 = 0x00;
		//C3: 0x1c, Alex Attenuator: 0dB, Preamp, LTC2208 Dither, LTC2208 Random, Alex Rx Antenna: none
		cfgBytes.C3 = 0x1C;

		//C4: number of RX, Alex Tx Relay: Tx1, Duplex
		cfgBytes.C4 = 0x04 | ((RxCount-1) << 3);


		//fire up this baby
		for (int i = 0; i < RxCount; i++) 
		{
			//set all rcvrs to 10 MHz
			SetLO(i,10000000);
		};
			
		//start
		sendto(sock,sendMSG,sizeof(sendMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));
		
		return 0;
	}


	int Hermes::Discover(void)
	{
		
		int len = sizeof(struct sockaddr_in);
 
		char recvbuff[1500] = {0};
		int recvbufflen = 1500;
		char sendMSG[63] = {0};
		sendMSG[0] = (char) 0xEF;
		sendMSG[1] = (char) 0xFE;
		sendMSG[2] = (char) 0x02;

		/* Set the socket to nonblocking */
		unsigned long nonblocking = 1;    /* Flag to make socket nonblocking */
 		ioctlsocket(sock, FIONBIO, &nonblocking);

		Sleep(10);
		sendto(sock,sendMSG,sizeof(sendMSG),0,(sockaddr *)&bcast_addr,sizeof(bcast_addr));
		
		//write_text_to_log_file(std::to_string(WSAGetLastError())); 

		// wait for discovery packet for one second
		long int start_time = GetTickCount();

		while(GetTickCount() - start_time < 1000)
		{

			// Look for the discovery response for 200 ms or so, if not - give up.

			if (recvfrom(sock,recvbuff,recvbufflen,0,(sockaddr *)&Hermes_addr,&len)<0) 
			{
				//WSAGetLastError() == WSAEWOULDBLOCK;
				Sleep(10);
			} else {

				//recvfrom(sock,recvbuff,recvbufflen,0,NULL,0);

				if (recvbuff[0] == (char)0xEF && recvbuff[1] == (char)0xFE) 
				{


					/* sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					recvbuff[3] & 0xFF, recvbuff[4] & 0xFF, recvbuff[5] & 0xFF,
					recvbuff[6] & 0xFF, recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);
					*/
					sprintf(mac, "%02X:%02X",
						recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);

					ip_addr = inet_ntoa(Hermes_addr.sin_addr);
					ver = recvbuff[9];

					if (recvbuff[2] == (char)0x02)
					{
						status = "Idle";
					} else if (recvbuff[2] == (char)0x03) 
					{ 
						status = "Sending Data";
					} else {
						status = "Unknown Status";
					}
				
				}

				switch (recvbuff[10])
				{
				case 0x00:
					devname = "Metis";
					max_recvrs = 4;
					break;
				case 0x01:
					devname = "Hermes";
					max_recvrs = 5;
					break;
				case 0x02:
					devname = "Griffin";
					max_recvrs = 1;
					break;
				case 0x04:
					devname = "Angelia";
					max_recvrs = 5;
					break;
				default:
					devname = "Unknown brd ID";
					max_recvrs = 2;
				}
				break;

			}
		}

		/* restore socket to blocking */
		nonblocking = 0;    /* Flag to make socket blocking */
		ioctlsocket(sock, FIONBIO, &nonblocking);


		if (devname != NULL) 
		{
			return 0;
		} else {
			return 1;
		}
	}


	int Hermes::SetLO(int RecvrID, int Frequency)
	{
		//Configuration packet
		char cfgMSG[1032] = {0};
		
		//Metis packet hdr
		cfgMSG[0] = (char) 0xEF;
		cfgMSG[1] = (char) 0xFE;
		cfgMSG[2] = (char) 0x01;
		
		//End point EP2
		cfgMSG[3] = (char) 0x02;
		
		//next 4 bytes is the sequence num
		unsigned int seq = NextSeq();
		cfgMSG[4] = seq >> 24 & 0xFF;
		cfgMSG[5] = seq >> 8 & 0xFF;
		cfgMSG[6] = seq >> 16 & 0xFF;
		cfgMSG[7] = seq & 0xFF;

		//now three sync packets
		cfgMSG[8] = (char) SYNC;
		cfgMSG[9] = (char) SYNC;
		cfgMSG[10] = (char) SYNC;

		//five configuration packets C0-C4

		//C0 MOX False, Select Receiver
		char C0 = (RecvrID+2) << 1;
		
		//C1-C4 sets frequency
		char C1 = Frequency >> 24 & 0xFF;
		char C2 = Frequency >> 16 & 0xFF;
		char C3 = Frequency >> 8 & 0xFF;
		char C4 = Frequency & 0xFF;

		//first frame is RX configuration (from StartRX)
		cfgMSG[11] = cfgBytes.C0;
		cfgMSG[12] = cfgBytes.C1;
		cfgMSG[13] = cfgBytes.C2;
		cfgMSG[14] = cfgBytes.C3;
		cfgMSG[15] = cfgBytes.C4;

		//fast forward to the next frame
		//now three sync packets
		cfgMSG[520] = (char) SYNC;
		cfgMSG[521] = (char) SYNC;
		cfgMSG[522] = (char) SYNC;

		//five configuration packets C0-C4
		//frequency

		cfgMSG[523] = C0;
		cfgMSG[524] = C1;
		cfgMSG[525] = C2;
		cfgMSG[526] = C3;
		cfgMSG[527] = C4;

		//send the payload twice
		sendto(sock,cfgMSG,sizeof(cfgMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));
		sendto(sock,cfgMSG,sizeof(cfgMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));
		
		//sleep a little, let NCO settle
		Sleep(10);
		return 0;
		
	}
}
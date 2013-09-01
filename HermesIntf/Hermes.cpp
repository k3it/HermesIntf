

#include "stdafx.h"
#include "Hermes.h"
#include "HermesIntf.h"

#include <assert.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <iostream>

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


		//send out broadcast from each interface
		
		/*
		SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
		if (sd == SOCKET_ERROR) {
			write_text_to_log_file("Failed to get a socket. sd ");
			return 1;
		}
		*/

		INTERFACE_INFO InterfaceList[20];
		unsigned long nBytesReturned;
		if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
			sizeof(InterfaceList), &nBytesReturned, 0, 0) == SOCKET_ERROR) {
				write_text_to_log_file("Failed calling WSAIoctl");
				return 1;
		}

		Sleep(10);
		
		int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

		//check if destination address is specified in the DLL name
		//e.g. HermesIntf_192.168.111.222.dll
		

		for (int i = 0; i < nNumInterfaces; ++i) 
		{
			sockaddr_in *pAddress, *pMask;
			pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress); 
			pMask = (sockaddr_in *) & (InterfaceList[i].iiNetmask);

			//calculate magic broadcast address
			bcast_addr.sin_addr.S_un.S_addr = pAddress->sin_addr.S_un.S_addr | (~ pMask->sin_addr.S_un.S_addr);

			//send discovery
			sendto(sock,sendMSG,sizeof(sendMSG),0,(sockaddr *)&bcast_addr,sizeof(bcast_addr));

		}

		
		/* Set the socket to nonblocking */
		unsigned long nonblocking = 1;    /* Flag to make socket nonblocking */
 		ioctlsocket(sock, FIONBIO, &nonblocking);

		
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

					if (setBcastDest() == 0 && Hermes_addr.sin_addr.S_un.S_addr != Desired_addr.sin_addr.S_un.S_addr) 
					{
						continue;
					} else if (setMacDest() == 0 && ((recvbuff[7] & 0xFF) != Desired_mac[0] 
								|| (recvbuff[8] & 0xFF) != Desired_mac[1]) ) 
					{
						continue;
					}

					
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
					Att = 0;
					MaxAtt = 0;
					break;
				case 0x01:
					devname = "Hermes";
					max_recvrs = 5;
					Att = 0;
					MaxAtt = 31;
					break;
				case 0x02:
					devname = "Griffin";
					max_recvrs = 2;
					Att = 0;
					MaxAtt = 31;
					break;
				case 0x04:
					devname = "Angelia";
					max_recvrs = 5;
					Att = 0;
					MaxAtt = 31;
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
		
		void Hermes::SetAtt(int AttDb) 
		{
			if (AttDb >= 0 && AttDb <= MaxAtt) 
			{
				//send attenuator message
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

				//C0 MOX False, Select Hermes Attenuator
				char C0 = (char) 0x14;
				char C1 = (char) 0x00;
				char C2 = (char) 0x00;
				char C3 = (char) 0x00;
				char C4 = (char) 0x00;
				
				if (AttDb != 0) {
					//enable and set att
					C4 = (char) (1 << 5)  | AttDb;
					Att = AttDb;
				}

				cfgMSG[11] = C0;
				cfgMSG[12] = C1;
				cfgMSG[13] = C2;
				cfgMSG[14] = C3;
				cfgMSG[15] = C4;

				//fast forward to the next frame
				//now three sync packets
				cfgMSG[520] = (char) SYNC;
				cfgMSG[521] = (char) SYNC;
				cfgMSG[522] = (char) SYNC;

				//five configuration packets C0-C4
				//set C0 to Angelia second Att if needed

				if (devname == "Angelia") {
					C0 = (char) 0x16;
					C1 = C4;
					C4 = (char) 0x00;
				}

				cfgMSG[523] = C0;
				cfgMSG[524] = C1;
				cfgMSG[525] = C2;
				cfgMSG[526] = C3;
				cfgMSG[527] = C4;

				//send the payload twice
				sendto(sock,cfgMSG,sizeof(cfgMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));
				sendto(sock,cfgMSG,sizeof(cfgMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));

				Att = AttDb;

				//debug
				//write_text_to_log_file("Attenuator set");
				//write_text_to_log_file(std::to_string(Att));
			}
			return;
		}
		
		void Hermes::SetMaxAtt(void) {
			SetAtt(MaxAtt);
			return;
		}

		void Hermes::IncrAtt(void) {
			if (Att < MaxAtt) {
				SetAtt(Att+1);
			}
			return;
		}

		void Hermes::DecrAtt(void) {
			if (Att > 0) {
				SetAtt(Att-1);
			}
			return;
		}

		int Hermes::setBcastDest(void) {

			//this function looks at the name of the dll file and attempts to extract the broadcast destination
			


			char szFileName[MAX_PATH];
			char fname[_MAX_FNAME];
			
			HMODULE hm = NULL;

			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR) &StopRx, 
				&hm))
			{
				int ret = GetLastError();
				write_text_to_log_file("GetModuleHandle failed");
				return 1;
			}
			
			GetModuleFileNameA(hm, szFileName, sizeof szFileName);
			_splitpath(szFileName,NULL,NULL,fname,NULL);

			std::string bcast_dest = fname;
			int location = bcast_dest.find_last_of("_");
			if (location != std::string::npos) {
				bcast_dest = bcast_dest.substr(location+1);
				//write_text_to_log_file(bcast_dest);
				if (inet_pton(AF_INET,bcast_dest.c_str(),&(Desired_addr.sin_addr)) == 1){
					write_text_to_log_file("IP Address DLL Filter found");
					return 0;
				}
			}
			//could not parse the address
			return 1;

		}

		int Hermes::setMacDest(void) {

			//this function looks at the name of the dll file and attempts to extract the MAC destination
			
			char szFileName[MAX_PATH];
			char fname[_MAX_FNAME];

			

			
			HMODULE hm = NULL;

			if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				(LPCSTR) &StopRx, 
				&hm))
			{
				int ret = GetLastError();
				write_text_to_log_file("GetModuleHandle failed");
				return 1;
			}
			
			GetModuleFileNameA(hm, szFileName, sizeof szFileName);
			_splitpath(szFileName,NULL,NULL,fname,NULL);

			std::string mac_dest = fname;
			int location = mac_dest.find_last_of("_");
			if (location != std::string::npos) {
				mac_dest = mac_dest.substr(location+1);
				//write_text_to_log_file(mac_dest);
				if (mac_dest.size() == 4){
					if (sscanf((char *) &mac_dest, "%2hhx%2hhx", &Desired_mac[0], &Desired_mac[1]) == 2);
					write_text_to_log_file("MAC Address DLL Filter found");
					return 0;
				}
			}
			//could not parse the address
			return 1;

		}


}
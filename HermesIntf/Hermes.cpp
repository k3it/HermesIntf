

#include "stdafx.h"
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include "Hermes.h"
#include "HermesIntf.h"

#include <assert.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <string>
#include <iostream>
#include <sstream>

// Shared static string literal constants, location will not change
extern "C" const char  UNKNOWN_HPSDR[]  = "Unknown HPSDR";
extern "C" const char  IDLE[]           = "Idle";
extern "C" const char  SENDING_DATA[]   = "Sending Data";
extern "C" const char  UNKNOWN_STATUS[] = "Unknown Status";
extern "C" const char  METIS[]          = "Metis";
extern "C" const char  HERMES[]         = "Hermes";
extern "C" const char  GRIFFIN[]        = "Griffin";
extern "C" const char  ANGELIA[]        = "Angelia";
extern "C" const char  HERMESLT[]       = "HermesLT";
extern "C" const char  UNKNOWN_BRD_ID[] = "Unknown brd ID";
extern "C" const char  RTLDNGL[]        = "RTLdngl";
extern "C" const char  REDPITAYA[]      = "RP";	// Was "RedPitaya" -- name too long for CWSL_Tee / Skimmer
extern "C" const char  AFEDRI[]         = "Afedri";
extern "C" const char  ORION[]          = "Orion";
extern "C" const char  ORION2[]         = "Orion2";
extern "C" const char  SATURN[]         = "Saturn";
extern "C" const char  ANAN10E[]        = "Anan10E";

namespace HermesIntf 
{
	Hermes::Hermes(void)
	{
		#define MYPORT 1024  //port on which hermes sends and receives UDP packets
		
		// check if another HermesIntf is running
		// IsSlave();

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
		(prot_ver == 1) ? StopCapture() : StopCapture2();
		closesocket(sock);
		WSACleanup();

		//CloseHandle(hMutex);
		//write_text_to_log_file("Cleaned up mutex");
		
	}

	void Hermes::SignalHandler(int signal)
	{
		rt_exception("Caught exit signal");
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

	int Hermes::StopCapture2(void)
	{
		write_text_to_log_file("StopCapture2");

		//stop RX command
		recvMSG[4] = 0x00; //!run
		sendto(sock, recvMSG, sizeof(recvMSG), 0, (sockaddr *)&HP_addr, sizeof(HP_addr));
		Sleep(10);

		//reset frame seq num
		ResetSeq();

		return 0;
	}

	//detect if another copy of DLL is running
	void Hermes::IsSlave(void) {
		//use mutex to determine if another copy of the DLL is running

		LPCWSTR MY_MUTEX_NAME = L"HermesIntf_k3it_mutex";

		//attempt to open mutex, if success we are in the slave mode
		hMutex = ::OpenMutex(SYNCHRONIZE, FALSE, MY_MUTEX_NAME);
		if (hMutex != NULL)
		{
			SlaveMode = TRUE;
			CloseHandle(hMutex);
			rt_exception("Slave mode - isslave()");
			
		} else {
			hMutex = ::CreateMutex(NULL, FALSE, MY_MUTEX_NAME);
			SlaveMode = FALSE;
			write_text_to_log_file("MASTER Mode");
			
		}
		
	}

	int Hermes::StartCapture2(int RxCount, int SampleRate)
	{

		rxCount = RxCount;
		//start RX command
		char sendMSG[LONG_MSG] = { 0 };
		unsigned int seq = NextSeq();
		int i;
		long phase;

		// General msg
		sendMSG[0] = seq >> 24 & 0xFF;
		sendMSG[1] = seq >> 16 & 0xFF;
		sendMSG[2] = seq >> 8 & 0xFF;
		sendMSG[3] = seq & 0xFF;

		sendMSG[37] = 0x08; //phase word (not freq)
		sendMSG[38] = 0x00; //enable hardware watchdog timer RRK
		sendMSG[58] = 0x00; //disable PA
		sendMSG[59] = 0x03; //enable Alexs

		sendto(sock, sendMSG, SHORT_MSG, 0, (sockaddr *)&Hermes_addr, sizeof(Hermes_addr));
		Sleep(100);

		memset(sendMSG, 0, LONG_MSG);

		// DDC specific msg
		sendMSG[4] = 2; // 2 ADCs

		for (i = 0; i < rxCount; i++) {
			sendMSG[7] |= 1 << i; //DDC enable
			sendMSG[17 + (i * 6)] = 0; //ADC0

			sendMSG[18 + (i * 6)] = 0;
			if (SampleRate == RATE_48KHZ) {
				sendMSG[19 + (i * 6)] = 48;
			}
			else if (SampleRate == RATE_96KHZ) {
				sendMSG[19 + (i * 6)] = 96;
			}
			else if (SampleRate == RATE_192KHZ) {
				sendMSG[19 + (i * 6)] = 192;
			}
			sendMSG[22 + (i * 6)] = 24;
		}

		sendto(sock, sendMSG, sizeof(sendMSG), 0, (sockaddr *)&Recv_addr, sizeof(Recv_addr));
		Sleep(100);

		// High Priority msg
		phase = (long)((4294967296.0*(double)10000000) / 122880000.0); //10Mhz

		memset(recvMSG, 0, LONG_MSG);

		for (i = 0; i < rxCount; i++) {
			recvMSG[9 + (i * 4)] = phase >> 24 & 0xff;
			recvMSG[10 + (i * 4)] = phase >> 16 & 0xff;
			recvMSG[11 + (i * 4)] = phase >> 8 & 0xff;
			recvMSG[12 + (i * 4)] = phase & 0xff;
		}

		recvMSG[1431] = 0x10; //HF Bypass on OrionMKII BPF
		recvMSG[1432] = 0x01; //Ant 1
		recvMSG[1434] = 0x10; //HF Bypass on Alex 0
		recvMSG[1443] = 0x00; //Step Attn 0 to 0dB
		recvMSG[4] = 0x01; //run

		sendto(sock, recvMSG, sizeof(recvMSG), 0, (sockaddr *)&HP_addr, sizeof(HP_addr));
		Sleep(100);
		write_text_to_log_file("StartCapture2");

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
		//C3: 0x1c, Alex Attenuator: 0dB, Preamp, LTC2208 Dither = 0, LTC2208 Random = 1, Alex Rx Antenna: none
		//cfgBytes.C3 = 0x1C;
		cfgBytes.C3 = 0x14;

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
		//if (SlaveMode == TRUE) {
		//	rt_exception("HermesIntf is in the slave mode");
		//	return 1;
		//}

		int len = sizeof(struct sockaddr_in);
 
		char recvbuff[1500] = {0};
		int recvbufflen = 1500;
		char sendMSG[63] = {0};
		char sendMSG_P2[60] = {0};
		sendMSG[0] = (char) 0xEF;
		sendMSG[1] = (char) 0xFE;
		sendMSG[2] = (char) 0x02;

		sendMSG_P2[4] = (char) 0x02;

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

			//send discovery P1
			sendto(sock,sendMSG,sizeof(sendMSG),0,(sockaddr *)&bcast_addr,sizeof(bcast_addr));

			//send discovery P2
			sendto(sock, sendMSG_P2, sizeof(sendMSG_P2), 0, (sockaddr*)& bcast_addr, sizeof(bcast_addr));
		}

		
		/* Set the socket to nonblocking */
		unsigned long nonblocking = 1;    /* Flag to make socket nonblocking */
 		ioctlsocket(sock, FIONBIO, &nonblocking);

		
		//write_text_to_log_file(std::to_string(WSAGetLastError())); 

		// wait for discovery packet for one second
		long int start_time = GetTickCount();
		devname = NULL;
		prot_ver = 0;

		while(GetTickCount() - start_time < 1500)
		{

			// Look for the discovery response for 200 ms or so, if not - give up.

			if (recvfrom(sock,recvbuff,recvbufflen,0,(sockaddr *)&Hermes_addr,&len)<0) 
			{
				//WSAGetLastError() == WSAEWOULDBLOCK;
				Sleep(10);
			} else {

				//recvfrom(sock,recvbuff,recvbufflen,0,NULL,0);

				if (recvbuff[0] == (char)0xEF && recvbuff[1] == (char)0xFE)
					prot_ver = 1;
				else if (recvbuff[0] == (char)0x00 && recvbuff[1] == (char)0x00 && recvbuff[2] == (char)0x00 && recvbuff[3] == (char)0x00)
					prot_ver = 2;

				if (prot_ver == 1) {
					write_text_to_log_file("Discover protocol1");
					/* sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					recvbuff[3] & 0xFF, recvbuff[4] & 0xFF, recvbuff[5] & 0xFF,
					recvbuff[6] & 0xFF, recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);
					*/
					//sprintf(mac, "%02X:%02X",
					//	recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);

					sprintf(mac, "%02X%02X%02X",
						recvbuff[6] & 0xFF, recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);

					ip_addr = inet_ntoa(Hermes_addr.sin_addr);

					if (setBcastDest() == 0 && Hermes_addr.sin_addr.S_un.S_addr != Desired_addr.sin_addr.S_un.S_addr) 
					{
						continue;
					} else if (setMacDest() == 0 && ((recvbuff[7] & 0xFF) != Desired_mac[0] 
								|| (recvbuff[8] & 0xFF) != Desired_mac[1]) ) 
					{
						//write_text_to_log_file(std::to_string(recvbuff[7] & 0xFF));
						//write_text_to_log_file(std::to_string(recvbuff[8] & 0xFF));
						continue;
					}

					
					ver = recvbuff[9];
					memcpy(emulation_id, &recvbuff[11], 8);
					emulation_id[8] = '\0';

					if (recvbuff[2] == (char)0x02)
					{
						status = (char *) IDLE;
					} else if (recvbuff[2] == (char)0x03) 
					{ 
						status = (char *) SENDING_DATA;
					} else {
						status = (char *) UNKNOWN_STATUS;
					}

					switch (recvbuff[10])
					{
					case 0x00:
						devname = (char *) METIS;
						max_recvrs = 4;
						Att = 0;
						MaxAtt = 0;
						break;
					case 0x01:
						devname = (char *) HERMES;
						if (ver == 18)
						{
							max_recvrs = 7;
						}
						else if (ver == 25)
						{
							max_recvrs = 5;
						}
						else if (ver == 29)
						{
							max_recvrs = 4;
						}
						else if (ver >= 15 && ver < 18 )
						{
							// ANAN10E board masquerades as a Hermes but sends a lower firmware version
							max_recvrs = 2;
							devname = (char *) ANAN10E;
						}
						else {
							max_recvrs = 4;
						}
						Att = 0;
						MaxAtt = 31;
						break;
					case 0x02:
						devname = (char *) GRIFFIN;
						max_recvrs = 2;
						Att = 0;
						MaxAtt = 31;
						break;
					case 0x04:
						devname = (char *) ANGELIA;
						max_recvrs = 7;
						Att = 0;
						MaxAtt = 31;
						break;
					case 0x05:
						devname = (char *) ORION;
						max_recvrs = 7;
						Att = 0;
						MaxAtt = 31;
						break;
					case 0x06:
						devname = (char *) HERMESLT;
						// Hermes-Lite may have different number of receivers
						// Always advertised here though
						max_recvrs = recvbuff[19];
						Att = 0;
						// Gain controlled automatically in FPGA with dither=on 
						MaxAtt = 0;
						break;
					case 0x0A:
						devname = (char*)ORION2;
						max_recvrs = 7;
						Att = 0;
						MaxAtt = 31;
						break;
					default:
						devname = (char *) UNKNOWN_BRD_ID;
						max_recvrs = 2;
					}

					//special case for N1GP emulation ID for rtl_dongles
					if (strcmp(emulation_id, "RTL_N1GP") == 0) {
						devname = (char *) RTLDNGL;
						max_recvrs = recvbuff[19];
						if ((recvbuff[22] != 0x00) & (recvbuff[22] != 0x01)) {
							sample_rates[0] = (recvbuff[20] & 0xFF) << 24 | (recvbuff[21] & 0xFF) << 16 | (recvbuff[22] & 0xFF) << 8 | (recvbuff[23] & 0xFF);
							sample_rates[1] = (recvbuff[24] & 0xFF) << 24 | (recvbuff[25] & 0xFF) << 16 | (recvbuff[26] & 0xFF) << 8 | (recvbuff[27] & 0xFF);
							sample_rates[2] = (recvbuff[28] & 0xFF) << 24 | (recvbuff[29] & 0xFF) << 16 | (recvbuff[30] & 0xFF) << 8 | (recvbuff[31] & 0xFF);
						}
						else {
							sample_rates[0] = 48000;
							sample_rates[1] = 96000;
							sample_rates[2] = 192000;
						}
					
						MaxAtt = 0;
					}

					//special case for Heremes Lite emulation HERMESLT
					//The Genuine Hermes Lite will send ID 0x06, handled above
					//but some forks may still be using  8 bytes emulation id.
					if (strcmp(emulation_id, "HERMESLT") == 0) {
						devname = (char *) HERMESLT;
						max_recvrs = recvbuff[19];
						if ((recvbuff[22] != 0x00) & (recvbuff[22] != 0x01)) {
							// nonstandard sample rates used by experimental N1GP fw
							// the official HL boards will not reach this section of the code
							sample_rates[0] = (recvbuff[20] & 0xFF) << 24 | (recvbuff[21] & 0xFF) << 16 | (recvbuff[22] & 0xFF) << 8 | (recvbuff[23] & 0xFF);
							sample_rates[1] = (recvbuff[24] & 0xFF) << 24 | (recvbuff[25] & 0xFF) << 16 | (recvbuff[26] & 0xFF) << 8 | (recvbuff[27] & 0xFF);
							sample_rates[2] = (recvbuff[28] & 0xFF) << 24 | (recvbuff[29] & 0xFF) << 16 | (recvbuff[30] & 0xFF) << 8 | (recvbuff[31] & 0xFF);
						}
						else {
							sample_rates[0] = 48000;
							sample_rates[1] = 96000;
							sample_rates[2] = 192000;
						}
					
						MaxAtt = 0;
					}

					
					//special case for Red Pitya emulation ID
					if (strcmp(emulation_id, "R_PITAYA") == 0) {
						devname = (char *) REDPITAYA;
						max_recvrs = recvbuff[19];
						if ((recvbuff[22] != 0x00) & (recvbuff[22] != 0x01)) {
							sample_rates[0] = (recvbuff[20] & 0xFF) << 24 | (recvbuff[21] & 0xFF) << 16 | (recvbuff[22] & 0xFF) << 8 | (recvbuff[23] & 0xFF);
							sample_rates[1] = (recvbuff[24] & 0xFF) << 24 | (recvbuff[25] & 0xFF) << 16 | (recvbuff[26] & 0xFF) << 8 | (recvbuff[27] & 0xFF);
							sample_rates[2] = (recvbuff[28] & 0xFF) << 24 | (recvbuff[29] & 0xFF) << 16 | (recvbuff[30] & 0xFF) << 8 | (recvbuff[31] & 0xFF);
						}
						else {
							sample_rates[0] = 48000;
							sample_rates[1] = 96000;
							sample_rates[2] = 192000;
						}
					
						MaxAtt = 0;
					}


					//special case for Afedri emulation ID for Afedri
					if (strcmp(emulation_id, "AFEDRIRX") == 0) {
						devname = (char *) AFEDRI;
						max_recvrs = recvbuff[19];
						clock = (recvbuff[20] & 0xFF) << 24 | (recvbuff[21] & 0xFF) << 16 | (recvbuff[22] & 0xFF) << 8 | (recvbuff[23] & 0xFF);
						MaxAtt = 0;
					}

					//check that we have not exceeded MAX_RX_COUNT 
					if (max_recvrs > MAX_RX_COUNT) {
						max_recvrs = MAX_RX_COUNT;
					}

					break;
				}
				else if (prot_ver == 2) {
					write_text_to_log_file("Discover protocol2");

					/* sprintf(mac, "%02X:%02X:%02X:%02X:%02X:%02X",
					recvbuff[5] & 0xFF, recvbuff[6] & 0xFF, recvbuff[7] & 0xFF,
					recvbuff[8] & 0xFF, recvbuff[9] & 0xFF, recvbuff[10] & 0xFF);
					*/
					//sprintf(mac, "%02X:%02X",
					//	recvbuff[7] & 0xFF, recvbuff[8] & 0xFF);

					sprintf(mac, "%02X%02X%02X",
						recvbuff[8] & 0xFF, recvbuff[9] & 0xFF, recvbuff[10] & 0xFF);

					ip_addr = inet_ntoa(Hermes_addr.sin_addr);

					if (setBcastDest() == 0 && Hermes_addr.sin_addr.S_un.S_addr != Desired_addr.sin_addr.S_un.S_addr)
					{
						continue;
					}
					else if (setMacDest() == 0 && ((recvbuff[9] & 0xFF) != Desired_mac[0]
						|| (recvbuff[10] & 0xFF) != Desired_mac[1]))
					{
						//write_text_to_log_file(std::to_string(recvbuff[7] & 0xFF));
						//write_text_to_log_file(std::to_string(recvbuff[8] & 0xFF));
						continue;
					}

					ver = recvbuff[13]; //Firmware version

					if (recvbuff[4] == (char)0x02)
					{
						status = (char *)IDLE;
					}
					else if (recvbuff[4] == (char)0x03)
					{
						status = (char *)SENDING_DATA;
					}
					else {
						status = (char *)UNKNOWN_STATUS;
					}

					max_recvrs = recvbuff[20];
					MaxAtt = 31;
					Att = 0;
					sample_rates[0] = 48000;
					sample_rates[1] = 96000;
					sample_rates[2] = 192000;

					switch (recvbuff[11])
					{
						case 0x00:
							devname = (char *)METIS;
							MaxAtt = 0;
							break;
						case 0x01:
							devname = (char *)HERMES;
							break;
						case 0x02:
							devname = (char *)GRIFFIN;
							break;
						case 0x03:
							devname = (char *)ANGELIA;
							break;
						case 0x04:
							devname = (char *)ORION;
							break;
						case 0x05:
							devname = (char *)ORION2;
							break;
						case 0x0A:
							devname = (char*)SATURN;
							break;
						default:
							devname = NULL;
					}

					//check that we have not exceeded MAX_RX_COUNT 
					if (max_recvrs > MAX_RX_COUNT) {
						max_recvrs = MAX_RX_COUNT;
					}

					memcpy(&Recv_addr, &Hermes_addr, sizeof(Hermes_addr));
					Recv_addr.sin_port = htons(RECEIVER_SPECIFIC_REGISTERS_FROM_HOST_PORT);
					memcpy(&HP_addr, &Hermes_addr, sizeof(Hermes_addr));
					HP_addr.sin_port = htons(HIGH_PRIORITY_FROM_HOST_PORT);

					char broadcast = '1';
					setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &broadcast, sizeof(Hermes_addr));
					for (int i = 0; i < max_recvrs; ++i)
					{
						memcpy(&Data_addr[i], &Hermes_addr, sizeof(Hermes_addr));
						Data_addr[i].sin_port = htons(RX_IQ_TO_HOST_PORT_0 + i);
					}
					break;
				}
			}
		}

		/* restore socket to blocking */
		nonblocking = 0;    /* Flag to make socket blocking */
		ioctlsocket(sock, FIONBIO, &nonblocking);

		return (devname != NULL) ? 0 : 1;
	}

	void Hermes::Ping()
	{
		// reset the Network HW watchdog timer by resending rx specific msg
		sendto(sock, recvMSG, sizeof(recvMSG), 0, (sockaddr *)&HP_addr, sizeof(HP_addr));
	}

	int Hermes::SetLO2(int RecvrID, int Frequency)
	{
		long phase;
		write_text_to_log_file("SetLO2");
		phase = (long)((4294967296.0*(double)Frequency) / 122880000.0); //convert to phase

		recvMSG[9 + (RecvrID * 4)] = phase >> 24 & 0xff;
		recvMSG[10 + (RecvrID * 4)] = phase >> 16 & 0xff;
		recvMSG[11 + (RecvrID * 4)] = phase >> 8 & 0xff;
		recvMSG[12 + (RecvrID * 4)] = phase & 0xff;

		sendto(sock, recvMSG, sizeof(recvMSG), 0, (sockaddr *)&HP_addr, sizeof(HP_addr));

		return 0;
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
	
		//char C0 = (RecvrID + 2) << 1;
		//N1GP: Yes, there is a bug for rcvr 8. I think this change will fix it (assuming RecvrID is zero based):
		char C0 = (RecvrID < 7) ? (RecvrID + 2) << 1 : (RecvrID + 11) << 1;
		
		//workaround for v2.5 of Hermes where Rx5 is hard wired to the TX NCO
		//if (ver == 25 & RecvrID == 4) {
			//C0 = 0x02;  //NCO for TX
		//}


		
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
		sendto(sock, cfgMSG, sizeof(cfgMSG), 0, (sockaddr *)&Hermes_addr, sizeof(bcast_addr));
		sendto(sock,cfgMSG,sizeof(cfgMSG),0,(sockaddr *)&Hermes_addr,sizeof(bcast_addr));

		
		//sleep a little, let NCO settle
		Sleep(10);
		return 0;
		
	}
		
	void Hermes::SetAtt2(int AttDb) 
	{
		if (AttDb >= 0 && AttDb <= MaxAtt) 
		{
			//set att
			Att = AttDb;

			recvMSG[1443] = AttDb & 0xFF;
			sendto(sock, recvMSG, sizeof(recvMSG), 0, (sockaddr *)&HP_addr, sizeof(HP_addr));

			//debug
			std::stringstream sstm;
			sstm << "Step ATT2: " << Att << "dB";
			write_text_to_log_file(sstm.str());
		}
		return;
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

				if (devname == ANGELIA) {
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
				std::stringstream sstm;
				sstm << "Step ATT: " << Att << "dB";
				write_text_to_log_file(sstm.str());
				//write_text_to_log_file("Attenuator set");
				//write_text_to_log_file(std::to_string(Att));
			}
			return;
		}
		
		void Hermes::SetMaxAtt(void) {
			(prot_ver == 1) ? SetAtt(MaxAtt) : SetAtt2(MaxAtt);
			return;
		}

		void Hermes::IncrAtt(void) {
			if (Att < MaxAtt) {
			(prot_ver == 1) ? SetAtt(Att+1) : SetAtt2(Att+1);
			}
			return;
		}

		void Hermes::DecrAtt(void) {
			if (Att > 0) {
			(prot_ver == 1) ? SetAtt(Att-1) : SetAtt2(Att-1);
			}
			return;
		}

		int Hermes::setBcastDest(void) {

			//this function looks at the name of the dll file and attempts to extract the broadcast destination
			


			char szFileName[MAX_PATH];
			char fname[_MAX_FNAME];
			unsigned long dest_addr;
			
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
				//use of inet_pton breaks windows XP compatibility 
				//if (inet_pton(AF_INET,bcast_dest.c_str(),&(Desired_addr.sin_addr)) == 1){
				
				dest_addr = inet_addr(bcast_dest.c_str());
				if (bcast_dest.size() > 6 && dest_addr != INADDR_NONE) {
					Desired_addr.sin_addr.S_un.S_addr = dest_addr;
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
					if (sscanf((char *)&mac_dest, "%2hhX%2hhX", &Desired_mac[0], &Desired_mac[1]) == 2) {
						write_text_to_log_file("MAC Address DLL Filter found");
						return 0;
					}
				}
			}
			//could not parse the address
			return 1;

		}


}

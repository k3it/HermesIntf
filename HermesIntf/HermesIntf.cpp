// HermesIntf.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include "HermesIntf.h"
#include <stdexcept>
#include <string>
#include <iostream>
#include "Hermes.h"
#include <assert.h>


namespace HermesIntf
{
	///////////////////////////////////////////////////////////////////////////////
	// Global variables

	// Settings from Skimmer server
	SdrSettings gSet;

	volatile int ADC_overflow_count = 0;

	// Sample rate of Skimmer server
	int gSampleRate = 0;

	// Length of block for one call of IQProc
	int gBlockInSamples = 0;

	// Buffers for calling IQProc
	CmplxA gData[MAX_RX_COUNT];

	// Current length of data in Buffers for calling IQProc (in samples)
	int gDataSamples = 0;

	// Instance of hermes
	Hermes myHermes;

	//Handle & ID of worker thread
	DWORD gidWrk = 0;
	HANDLE ghWrk = NULL;

	//Handle & ID of Agc thread
	DWORD gidAgc = 0;
	HANDLE ghAgc = NULL;


	// Stop flag
	volatile bool gStopFlag = false;



	//////////////////////////////////////////////////////////////////////////////
	// Allocate working buffers & others
	BOOL Alloc(void)
	{
		int i;

		// free buffers for calling IQProc
		for (i = 0; i < MAX_RX_COUNT; i++)
		{// allocated ?
			if (gData[i] != NULL) free(gData[i]);
			gData[i] = NULL;
		}

		// decode sample rate
		if (gSet.RateID == RATE_48KHZ) {
			gSampleRate = 48000;
		} else if (gSet.RateID == RATE_96KHZ) {
			gSampleRate = 96000;
		} else if (gSet.RateID == RATE_192KHZ) {
			gSampleRate = 192000;
		} else {
			// unknown sample rate
			rt_exception("Unknown sample rate");
			return(FALSE);
		} 


		// compute length of block in samples
		gBlockInSamples = (int)((float)gSampleRate / (float)BLOCKS_PER_SEC);

		// allocate buffers for calling IQProc
		for (i = 0; i < MAX_RX_COUNT; i++)
		{
			// allocated ?
			if (gData[i] != NULL) free(gData[i]);

			// allocate buffer
			gData[i] = (CmplxA) malloc(gBlockInSamples * sizeof(Cmplx));

			// have we memory ?
			if (gData[i] == NULL) 
			{// low memory
				rt_exception("Low memory");
				return(FALSE);
			} 

			// clear it
			memset(gData[i], 0, gBlockInSamples * sizeof(Cmplx));
		}
		gDataSamples = 0;

		// success
		return(TRUE);
	}

	DWORD WINAPI Agc(LPVOID lpParameter)
	{
		//reset attenuator
		myHermes.SetAtt(0);
		int lastAttChange = GetTickCount();

		while(!gStopFlag)
		{
			if (ADC_overflow_count > 0) 
			{
				//write_text_to_log_file("ADC Overload");
				myHermes.IncrAtt();
				Sleep(10);
				ADC_overflow_count = 0;
				lastAttChange = GetTickCount();
			}

			//gradually decrease attenuation
			if (GetTickCount() - lastAttChange > 10000) {
				myHermes.DecrAtt();
				lastAttChange = GetTickCount();
			}

			Sleep(100);
		}
		
		myHermes.SetAtt(0);
		return(0);
	}


	DWORD WINAPI Worker(LPVOID lpParameter)
	{
		
		Cmplx *optr[MAX_RX_COUNT];
		int gNChan = myHermes.rxCount;

		// metis samples are in two frames at recvbuff[16..519] and [528..1031]
		int samplesPerFrame = 504 / (gNChan*6+2);
		const int framesPerPacket = 2;

		int i,j,k,bytes_received;
		//int smplI, smplQ;

		// wait for a while ...
		Sleep(100);

		// prepare pointers to IQ buffer
		gDataSamples = 0;
		for (i = 0; i < gNChan; i++) optr[i] = gData[i];

		char recvbuff[1500] = {0};
		int recvbufflen = 1500;
		int len = sizeof(struct sockaddr_in);

		//packet sequence numbers
		unsigned last_seq=0;
		unsigned int rx_seq = 0;
		unsigned int lost_pkts = 0;
		DWORD last_error = GetTickCount();
		
	

		// main loop
		while (!gStopFlag)
		{

			// read data block
				
			//bytes_received = recv(myHermes.sock,recvbuff,recvbufflen,0);

			bytes_received = recvfrom(myHermes.sock,recvbuff,recvbufflen,0,(sockaddr *)&myHermes.Hermes_addr,&len);

			if (bytes_received <= 0 && WSAGetLastError() == WSAETIMEDOUT)
			{
				rt_exception("Timed out waiting for UDP data");
				break;
			}

			 if (myHermes.Hermes_addr.sin_port == htons(12345)) {
				rt_exception("Got a magic packet");
			}

			
			//verify sequence #
			
			/* Calculate avg sample rate every 10 sec.  UDB buffer size tuning.
				
			//rx_seq = (recvbuff[4] & 0xFF << 24) + (recvbuff[5] & 0xFF << 16) + (recvbuff[6] & 0xFF << 8) + recvbuff[7] & 0xFF;
			 rx_seq++;

			if (GetTickCount() - last_error >= 10000)
			{
				//rt_exception("Loosing packets!!!");
			//write_text_to_log_file(std::to_string(rx_seq));
				write_text_to_log_file(std::to_string(rx_seq*samplesPerFrame/5));
				last_error = GetTickCount();
				rx_seq=0;
			}

			last_seq = rx_seq;

			//continue;

			*/

			//check if this is a EP6 frame from metis

			 if (bytes_received > 0 && recvbuff[0] == (char)0xEF && recvbuff[1] == (char)0xFE && recvbuff[2] == (char)0x01 && recvbuff[3] == (char) 0x06)
			{
				
				
				//process UDP packet
				int indx = 16;  //start of samples in recvbuff


				
				/* Debug code for checking sequence numbers
			
				rx_seq = recvbuff[4] << 24 | recvbuff[5] << 16 | recvbuff[6] << 8 | recvbuff[7];
			
				if ( rx_seq != last_seq+1) 
				{
					lost_pkts++;
				}

				last_seq = rx_seq;

				if (GetTickCount() - last_error >= 20000)
				{
					//rt_exception("Loosing packets!!!");
					write_text_to_log_file(std::to_string(lost_pkts));
					last_error = GetTickCount();
				}

				*/

				//check for PTT bit
				if ( (recvbuff[11] & 0xFF) & (1<<0) == 1 || (recvbuff[523] & 0xFF) & (1<<0) == 1 ) {
					//write_text_to_log_file("PTT!");
					continue;
				}

				//check for ADC overload
				if ( (((recvbuff[11] & 0xFF) >> 3) == 0  && (recvbuff[12] & 0xFF)  & (1<<0) == 1) ||
					 (((recvbuff[523] & 0xFF) >> 3) == 0 && (recvbuff[524] & 0xFF) & (1<<0) == 1) ) 
				{
					ADC_overflow_count++;
				}
		
				
				//check for sync bytes
				
				
				if ( recvbuff[8] != SYNC || recvbuff[9] != SYNC || recvbuff[10] != SYNC || 
					 recvbuff[520] != SYNC || recvbuff[521] != SYNC || recvbuff[522] != SYNC )
				{
					rt_exception("Loss of Sync on HPSDR frames");
					return(FALSE);
					//continue;
				}
				

				//process two HPSDR UDP frames
				for (k = 0; k < framesPerPacket; k++) 
				{
					for (i = 0; i < samplesPerFrame; i++)
					{
						for (j = 0; j < gNChan; j++)
						{
																			
							//optr[j]->Re =   (recvbuff[indx++] & 0xFF) << 24 | (recvbuff[indx++] & 0xFF) << 16 |  (recvbuff[indx++] & 0xFF) << 8;
							//optr[j]->Im = -((recvbuff[indx++] & 0xFF) << 24 | (recvbuff[indx++] & 0xFF) << 16 |  (recvbuff[indx++] & 0xFF) << 8);
							
							optr[j]->Re =   (recvbuff[indx++] & 0xFF) << 16 | (recvbuff[indx++] & 0xFF) << 8 |  (recvbuff[indx++] & 0xFF) << 0;
							optr[j]->Im = -((recvbuff[indx++] & 0xFF) << 16 | (recvbuff[indx++] & 0xFF) << 8 |  (recvbuff[indx++] & 0xFF) << 0);
							
							//advance to the next sample
							(optr[j])++;
		
						}
						gDataSamples++;
						indx += 2; //skip two mic bytes

						// do we have enough data ?
						if (gDataSamples >= gBlockInSamples)
						{
							
							// yes -> report it
							gDataSamples = 0;
							if (gSet.pIQProc != NULL) (*gSet.pIQProc)(gSet.THandle, gData);
							// start filling of new data
							for (int kount = 0; kount < gNChan; kount++) optr[kount] = gData[kount];
							
						}
					}
					//start of the second UDP frame
					indx = 528;
				}

			} 
			
		}

		Sleep(10);

		return(0);
	}



	// DllMain function - Do we need this?
	BOOL APIENTRY DllMain( HMODULE hModule,	DWORD  ul_reason_for_call, LPVOID lpReserved)
	{
		switch (ul_reason_for_call)
		{
		case DLL_PROCESS_ATTACH:
			for (int i = 0; i < MAX_RX_COUNT; i++) gData[i] = NULL;  
			break;

		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
		}
		return TRUE;
	}

	extern "C" 
	{
		HERMESINTF_API void __stdcall GetSdrInfo(PSdrInfo pInfo)
		{
			// did we get info ?
			if (pInfo == NULL) return;

			std::string dbg = "GetSdrInfo: ";
			char display_name[50];

			if (myHermes.Discover() == 0) {

				sprintf(display_name, "%s v%d %s", myHermes.devname, myHermes.ver, myHermes.mac);
				pInfo->MaxRecvCount = myHermes.max_recvrs;
				pInfo->ExactRates[RATE_48KHZ]  =  48e3;
				pInfo->ExactRates[RATE_96KHZ]  =  96e3;
				pInfo->ExactRates[RATE_192KHZ] = 192e3;

				dbg+=(myHermes.devname); dbg+=" ";
				dbg+=(myHermes.ip_addr); dbg+=" ";
				dbg+=(myHermes.mac); dbg+=" ";
				dbg+=(myHermes.status); dbg+=" v";
				dbg+=(std::to_string(myHermes.ver));
				write_text_to_log_file(dbg);
				pInfo->DeviceName = display_name;
			} else {
				pInfo->DeviceName = "Unknown HPSDR";
				dbg += "No response from HPSDR";
			}
			write_text_to_log_file(dbg);
		}

		

		// Start receivers
		HERMESINTF_API void __stdcall StartRx(PSdrSettings pSettings)
		{
			// have we settings ?
			if (pSettings == NULL) return;

			// make a copy of SDR settings
			memcpy(&gSet, pSettings, sizeof(gSet));


			// from skimmer server version 1.1 in high bytes is something strange
			gSet.RateID &= 0xFF;

			if (myHermes.status == NULL) {
				rt_exception("Can't locate HPSDR device");
				return;
			} else if (myHermes.status != "Idle") {
				rt_exception("HPSDR is busy sending data");
				return;
			} else if ((myHermes.devname == "Hermes" && myHermes.ver < 24) 
				|| (myHermes.devname == "Metis" && myHermes.ver < 26) 
				|| (myHermes.devname == "Angelia" && myHermes.ver < 19)) 
			{
				rt_exception("Check FPGA firmware version");
				return;
			}

			
			myHermes.StartCapture(gSet.RecvCount,gSet.RateID);
			
			write_text_to_log_file("StartRx");
			
			

			// allocate buffers & others
			if (!Alloc()) 
			{
				// something wrong ...
				return;
			}

			// start worker thread
			gStopFlag = false;
			ghWrk = CreateThread(NULL, 0, Worker, NULL, 0, &gidWrk);
			if (ghWrk == NULL)
			{
				// can't start
				rt_exception("Can't start worker thread");
				return;
			}
			write_text_to_log_file("Worker thread running");
		

			
			//for Hermes/Angelia start the AGC loop
			if (myHermes.devname == "Hermes" || myHermes.devname == "Angelia") {

				ghAgc = CreateThread(NULL, 0, Agc, NULL, 0, &gidAgc);
				if (ghAgc == NULL)
				{
					rt_exception("Can't start AGC thread");
					return;
				}
				write_text_to_log_file("AGC thread running");
			}
		
		}

		HERMESINTF_API void __stdcall StopRx(void) 
		{
			// was started worker thread ?
			if (ghWrk != NULL)
			{// set stop flag
				gStopFlag = true;

				// wait for threads
				WaitForSingleObject(ghWrk, 100);
				
				// close thread handle 
				CloseHandle(ghWrk);
				ghWrk = NULL;
				write_text_to_log_file("Worker thread done");
			}

			//was agc thread started?
			if (ghAgc != NULL)
			{// set stop flag
				gStopFlag = true;

				// wait for thread
				WaitForSingleObject(ghAgc, 200);

				// close thread handle 
				CloseHandle(ghAgc);
				ghAgc = NULL;
				write_text_to_log_file("AGC thread done");
			}


			myHermes.StopCapture();
			write_text_to_log_file("StopRx");
		}

		// Set Rx frequency
		HERMESINTF_API void __stdcall SetRxFrequency(int Frequency, int Receiver)
		{

			// check
			if ((myHermes.devname == NULL) || (Receiver < 0) || (Receiver >= myHermes.max_recvrs)) return;

			myHermes.SetLO(Receiver,Frequency);

			std::string dbg = "SetRxFrequency Rx# ";
			dbg += std::to_string(Receiver);
			dbg += " Frequency: ";
			dbg += std::to_string(Frequency);
			write_text_to_log_file(dbg);
			
		}

		HERMESINTF_API int __stdcall ReadPort(int PortNumber)
		{
			return(0);
		}

		void __stdcall SetCtrlBits(unsigned char Bits)
		{
		}



	}

	void write_text_to_log_file( const std::string &text )
	{
		std::ofstream log_file(
			"HermesIntf_log_file.txt", std::ios_base::out | std::ios_base::app );
		log_file << GetTickCount() << ": " << text << std::endl;
	}

	void rt_exception(const std::string &text)
	{
		const char *error = text.c_str();
		if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, (char *) error);	
		
		write_text_to_log_file(text);
		
		//kill thread if running
		//gStopFlag = true;
		//StopRx();
		
		//send error to the server
		//if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, (char *) error);
		
		return;
	}


}


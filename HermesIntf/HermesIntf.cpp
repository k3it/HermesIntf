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
			if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Unknown sample rate");
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
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Low memory");
				return(FALSE);
			} 

			// clear it
			memset(gData[i], 0, gBlockInSamples * sizeof(Cmplx));
		}
		gDataSamples = 0;

		// success
		return(TRUE);
	}


	DWORD WINAPI Worker(LPVOID lpParameter)
	{
		//bool Over;
		//DWORD OutLen;
		//float *iptr[2*MAX_RX_COUNT];
		Cmplx *optr[MAX_RX_COUNT];
		int gNChan = myHermes.rxCount;

		// metis samples are in two frames at recvbuff[16..519] and [528..1031]
		int samplesPerFrame = 504 / (gNChan*6+2);
		const int framesPerPacket = 2;

		int i,j,k,bytes_received;
		int smplI, smplQ;

		// wait for a while ...
		Sleep(100);

		/* Set the socket to blocking */
		//unsigned long blocking = 0;    /* Flag to make socket blocking */
		//assert(ioctlsocket(myHermes.sock, FIONBIO, &blocking) == 0);
		
		// prepare pointers to IQ buffer
		gDataSamples = 0;
		for (i = 0; i < gNChan; i++) optr[i] = gData[i];

		char recvbuff[1500] = {0};
		int recvbufflen = 1500;
		int len = sizeof(struct sockaddr_in);

		write_text_to_log_file("Worker thread running:");
		write_text_to_log_file(std::to_string(gNChan));

		

		// main loop
		while (!gStopFlag)
		{

			// read data block
			bytes_received=0;
			//bytes_received = recvfrom(myHermes.sock,recvbuff,recvbufflen,0,(sockaddr *)&myHermes.Hermes_addr,&len);
		
			bytes_received = recv(myHermes.sock,recvbuff,recvbufflen,0);

			if (WSAGetLastError() == WSAETIMEDOUT)
			{
				write_text_to_log_file("UDP socket timeout");
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Timeout waiting for UDP data");
				return(FALSE);
			}

			//check if this is a EP6 frame from metis
			if (bytes_received > 0 && recvbuff != NULL && recvbuff[0] == (char)0xEF && recvbuff[1] == (char)0xFE && recvbuff[2] == (char)0x01 && recvbuff[3] == (char) 0x06)
			{
				//process UDP packet
				int indx = 16;  //start of samples in recvbuff

				//check for ADC overload
				//if (recvbuff[12] & (1<<0) == 1) {
					// if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "ADC Overload");
					// return(FALSE);
				//}
		
				
				//check for sync bytes
				if ( recvbuff[8] != SYNC || recvbuff[9] != SYNC || recvbuff[10] != SYNC || 
					 recvbuff[520] != SYNC || recvbuff[521] != SYNC || recvbuff[522] != SYNC )
				{
					write_text_to_log_file("Loss of Sync detected");
					if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Loss of Sync on HPSDR frames");
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
							//smplI = (recvbuff[indx] << 24) + (recvbuff[indx+1] << 16) + (recvbuff[indx+2] << 8);
							//smplQ = (recvbuff[indx+3] << 24) + (recvbuff[indx+4] << 16) + (recvbuff[indx+5] << 8);

							smplI = (recvbuff[indx++] << 24) + (recvbuff[indx++] << 16) + (recvbuff[indx++] << 8);
							smplQ = (recvbuff[indx++] << 24) + (recvbuff[indx++] << 16) + (recvbuff[indx++] << 8);

							optr[j]->Re = smplI*256;
							optr[j]->Im = -smplQ*256;
							//advance to the next sample
							(optr[j])++;
							//indx += 6;

						}
						gDataSamples++;
						indx += 2; //skip two mic bytes

						// have we enough data ?
						if (gDataSamples >= gBlockInSamples)
						{
							// start filling of new data
							gDataSamples = 0;
							for (int kount = 0; kount < gNChan; kount++) optr[kount] = gData[kount];
							// yes -> report it
							if (gSet.pIQProc != NULL) (*gSet.pIQProc)(gSet.THandle, gData);
							
						}
					}
					//start of the second frame
					indx = 528;
				}

			}
		}

		Sleep(10);

		//cleanup
		/* for (i = 0; i < gNChan; i++)
		{// allocated ?
			if (gData[i] != NULL) free(gData[i]);
			gData[i] = NULL;
			if (optr[i] != NULL) free(optr[i]);
			optr[i] = NULL;
		}


		write_text_to_log_file("Worker thread exit");
		*/
		// that's all
		return(0);
	}



	// DllMain function
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
			// have we info ?
			if (pInfo == NULL) return;

			std::string dbg = "GetSdrInfo ";
			char display_name[50];

			//write_text_to_log_file("GetSdrInfo got called");

			if (myHermes.Discover() == 0) {

				sprintf(display_name, "%s v%d %s", myHermes.devname, myHermes.ver, myHermes.mac);
				pInfo->MaxRecvCount = myHermes.max_recvrs;
				pInfo->ExactRates[RATE_48KHZ]  =  48e3;
				pInfo->ExactRates[RATE_96KHZ]  =  96e3;
				pInfo->ExactRates[RATE_192KHZ] = 192e3;

				dbg+=(myHermes.devname);
				dbg+=(myHermes.ip_addr);
				dbg+=(myHermes.mac);
				dbg+=(myHermes.status);
				dbg+=(std::to_string(myHermes.ver));
				write_text_to_log_file(dbg);
				pInfo->DeviceName = display_name;
			} else {
				pInfo->DeviceName = "Unknown HPSDR";
			}
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
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Can't locate an HPSDR device");
				return;
			} else if (myHermes.status != "Idle") {
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "HPSDR is busy sending data");
				return;
			} else if (myHermes.ver < 24) {
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Check FPGA firmware version");
				return;
			}

			write_text_to_log_file("StartRx");
			write_text_to_log_file(std::to_string(gSet.RateID));
			write_text_to_log_file(std::to_string(gSet.RecvCount));

			myHermes.StartCapture(gSet.RecvCount,gSet.RateID);

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
				if (gSet.pErrorProc != NULL) (*gSet.pErrorProc)(gSet.THandle, "Can't start worker thread");
				return;
			}
		}

		HERMESINTF_API void __stdcall StopRx(void) 
		{
			// was started worker thread ?
			if (ghWrk != NULL)
			{// set stop flag
				gStopFlag = true;

				// wait for thread
				WaitForSingleObject(ghWrk, 100);

				// close thread handle 
				CloseHandle(ghWrk);
				ghWrk = NULL;
			}

			myHermes.StopCapture();
			write_text_to_log_file("StopRx got called");
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
			//write_text_to_log_file(std::to_string(Frequency));

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

}

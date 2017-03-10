# HermesIntf
CW Skimmer Server plugin for HPSDR network protocol compatible Software Defined Radios

HermesIntf.dll should be saved to the SkimSrv program directory. 
e.g. C:\Program Files (x86)\Afreet\SkimSrv 
(see http://www.dxatlas.com/skimserver/)

The Interface will attach to the SDR that responds first to the Discovery
Packet. However, it's possible to "lock" the DLL to a particular MAC or 
IP address.  To do that HermesIntf.dll should be renamed as follows:

HermesIntf_<sdr ip addres>.dll 
e.g.   HermesIntf_192.168.111.222.dll

-or-

HermesIntf_<last 2 bytes of the MAC>.dll  
eg.  HermesIntf_693c.dll, HermesIntf_0B63.dll etc

This is useful if you have multiple boards.  Note CWSL_Tee.dll works only with
the MAC address filters.  You can save multiple copies of the DLL in the skimserver 
directory for connecting to different SDRs. They will appear as separate entries 
in the Receiver dropdown list


Supported sample rates/receivers:

48/96/192 Khz


Hermes:
- Firmware version 1.8 - 7 receivers
- Firmware version 2.4,2.5 - 5 receivers
- Firmware version 2.9 - 4 receivers
- Other revisions: defaults to 4 receivers

Hermes Lite:
- 8 receivers with BeMicro CV A9 and N1GP firmware

Red Pitaya
- 6 receivers with http://pavel-demin.github.io/red-pitaya-notes/sdr-receiver-hpsdr/

Angelia:
- 7 receivers

Griffin:
- 2 receivers

Metis:
- 2 receivers

N1GP RTL dongle in HPSDR emulation mode:
- up to 8 receivers, depending on the number of connected dongles

Afedri single and dual channel (firmware 228e and up), HPSDR emulation mode:
- 1 or 2 receivers






The ADC overload is managed using the following algorithm:
In case of an ADC overflow  the step attenuator is incremented by one db every
100ms until the overload goes away or we run out of the attenuation range.   In
the absence of overload the attenuator is decreased 1db every 10s, until it's
back to zero or the overflow bit re-appears. In a high power environment the ATT
will "teeter" +/-1db every 10s...

PTT bit:
Samples with the PTT bit ON are not sent the skimmer and the ADC overflow is
ignored.  This prevents the skimmer from starting too many threads and trying to
decode junk. also keeps CPU and memory consumption under control.  If you TX on
another radio while the skimmer server is running, you can tie its PTT-out to
pin 26 on the Hermes J16 connector.   



Not yet implemented:
- a small GUI window with status info
- warning on packet loss or ADC overload


With the RBN aggregattor software you can monitor the quality of your spots by
telnetting to:

 telnet arcluster.reversebeacon.net 7000

Set cluster filter to: set/dx/filter spotter=yourcall

See this link for more details:
http://dayton.contesting.com/pipermail/skimmertalk/2013-August/001215.html

Merucry/Angelia support -  should work but I no reports yet


Please send feedback to gokoyev+k3it@gmail.com

73 K3IT

==========================================
CHANGES
==========================================
16.6.27 - add support for Red Pitaya
16.3.5 - remove slave mode
15.7.9 -  8 channel Hermes Lite support https://groups.google.com/d/msg/hermes-lite/N130G9MEJwE/wfg05AgXrrUJ
14.8.13 - Support for more hardware and firmware combinations, improved logging
13.9.3 - added filtering by MAC or IP address (see above)
13.8.24 - fixed sample format conversion error(!)
13.8.23 - Hermes/Angelia - adjust 31db step attenuator(s) in response to ADC
	  overload. 100ms/10s/db timing
13.8.22 - Do not send samples to skimmer when PTT bit is ON (pin 26 J16)
13.8.21 - improved AutoDiscovery on "multi-homed" PCs 
13.8.19 - initial version

Code snippets TNX to: Peter Parэzek (USRP code for skim server), VE3NEA, W7AY,
openHPSDR list, Wireshark plugin: NH6Z ... ...  
too many to list :)

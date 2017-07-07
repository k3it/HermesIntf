@Rem Windows batch file that rebuilds HeremesIntf.dll from source using GNU MinGW C++ compiler
@Rem N6TV 6 July 2017 07:25 UTC
@g++ -O2 -s -static-libgcc -static-libstdc++ -shared -o HermesIntf.dll -DUNICODE -DHERMESINTF_EXPORTS Hermes.cpp HermesIntf.cpp HermesIntfMSVC.def -Wl,-lws2_32

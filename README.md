# pico
capture de données depuis un picoscope 5244b

# installation 
voir picosdk-c-wrappers-binaries dans https://github.com/picotech

# compilation
gcc frps5000a.c -o fr -lm -lps5000a -lpthread -Wno-format -Wl,-s -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

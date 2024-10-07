# pico
capture de données depuis un picoscope 5244b

# installation 
voir picosdk-c-wrappers-binaries dans https://github.com/picotech

# compilation
gcc frps5000a.c -o fr -lps5000a -lpthread -Wno-format -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

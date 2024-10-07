# Makefile généré par eclipse
# sacagé par fr

PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

OBJS = frps5000a.o
CFLAGS += -I/opt/picoscope/include
LDFLAGS += -L/opt/picoscope/lib -lps5000a -lpthread -u,pthread_atfork 
ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g
else ifeq ($(BUILD_MODE),run)
	CFLAGS += -O2
else ifeq ($(BUILD_MODE),linuxtools)
	CFLAGS += -g -pg -fprofile-arcs -ftest-coverage
	LDFLAGS += -pg -fprofile-arcs -ftest-coverage
	EXTRA_CLEAN += frps5000a.gcda frps5000a.gcno $(PROJECT_ROOT)gmon.out
	EXTRA_CMDS = rm -rf frps5000a.gcda
endif

all:	mano

fr:	$(OBJS)
	$(CXX) $(LDFLAGS) $(CFLAGS) -o $@ $^
	$(EXTRA_CMDS)

mano:
	gcc frps5000a.c -o fr -lps5000a -lpthread -Wno-format -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

%.o:	$(PROJECT_ROOT)%.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

%.o:	$(PROJECT_ROOT)%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -fr fr $(OBJS) $(EXTRA_CLEAN)

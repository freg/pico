OBJS = .o
OBJS_CAPTURE = capture.o
OBJS_ANALYSE = analyse.o

CFLAGS += -I/opt/picoscope/include
LDFLAGS += -L/opt/picoscope/lib -lps5000a -lpthread -u,pthread_atfork 
CFLAGS += -g -pg 
LDFLAGS += -pg 

all:	analyse capture

capture: $(OBJS_CAPTURE)
	$(CXX) $(LDFLAGS) $(CFLAGS) -o $@ $^

analyse: $(OBJS_ANALYSE)
	$(CXX) $(LDFLAGS) $(CFLAGS) -o $@ $^

mano:
	gcc frps5000a.c -o fr -lps5000a -lpthread -Wno-format -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

%.c:
	touch $@

%.o:	%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) 


OBJS_CAPTURE = capture.o
OBJS_ANALYSE = analyse.o
CC = gcc
CFLAGS += -I/opt/picoscope/include
LDFLAGS += -L/opt/picoscope/lib -lps5000a -lpthread -u,pthread_atfork  -Wno-format 

all:	analyse capture
	.

capture: $(OBJS_CAPTURE)
	$(CXX) $(LDFLAGS) $(CFLAGS) -o $@ $^

analyse: $(OBJS_ANALYSE)
	$(CXX) $(LDFLAGS) $(CFLAGS) -o $@ $^

mano:
	gcc capture.o -o capture -lps5000a -lpthread -Wno-format -u,pthread_atfork -L/opt/picoscope/lib -I/opt/picoscope/include

%.c:
	touch $@

%.o:	%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) 

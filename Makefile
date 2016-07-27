CC=g++
CFLAGS=-c -g -Wall
LDFLAGS=-lpthread
SOURCES=test_shmringbuffer.cc
OBJECTS=$(SOURCES:.cc=.o)
EXECUTABLE=shmringbuf

all: $(SOURCES) $(EXECUTABLE)
    
$(EXECUTABLE): $(OBJECTS) 
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

.cc.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)

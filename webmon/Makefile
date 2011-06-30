

SOURCES = ../llprofcommon/network.cpp http.cpp webmon.cpp data_store.cpp
LIBPATH = 
INCPATH = 

CFLAGS = -g -Wall $(INCPATH)
LDFLAGS = -lpthread
OUT = webmon


all: $(OUT)

clean:
	rm -f *.o
	rm -f $(OUT)

rebuild: clean all


$(OUT): $(SOURCES:.cpp=.o)
	g++ $(SOURCES:.cpp=.o) $(LDFLAGS) -o $(OUT)

.cpp.o:
	g++ $(CFLAGS) -c -o $(<:.cpp=.o) $< 




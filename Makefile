CXX=g++
CXXFLAGS=-fPIC -Wall

LIBDIR=-L lib
TARGET1=lib/liblog.so
TARGET2=lib/libtimer.so
TARGET3=lib/libhttpconn.so
TARGET4=lib/libthreadpool.so
TARGET=webserver

SOURCE1=log/log.cc log/log_level.cc log/block_queue.cc
SOURCE2=timer/util_timer.cc
SOURCE3=http_conn/http_conn.cc
SOURCE4=threadpool/threadpool.cc
SOURCE=main.cc

OBJECTS1=$(SOURCE1:.cc=.o)
OBJECTS2=$(SOURCE2:.cc=.o)
OBJECTS3=$(SOURCE3:.cc=.o)
OBJECTS4=$(SOURCE4:.cc=.o)
OBJECTS=$(SOURCE:.cc=.o)
SOME_FILE := formatlist.txt

all:
	@if [ ! -f $(SOME_FILE) ]; then \
        	echo "$(SOME_FILE) does not exist, which is for MIME indexing. Aborting."; \
        	exit 1; \
    	fi
	$(MAKE) build
build: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET4) $(TARGET)

main.o: main.cc
	$(CXX) -c main.cc -o main.o

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET1): $(OBJECTS1) | lib
	$(CXX) -shared $(OBJECTS1) -o $(TARGET1) -lpthread -D_LEVEL=ERROR

$(TARGET2): $(OBJECTS2) $(TARGET1) | lib
	$(CXX) -shared $(OBJECTS2) $(LIBDIR) -llog -o $(TARGET2)

$(TARGET3): $(OBJECTS3) $(TARGET1) | lib
	$(CXX) -shared $(OBJECTS3) $(LIBDIR) -llog -lpthread -o $(TARGET3)

$(TARGET4): $(OBJECTS4) | lib
	$(CXX) -shared $(OBJECTS4) -o $(TARGET4)

$(TARGET): $(OBJECTS) $(TARGET2) $(TARGET3) $(TARGET4) | lib
	$(CXX) $(OBJECTS) $(LIBDIR) -llog -ltimer -lhttpconn -lthreadpool -o $(TARGET)

lib:
	mkdir -p lib

clean:
	rm -rf $(OBJECT1) $(TARGET1) $(OBJECT2) $(TARGET2) $(OBJECTS3) $(TARGET3) $(OBJECTS) $(TARGET) $(OBJECT4) $(TARGET4) error* 20* lib *.gch

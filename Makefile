CXX=g++
CXXFLAGS=-fPIC -Wall

LIBDIR=-L lib
TARGET1=lib/liblog.so
TARGET2=lib/libtimer.so
TARGET3=lib/libhttpconn.so
TARGET=webserver

SOURCE1=log/log.cc log/log_level.cc log/block_queue.cc
SOURCE2=timer/util_timer.cc
SOURCE3=http_conn/http_conn.cc
SOURCE=main.cc

OBJECTS1=$(SOURCE1:.cc=.o)
OBJECTS2=$(SOURCE2:.cc=.o)
OBJECTS3=$(SOURCE3:.cc=.o)
OBJECTS=$(SOURCE:.cc=.o)

all: $(TARGET1) $(TARGET2) $(TARGET3) $(TARGET)

main.o: main.cc
	$(CXX) -c main.cc -o main.o
%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET1):$(OBJECTS1) | lib
	$(CXX)  -shared $(OBJECTS1) -o $(TARGET1) -lpthread -D_LEVEL=ERROR
$(TARGET2):$(OBJECTS2) $(TARGET1) | lib
	$(CXX)  -shared $(OBJECTS2) $(LIBDIR) -llog -o $(TARGET2)
$(TARGET3):$(OBJECTS3) $(TARGET1) | lib
	$(CXX)  -shared $(OBJECTS3) $(LIBDIR) -llog -lpthread -o $(TARGET3)
$(TARGET):$(OBJECTS) $(TARGET2) $(TARGET2) |lib
	$(CXX)  $(OBJECTS) $(LIBDIR) -llog -ltimer -lhttpconn  -o $(TARGET)
lib:
	mkdir -p lib

clean:
	rm -rf $(OBJECT1) $(TARGET1) $(OBJECT2) $(TARGET2) $(OBJECTS3) $(TARGET3) $(OBJECTS) $(TARGET) lib

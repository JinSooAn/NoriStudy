CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -lrdkafka++

# Include and library paths
CXXFLAGS += -I/usr/include/librdkafka

TARGETS = kafka_send kafka_recv

all: $(TARGETS)

kafka_send: kafka_send.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

kafka_recv: kafka_recv.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS) *.o

run_send: kafka_send
	./kafka_send

run_recv: kafka_recv
	./kafka_recv

.PHONY: all clean run_send run_recv

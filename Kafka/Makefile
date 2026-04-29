CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
LDFLAGS = -lrdkafka++

# Include and library paths
CXXFLAGS += -I/usr/include/librdkafka

KAFKA_SRCS = CKafka.cpp
KAFKA_OBJS = $(KAFKA_SRCS:.cpp=.o)
KAFKA_LIB = libckafka.a

TARGETS = kafka_send kafka_recv $(KAFKA_LIB)

all: $(TARGETS)

kafka_send: ../kafka_send.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

kafka_recv: ../kafka_recv.cpp
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

$(KAFKA_LIB): $(KAFKA_OBJS)
	ar rcs $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGETS) *.o

run_send: kafka_send
	./kafka_send

run_recv: kafka_recv
	./kafka_recv

.PHONY: all clean run_send run_recv

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

SRCS = main.cpp init.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = mygit

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.cpp mygit.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

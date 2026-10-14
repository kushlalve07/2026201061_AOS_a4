CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

# OpenSSL (SHA-1) is not on the default search path on macOS, so ask
# pkg-config where it is; fall back to plain -lcrypto when pkg-config is absent.
OPENSSL_CFLAGS := $(shell pkg-config --cflags libcrypto 2>/dev/null)
OPENSSL_LIBS := $(shell pkg-config --libs libcrypto 2>/dev/null || echo -lcrypto)

CXXFLAGS += $(OPENSSL_CFLAGS)
LDLIBS = $(OPENSSL_LIBS) -lz

SRCS = main.cpp init.cpp objects.cpp hash_object.cpp cat_file.cpp write_tree.cpp ls_tree.cpp index.cpp add.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = mygit

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDLIBS)

%.o: %.cpp mygit.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

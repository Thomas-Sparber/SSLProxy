CXX=g++

CXXFLAGS=-Wall -Wextra -Weffc++
LDFLAGS=-lssl -lcrypto

ifeq ($(OS),Windows_NT)
	LDFLAGS+=-lws2_32 -lwsock32
endif

TESTS=\
	test_blocking \
	test_nonblocking \
	test_boost \
	test_crow

all: $(TESTS)

%: src/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTS)

.PHONY: all clean

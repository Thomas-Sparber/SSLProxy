CXX=g++

CXXFLAGS=-Wall -Wextra -Weffc++
LDFLAGS=-lssl -lcrypto

TESTS=\
	test_blocking \
	test_nonblocking \
	test_crow

all: $(TESTS)

%: src/%.cpp
	$(CXX) $(CXXFLAGS) $< -o $@ $(LDFLAGS)

clean:
	rm -f $(TESTS)

.PHONY: all clean

override CXXFLAGS := -std=gnu++0x -Wall -Wextra -Wold-style-cast -Wconversion -Wundef -O2 -g $(shell pkg-config --cflags gtkmm-2.4 | sed 's/-I/-isystem /g') $(CXXFLAGS)
override LDFLAGS := $(shell pkg-config --libs-only-L --libs-only-other gtkmm-2.4)
override LDLIBS := $(shell pkg-config --libs-only-l gtkmm-2.4)

.PHONY : world
world : sslrefbox

sslrefbox : $(patsubst %.cc,%.o,$(wildcard *.cc))
	$(CXX) $(LDFLAGS) -o $@ $+ $(LDLIBS)

$(patsubst %.cc,%.o,$(wildcard *.cc)) : $(wildcard *.h)

.PHONY : clean
clean :
	$(RM) *.o sslrefbox

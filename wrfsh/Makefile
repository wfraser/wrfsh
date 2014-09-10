CXXFLAGS=-std=c++1y -Wall -Werror -pedantic

OBJS=main.o repl.o global_state.o

all: wrfsh

wrfsh: $(OBJS)
	g++ -o wrfsh $(LDFLAGS) $(OBJS)

clean:
	rm -f *.o wrfsh

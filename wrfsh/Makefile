CXXFLAGS=-std=c++11 -Wall -Werror -pedantic

OBJS=main.o repl.o

all: wrfsh

wrfsh: $(OBJS)
	g++ -o wrfsh $(LDFLAGS) $(OBJS)

clean:
	rm -f *.o wrfsh

CXXFLAGS+=-std=c++1y -Wall -Werror -pedantic -g
LFLAGS+=-Wall -lstdc++ -lm -lpthread

OBJS=main.o repl.o global_state.o commandlets.o if_else_endif.o process.o process_posix.o stream_ex.o console.o console_posix.o

all: wrfsh

wrfsh: $(OBJS)
	$(CXX) $(OBJS) $(LFLAGS) -o wrfsh

clean:
	rm -f *.o wrfsh

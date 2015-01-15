CXXFLAGS+=-std=c++1y -Wall -Werror -pedantic -g
LFLAGS+=-Wall -lstdc++

OBJS=main.o

all: rawkeys

rawkeys: $(OBJS)
	$(CXX) $(OBJS) $(LFLAGS) -o rawkeys

clean:
	rm -f *.o rawkeys

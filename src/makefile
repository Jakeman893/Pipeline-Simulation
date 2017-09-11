SIM_SRC  = sim.cpp pipeline.cpp bpred.cpp 
SIM_OBJS = $(SIM_SRC:.cpp=.o)

all: $(SIM_SRC) sim

debug:
	g++ -g sim.cpp -g pipeline.cpp -g bpred.cpp -o sim

%.o: %.c 
	g++ -c -o $@ $<  

sim: $(SIM_OBJS) 
	g++ -o $@ $^

clean: 
	rm sim *.o

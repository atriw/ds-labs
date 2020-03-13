# NOTE: Feel free to change the makefile to suit your own need.

# compile and link flags
CCFLAGS = -Wall -g
LDFLAGS = -Wall -g

GBNFLAG = -DGBN=1
SRFLAG = -DGBN=0

SRC = $(wildcard *.cc)

# make rules
TARGETS = rdt_sim 

all: $(TARGETS)

gbn: $(SRC)
	g++ $(CCFLAGS) $(GBNFLAG) -o $(TARGETS) $^

sr: $(SRC)
	g++ $(CCFLAGS) $(SRFLAG) -o $(TARGETS) $^

run-gbn-%: gbn
	./$(TARGETS) 1000 0.1 100 $* $* $* 0

run-sr-%: sr
	./$(TARGETS) 1000 0.1 100 $* $* $* 0

.cc.o:
	g++ $(CCFLAGS) -c -o $@ $<

util.o: rdt_struct.h util.h

rdt_sender.o: 	rdt_struct.h rdt_sender.h util.h

rdt_receiver.o:	rdt_struct.h rdt_receiver.h util.h

rdt_sim.o: 	rdt_struct.h

rdt_sim: rdt_sim.o rdt_sender.o rdt_receiver.o util.o
	g++ $(LDFLAGS) -o $@ $^

clean:
	rm -f *~ *.o $(TARGETS)

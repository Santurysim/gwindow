CFLAGS = -g -O0 -Wall -Iinclude
CXX = g++ $(CFLAGS)

.PHONY: clean create-dirs

graphtst: src/graphtst.cpp R2Graph.o include/R2Graph.h
	$(CXX) -o bin/graphtst src/graphtst.cpp obj/R2Graph.o

R2Graph.o: src/R2Graph.cpp include/R2Graph.h create-dirs
	$(CXX) -c src/R2Graph.cpp -o obj/R2Graph.o

create-dirs:
	mkdir -p obj bin

clean:
	rm -f bin/* obj/*

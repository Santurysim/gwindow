CFLAGS= -g -O0 -Wall -I/usr/X11R6/include -L/usr/X11R6/lib -L./bin/ -Iinclude/ -I./R2Graph/include 
CXX= g++ $(CFLAGS)

.PHONY: install uninstall clean all create-dirs

all: libgwindow.so

samples: func gclock mondrian bezier cursTst react

func: func.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/func obj/func.o R2Graph/obj/R2Graph.o -lX11 -lgwindow

gclock: clock.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/gclock obj/clock.o R2Graph/obj/R2Graph.o -lX11 -lgwindow

mondrian: mondrian.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/mondrian obj/mondrian.o R2Graph/obj/R2Graph.o -lX11 -lgwindow

bezier: bezier.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/bezier obj/bezier.o R2Graph/obj/R2Graph.o -lX11 -lgwindow

cursTst: cursTst.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/cursTst obj/cursTst.o R2Graph/obj/R2Graph.o -lX11 -lgwindow

react: react.o libgwindow.so R2Graph/R2Graph.o
	$(CXX) -o bin/react obj/react.o R2Graph/obj/R2Graph.o -lX11 -lrt -lgwindow

gwindow.o: src/gwindow.cpp gwindow.h create-dirs
	$(CXX) -fPIC -c src/gwindow.cpp -o obj/gwindow.o

func.o: src/func.cpp gwindow.h
	$(CXX) -c src/func.cpp -o obj/func.o

clock.o: src/clock.cpp gwindow.h
	$(CXX) -c src/clock.cpp -o obj/clock.o

mondrian.o: src/mondrian.cpp gwindow.h
	$(CXX) -c src/mondrian.cpp -o obj/mondrian.o

bezier.o: src/bezier.cpp gwindow.h
	$(CXX) -c src/bezier.cpp -o obj/bezier.o

cursTst.o: src/cursTst.cpp gwindow.h
	$(CXX) -c src/cursTst.cpp -o obj/cursTst.o

react.o: src/react.cpp gwindow.h
	$(CXX) -c src/react.cpp -o obj/react.o

R2Graph/R2Graph.o:
	cd R2Graph; make R2Graph.o; cd ..

gwindow.h: R2Graph/include/R2Graph.h include/gwindow.h

grtst: src/grtst.cpp libgwindow.so
	$(CXX) -o bin/grtst src/grtst.cpp -lX11 -lgwindow

libgwindow.so: gwindow.o
	$(CXX) -shared obj/gwindow.o -o bin/libgwindow.so

install: bin/libgwindow.so
	install -m755 bin/libgwindow.so /usr/lib
	install -m755 include/gwindow.h /usr/include

uninstall:
	rm -f /usr/lib/libgwindow.so
	rm -f /usr/include/gwindow.h

create-dirs:
	mkdir -p bin obj

clean:
	rm -f bin/* obj/*
	cd R2Graph; make clean; cd ..

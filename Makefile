CFLAGS= -g -O0 -Wall -I/usr/X11R6/include -L/usr/X11R6/lib -L./lib/ -I.. -I.
CXX= g++ $(CFLAGS)

.PHONY: install uninstall clean 

all: bin/func bin/gclock bin/mondrian bin/bezier bin/cursTst bin/react lib/libgwindow.so

bin/func: func.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/func func.o gwindow.o R2Graph/R2Graph.o -lX11

bun/gclock: clock.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/gclock clock.o gwindow.o R2Graph/R2Graph.o -lX11

bin/mondrian: mondrian.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/mondrian mondrian.o gwindow.o R2Graph/R2Graph.o -lX11

bin/bezier: bezier.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/bezier bezier.o gwindow.o R2Graph/R2Graph.o -lX11

bin/cursTst: cursTst.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/cursTst cursTst.o gwindow.o R2Graph/R2Graph.o -lX11

bin/react: react.o gwindow.o R2Graph/R2Graph.o
	$(CXX) -o bin/react react.o gwindow.o R2Graph/R2Graph.o -lX11 -lrt

gwindow.o: gwindow.cpp gwindow.h
	$(CXX) -fPIC -c gwindow.cpp

func.o: func.cpp gwindow.h
	$(CXX) -c func.cpp

clock.o: clock.cpp gwindow.h
	$(CXX) -c clock.cpp

mondrian.o: mondrian.cpp gwindow.h
	$(CXX) -c mondrian.cpp

besier.o: besier.cpp gwindow.h
	$(CXX) -c besier.cpp

cursTst.o: cursTst.cpp gwindow.h
	$(CXX) -c cursTst.cpp

react.o: react.cpp gwindow.h
	$(CXX) -c react.cpp

R2Graph/R2Graph.o:
	cd R2Graph; make R2Graph.o; cd ..

gwindow.h: R2Graph/R2Graph.h

grtst: grtst.cpp gwindow.o
	$(CXX) -o grtst grtst.cpp gwindow.o -lX11

libgwindow.so: gwindow.o
	$(CXX) -shared gwindow.o -o libgwindow.so

install: libgwindow.so
	install -m755 libgwindow.so /usr/lib

uninstall: install
	rm -f /usr/lib/libgwindow.so

clean:
	rm -f *.o *.so func gclock mondrian bezier grtst cursTst react *\~
	cd R2Graph; make clean; cd ..

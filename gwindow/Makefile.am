#noinst_LIBRARIES = libgwindow.a
#libgwindow_a_SOURCES = gwindow.cpp gwindow.h

AM_CXXFLAGS = -I${srcdir}/../r2graph

lib_LTLIBRARIES = libgwindow.la
libgwindow_la_SOURCES = gwindow.cpp
#libgwindow_la_CFLAGS = $(AM_CFLAGS)
include_HEADERS = gwindow.h
libgwindow_la_LIBADD = ../r2graph/libr2graph.la
libgwindow_la_LDFLAGS = -lxcb -lxcb-util -lxcb-keysyms

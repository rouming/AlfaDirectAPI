TARGET = ADAPIServer
QT -= core gui network
CONFIG += staticlib warn_on console

LEVEL = ../..

!include($$LEVEL/AlfaDirectAPI.pri):error("Can't load AlfaDirectAPI.pri")

TEMPLATE = lib

SOURCES = ADAPIServer.cpp
INCLUDEPATH += $$LEVEL/ADAPI/include

QMAKE_CXXFLAGS = -m32
QMAKE_CCFLAGS = -m32

TARGET = ADAPIServer.exe
QT -= core gui network
TEMPLATE = app
CONFIG += warn_on console

LEVEL = ../..

QMAKE_CC   = winegcc
QMAKE_LINK = wineg++
QMAKE_CXX  = wineg++

QMAKE_CXXFLAGS = -m32
QMAKE_CCFLAGS = -m32
QMAKE_LFLAGS = -m32

WINE_BUILD = 1

!include($$LEVEL/AlfaDirectAPI.pri):error("Can't load AlfaDirectAPI.pri")

INCLUDEPATH += \
               $$LEVEL/src \
               $$LEVEL/ADAPI/include \

SOURCES = \
          Main.cpp \
          ADAPIServer.cpp \
          $$LEVEL/src/ADRPC.cpp \
          $$LEVEL/src/ADLocalLibrary.cpp \
          $$LEVEL/src/ADDynaLoader.cpp

QMAKE_LFLAGS += -Wl,-rpath,$$DESTDIR

DEFINED -= _LIN_=1

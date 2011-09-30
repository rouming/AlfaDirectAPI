TARGET = AlfaDirectAPI
QT -= gui
QT += core network xml sql
CONFIG += warn_on console

LEVEL = ..

!include($$LEVEL/AlfaDirectAPI.pri):error("Can't load AlfaDirectAPI.pri")

TEMPLATE = lib

INCLUDEPATH += \
           $$LEVEL/ADSDK \
           $$LEVEL/ADAPI/include

HEADERS += \
           ADConnection.h \
           ADSubscription.h \
           ADOrder.h \
           ADLibrary.h \
           ADSmartPtr.h \
           ADAtomicOps.h \

SOURCES += \
           ADConnection.cpp \
           ADSubscription.cpp \
           ADOrder.cpp

win32:SOURCES += \
           ADLocalLibrary.cpp \
           ADDynaLoader.cpp

unix:SOURCES += \
           ADRemoteLibrary.cpp

RESOURCES += $$LEVEL/AlfaDirectAPI.qrc

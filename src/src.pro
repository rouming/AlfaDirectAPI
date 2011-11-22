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
           ADTemplateParser.h \

SOURCES += \
           ADConnection.cpp \
           ADSubscription.cpp \
           ADOrder.cpp \
           ADBootstrap.cpp \
           ADTemplateParser.cpp \

win32:SOURCES += \
           ADLocalLibrary.cpp \
           ADDynaLoader.cpp \
           ADBootstrap.h

unix:SOURCES += \
           ADRemoteLibrary.cpp

RESOURCES += $$LEVEL/AlfaDirectAPI.qrc

# Include ADAPIServer resources for any Unix.
# Unix we must use special server which loads
# ADAPI.dll (AlfaDirect black box API) with wine,
# and we include this server as resources to
# bootstrap and run process on AD connect.
unix {
   # To make RCC (resource compiler) happy we should create empty files.
   # Of course this files will be created by winegcc after successful
   # ADAPIServer build.
   !exists($$DESTDIR/ADAPIServer.exe):system(touch $$DESTDIR/ADAPIServer.exe > /dev/null 2>&1)
   !exists($$DESTDIR/ADAPIServer.exe.so):system(touch $$DESTDIR/ADAPIServer.exe.so > /dev/null 2>&1)
   RESOURCES += $$LEVEL/ADAPIServer/ADAPIServer.qrc
}

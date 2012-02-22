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
           ADCryptoAPI.h \

SOURCES += \
           ADConnection.cpp \
           ADSubscription.cpp \
           ADOrder.cpp \
           ADBootstrap.cpp \
           ADTemplateParser.cpp \
           ADCryptoAPI.cpp \

win32:SOURCES += \
           ADLocalLibrary.cpp \
           ADDynaLoader.cpp \
           ADBootstrap.h

unix:SOURCES += \
           ADRemoteLibrary.cpp \
           ADRPC.cpp \

# Check that CryptoPRO exists on Linux
linux-*:exists( /opt/cprocsp/include/cpcsp ) {

  DEFINES += UNIX
  DEFINES += LINUX
  DEFINES += _CRYPTOAPI_

  contains(QT_ARCH, x86_64): {
      DEFINES += SIZEOF_VOID_P=8
      CRYPTODIR = amd64
  }
  else {
     DEFINES += SIZEOF_VOID_P=4
     CRYPTODIR = ia32
  }

  INCLUDEPATH += \
             /opt/cprocsp/include \
             /opt/cprocsp/include/cpcspr \
             /opt/cprocsp/include/cpcsp

  QMAKE_LIBDIR += \
            /opt/cprocsp/lib/$$CRYPTODIR

  QMAKE_LFLAGS += \
           -Wl,-rpath,/opt/cprocsp/lib/$$CRYPTODIR

  LIBS += -lssp -lcapi20 -lcapi10 -lcpext -lrdrsup -lasn1data
}

win32:LIBS += -lAdvapi32  -lCrypt32

RESOURCES += $$LEVEL/AlfaDirectAPI.qrc

# Include ADAPIServer resources for any Unix.
# For Unix we must use special server which loads
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

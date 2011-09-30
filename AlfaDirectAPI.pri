# Version definition
ADAPI_VERSION=0.1.0

# Define your own build rules
exists(Subsidiary.pri):include(Subsidiary.pri)

CONFIG += debug_and_release

DEFINES += "ADAPI_VERSION=$$ADAPI_VERSION"

# Visual Studio 8 introduced a strange way to force developers
# move to the Microsoft-Specific version of the Standatd
# We're avoiding all these warnings with this define
win32:DEFINES += _CRT_SECURE_NO_DEPRECATE

# Platform specific macroses
isEmpty( WINE_BUILD ) {
  win32:DEFINES += "_WIN_=1"
  mac:DEFINES += "_MAC_=1"
  linux-*:DEFINES += "_LIN_=1"

  contains(QT_ARCH, x86_64):DEFINES += "_AMD64_"
  else::DEFINES += "_X86_"
}
else {
  DEFINES += "_WIN_=1"
  DEFINES += "_X86_"
}

!isEmpty(BUILD_NAME) {
     DESTDIR = $$join(BUILD_NAME, "", $$LEVEL/build/)
     QMAKE_LIBDIR += $$DESTDIR
     MOC_DIR = $$join(TARGET, "", $$DESTDIR/mocs/)
     OBJECTS_DIR = $$join(TARGET, "", $$DESTDIR/obj/)
     UI_DIR = $$DESTDIR/ui
}

INCLUDEDIR += $UI_DIR

CONFIG(debug, debug|release) {
   CONFIG += console

   # Define macros DEBUG for Unix
   unix: !contains( DEFINES, DEBUG ): DEFINES += DEBUG
}

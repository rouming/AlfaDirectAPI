TEMPLATE = subdirs

LEVEL = .

!include($$LEVEL/AlfaDirectAPI.pri):error("Can't load AlfaDirectAPI.pri")

unix:SUBDIRS += ADAPIServer
SUBDIRS += src

# build must be last:
CONFIG += ordered

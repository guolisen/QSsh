include(../ssh.pri)

QT += gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET=sftpfsmodel
SOURCES+=main.cpp window.cpp
HEADERS+=window.h
FORMS=window.ui
LIBS += C:/Botan/lib/botan.lib

#Enable debug log
#DEFINES += CREATOR_SSH_DEBUG

INCLUDEPATH += C:/Botan/include/botan-2

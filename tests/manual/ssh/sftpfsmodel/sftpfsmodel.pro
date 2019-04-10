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
INCLUDEPATH += Thirdparty/Lua/src \
               E:/code/qt/quazip/quazip/quazip \
               E:/"Program Files"/zlib/include \
               E:/code/qt/qssh/MyQssh/QSsh/src/libs

LIBS += E:/"Program Files"/zlib/lib/zlibstaticd.lib \
        E:/code/qt/quazip/build-quazip-Desktop_Qt_5_12_1_MSVC2017_64bit-Debug/quazip/debug/quazipd.lib \
        E:/code/qt/qssh/MyQssh/build-qssh-Desktop_Qt_5_12_1_MSVC2017_64bit-Debug/lib/QSshd.lib

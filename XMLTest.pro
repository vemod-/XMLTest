#-------------------------------------------------
#
# Project created by QtCreator 2011-08-05T00:31:14
#
#-------------------------------------------------

QT       += core gui widgets

CONFIG += c++11

DEFINES += QT_USE_QSTRINGBUILDER

LIBS += -framework CoreFoundation

INCLUDEPATH += ../ObjectComposerXML
INCLUDEPATH += ../SoftSynths/SoftSynthsClasses
INCLUDEPATH += ../QDomLite

DEFINES += __MACOSX_CORE__

#include(MusicXML.pri)
include($$PWD/../quazip/quazip.pri)

TARGET = XMLTest
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    $$PWD/../ObjectComposerXML/ocxmlwrappers.cpp \
    $$PWD/../SoftSynths/SoftSynthsClasses/cpitchtextconvert.cpp \
    $$PWD/cmusicxml.cpp

HEADERS  += mainwindow.h \
    $$PWD/../ObjectComposerXML/ocxmlwrappers.h \
    $$PWD/../SoftSynths/SoftSynthsClasses/cpitchtextconvert.h \
    $$PWD/cmusicxml.h

FORMS    += mainwindow.ui

DISTFILES += \
    MusicXML.pri

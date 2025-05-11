!contains(PROFILES,$$_FILE_){
PROFILES+=$$_FILE_

CONFIG += sdk_no_version_check

include($$PWD/../quazip/quazip.pri)

INCLUDEPATH += $$PWD
INCLUDEPATH += $$PWD/../ObjectComposerXML
INCLUDEPATH += $$PWD/../SoftSynths/SoftSynthsClasses
INCLUDEPATH += $$PWD/../QDomLite

SOURCES += \
    #$$PWD/../ObjectComposerXML/ocxmlwrappers.cpp \
    #$$PWD/../SoftSynths/SoftSynthsClasses/cpitchtextconvert.cpp \
    $$PWD/cmusicxml.cpp

HEADERS  += \
    #$$PWD/../ObjectComposerXML/ocxmlwrappers.h \
    #$$PWD/../SoftSynths/SoftSynthsClasses/cpitchtextconvert.h \
    $$PWD/cmusicxml.h
}

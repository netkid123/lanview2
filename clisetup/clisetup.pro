QT -= gui
QT += sql network xml

CONFIG += c++11 console
CONFIG -= app_bundle
CONFIG -= debug_and_release

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

TEMPLATE = app

SOURCES += \
        main.cpp

INCLUDEPATH += ../lv2
LIBS += -L../lv2 -llv2

TRANSLATIONS    = clisetup_hu.ts \
                  clisetup_en.ts

CODECFORSRC     = UTF-8

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

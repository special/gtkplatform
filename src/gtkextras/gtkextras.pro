TARGET = QtGtkExtras
QT -= gui
QT += widgets gui-private

HEADERS += \
    qgtkrefptr.h \
    qgtkheaderbar.h

SOURCES += \
    qgtkheaderbar.cpp

CONFIG += link_pkgconfig
PKGCONFIG_PRIVATE += gtk+-4.0

CONFIG += no_keywords

load(qt_module)
CONFIG -= create_cmake # should be fixed, but ...

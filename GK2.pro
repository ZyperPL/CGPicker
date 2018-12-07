INCLUDEPATH += ./src ./include

SOURCES += ./src/*.cpp
HEADERS += ./include/*.hpp

QT += core gui widgets

RESOURCES     = resources.qrc

DESTDIR = .
OBJECTS_DIR=obj/objects/
MOC_DIR=obj/
RCC_DIR=obj/

######################################################################
# Automatically generated by qmake (2.01a) ven. juin 8 21:29:34 2007
######################################################################

TEMPLATE = lib
DESTDIR = ..
CONFIG += staticlib release 

!debug {
  TARGET = qcodeedit
  MOC_DIR = .build
  OBJECTS_DIR = .build
}
debug {
  TARGET = qcodeeditd
  MOC_DIR = .buildd
  OBJECTS_DIR = .buildd
}

DEPENDPATH += . document language widgets qnfa
INCLUDEPATH += . document language widgets qnfa

CONFIG += qnfa

QT += xml

UI_DIR = 


# Input
include(lib.pri)

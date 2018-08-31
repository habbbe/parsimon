TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CXXFLAGS += -std=c++17
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O3

SOURCES += main.cpp

HEADERS += \
    parser.h \
    monad.h \
    lazy.h \
    parser_core.h \
    parser_combinators.h \
    parser_parsers.h \
    json_parser.h \
    parser_result.h \
    parser_state.h \
    parser_settings.h \
    parse_algorithm.h \
    time_measure.h

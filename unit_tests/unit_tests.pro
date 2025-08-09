QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle
CONFIG += c++20

TEMPLATE = app

SOURCES +=  \
    bitboard_test.cpp \
    main_test.cpp \
    mask_gen_test.cpp \
    move_test.cpp \
    pieces_test.cpp \
    \
    ../ChessBot/board_state/pieces.cpp \
    ../ChessBot/board_state/bitboard.cpp \
    ../ChessBot/board_state/move.cpp \
    ../ChessBot/board_state/zobrist_hash.cpp \
    ../ChessBot/board_state/position.cpp \
    ../ChessBot/board_state/repetition_history.cpp\
    ../ChessBot/move_generation/ps_legal_move_mask_gen.cpp \
    position_test.cpp \
    zobrist_hash_test.cpp

HEADERS += \
    bitboard_test.h \
    mask_gen_test.h \
    move_test.h \
    pieces_test.h \
    position_test.h \
    zobrist_hash_test.h

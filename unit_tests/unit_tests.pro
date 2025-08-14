QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle
CONFIG += c++20

TEMPLATE = app

SOURCES +=  \
    ../ChessBot/board_state/pieces.cpp \
    ../ChessBot/board_state/bitboard.cpp \
    ../ChessBot/board_state/move.cpp \
    ../ChessBot/board_state/zobrist_hash.cpp \
    ../ChessBot/board_state/position.cpp \
    ../ChessBot/board_state/repetition_history.cpp\
    ../ChessBot/move_generation/ps_legal_move_mask_gen.cpp \
    ../ChessBot/move_generation/move_list.cpp \
    ../ChessBot/move_generation/legal_move_gen.cpp \
    ../ChessBot/ai_logic/evaluation.cpp \
    ../ChessBot/ai_logic/move_ordering.cpp \
    ../ChessBot/ai_logic/static_exchange_evaluation.cpp \
    ../ChessBot/ai_logic/search.cpp \
    ../ChessBot/ai_logic/transposition_table.cpp \
    \
    bitboard_test.cpp \
    evaluation_test.cpp \
    legal_move_gen_test.cpp \
    legal_move_gen_tester.cpp \
    main_test.cpp \
    mask_gen_test.cpp \
    move_ordering_test.cpp \
    move_test.cpp \
    pieces_test.cpp \
    position_test.cpp \
    search_engine_test.cpp \
    see_test.cpp \
    transposition_table_test.cpp \
    zobrist_hash_test.cpp

HEADERS += \
    bitboard_test.h \
    evaluation_test.h \
    legal_move_gen_test.h \
    legal_move_gen_tester.h \
    mask_gen_test.h \
    move_ordering_test.h \
    move_test.h \
    pieces_test.h \
    position_test.h \
    search_engine_test.h \
    see_test.h \
    transposition_table_test.h \
    zobrist_hash_test.h

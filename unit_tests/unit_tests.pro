QT += testlib
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle
CONFIG += c++20

TEMPLATE = app

SOURCES +=  \
    ../ChessBot/src/engine_core/board_state/pieces.cpp \
    ../ChessBot/src/engine_core/board_state/bitboard.cpp \
    ../ChessBot/src/engine_core/board_state/move.cpp \
    ../ChessBot/src/engine_core/board_state/zobrist_hash.cpp \
    ../ChessBot/src/engine_core/board_state/position.cpp \
    ../ChessBot/src/engine_core/board_state/repetition_history.cpp\
    ../ChessBot/src/engine_core/move_generation/ps_legal_move_mask_gen.cpp \
    ../ChessBot/src/engine_core/move_generation/move_list.cpp \
    ../ChessBot/src/engine_core/move_generation/legal_move_gen.cpp \
    ../ChessBot/src/engine_core/ai_logic/evaluation.cpp \
    ../ChessBot/src/engine_core/ai_logic/move_ordering.cpp \
    ../ChessBot/src/engine_core/ai_logic/static_exchange_evaluation.cpp \
    ../ChessBot/src/engine_core/ai_logic/search.cpp \
    ../ChessBot/src/engine_core/ai_logic/transposition_table.cpp \
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

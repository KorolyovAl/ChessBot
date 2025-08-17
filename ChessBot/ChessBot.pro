QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ai_logic/evaluation.cpp \
    ai_logic/move_ordering.cpp \
    ai_logic/search.cpp \
    ai_logic/static_exchange_evaluation.cpp \
    ai_logic/transposition_table.cpp \
    board_state/bitboard.cpp \
    board_state/move.cpp \
    board_state/pieces.cpp \
    board_state/position.cpp \
    board_state/repetition_history.cpp \
    board_state/zobrist_hash.cpp \
    move_generation/legal_move_gen.cpp \
    move_generation/move_list.cpp \
    move_generation/ps_legal_move_mask_gen.cpp \
    user_interface/main.cpp \
    user_interface/mainwindow.cpp

HEADERS += \
    ai_logic/evaluation.h \
    ai_logic/move_ordering.h \
    ai_logic/piece_values.h \
    ai_logic/pst_tables.h \
    ai_logic/search.h \
    ai_logic/static_exchange_evaluation.h \
    ai_logic/transposition_table.h \
    board_state/bitboard.h \
    board_state/move.h \
    board_state/pieces.h \
    board_state/position.h \
    board_state/repetition_history.h \
    board_state/zobrist_hash.h \
    move_generation/king_masks.h \
    move_generation/knight_masks.h \
    move_generation/legal_move_gen.h \
    move_generation/move_list.h \
    move_generation/pawn_attack_masks.h \
    move_generation/ps_legal_move_mask_gen.h \
    move_generation/sliders_masks.h \
    user_interface/mainwindow.h \
    utils/bitboard_debug.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    utils/Structure_of_project

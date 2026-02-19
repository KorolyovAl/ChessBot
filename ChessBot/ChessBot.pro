QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++20

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/engine_core/ai_logic/evaluation.cpp \
    src/engine_core/ai_logic/move_ordering.cpp \
    src/engine_core/ai_logic/search.cpp \
    src/engine_core/ai_logic/static_exchange_evaluation.cpp \
    src/engine_core/ai_logic/transposition_table.cpp \
    src/engine_core/board_state/bitboard.cpp \
    src/engine_core/board_state/move.cpp \
    src/engine_core/board_state/pieces.cpp \
    src/engine_core/board_state/position.cpp \
    src/engine_core/board_state/repetition_history.cpp \
    src/engine_core/board_state/zobrist_hash.cpp \
    src/engine_core/move_generation/legal_move_gen.cpp \
    src/engine_core/move_generation/move_list.cpp \
    src/engine_core/move_generation/ps_legal_move_mask_gen.cpp \
    src/game_controller/game_controller.cpp \
    src/user_interface/board_widget.cpp \
    src/user_interface/game_controller_qt.cpp \
    src/user_interface/main.cpp \
    src/user_interface/mainwindow.cpp

HEADERS += \
    src/engine_core/ai_logic/evaluation.h \
    src/engine_core/ai_logic/move_ordering.h \
    src/engine_core/ai_logic/piece_values.h \
    src/engine_core/ai_logic/pst_tables.h \
    src/engine_core/ai_logic/search.h \
    src/engine_core/ai_logic/static_exchange_evaluation.h \
    src/engine_core/ai_logic/transposition_table.h \
    src/engine_core/board_state/bitboard.h \
    src/engine_core/board_state/move.h \
    src/engine_core/board_state/pieces.h \
    src/engine_core/board_state/position.h \
    src/engine_core/board_state/repetition_history.h \
    src/engine_core/board_state/zobrist_hash.h \
    src/engine_core/move_generation/king_masks.h \
    src/engine_core/move_generation/knight_masks.h \
    src/engine_core/move_generation/legal_move_gen.h \
    src/engine_core/move_generation/move_list.h \
    src/engine_core/move_generation/pawn_attack_masks.h \
    src/engine_core/move_generation/ps_legal_move_mask_gen.h \
    src/engine_core/move_generation/sliders_masks.h \
    src/game_controller/game_controller.h \
    src/user_interface/board_widget.h \
    src/user_interface/game_controller_qt.h \
    src/user_interface/mainwindow.h \
    src/utils/bitboard_debug.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    Structure_of_project

RESOURCES += \
    source.qrc

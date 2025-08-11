/************************************************
* LegalMoveGen — генерация ЛЕГАЛЬНЫХ ходов
*
* Подход:
* 1) Генерим псевдолегальные маски через PsLegalMaskGen
*    ИЛИ (для пешек-взятия) напрямую из PawnMasks::kAttack[from].
* 2) Конвертируем маски → Move и проверяем легальность
*    на копии Pieces (снимаем/ставим фигуры, EP/рокировки/промо),
*    затем SquareInDanger для своего короля.
*
* Зачем отдельно обрабатываем взятия пешек:
* — Это избавляет от «дельт» и wrap-ошибок по фалам.
* — “from” всегда известен (итерируем сами пешки),
*   и «этот from действительно бьёт этот to».
************************************************/

#pragma once

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "move_list.h"

class LegalMoveGen {
public:
    // Заполняет 'out' всеми ЛЕГАЛЬНЫМИ ходами для заданной стороны.
    // only_captures == true → только взятия (включая en passant).
    static void Generate(const Position& position, Side side, MoveList& out, bool only_captures = false);

private:
    /* Конвертеры масок → ходы для НЕ пешек */
    static void PiecesMaskToMoves(const Pieces& pcs, Bitboard to_mask,
                                  uint8_t from_sq, PieceType attacker_type,
                                  Side attacker_side, MoveList& out);

    /* Пешечные ходы: отдельные простые пути, без «дельт» */
    static void GenPawnCaptures (const Pieces& pcs, Side side, MoveList& out);
    static void GenPawnPushes  (const Pieces& pcs, Side side, MoveList& out);

    /* Проверка легальности после применения хода на КОПИИ Pieces */
    static bool IsLegalAfterMove(Pieces pcs, const Move& move);

    /* Спец-ходы */
    static void AddEnPassantCaptures(const Pieces& pcs, Side side, uint8_t ep_square, MoveList& out);
    static void AddCastlingMoves   (const Pieces& pcs, Side side,
                                 bool long_castle, bool short_castle, MoveList& out);

    static inline void TryPushMove(const Pieces& pcs, MoveList& out,
                                   uint8_t from, uint8_t to,
                                   PieceType attacker_type, Side attacker_side,
                                   uint8_t defender_type, uint8_t defender_side,
                                   Move::Flag flag);
};

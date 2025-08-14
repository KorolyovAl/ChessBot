#include "position.h"
#include "../move_generation/pawn_attack_masks.h"

#include <iostream>
#include <cmath>

/**********************************************
* Constructs position from short FEN and flags.
**********************************************/
Position::Position(const std::string& short_fen, uint8_t en_passant,
                   bool white_long, bool white_short,
                   bool black_long, bool black_short,
                   uint16_t move_counter)
    : pieces_(short_fen),
    en_passant_(en_passant),
    white_long_castling_(white_long),
    white_short_castling_(white_short),
    black_long_castling_(black_long),
    black_short_castling_(black_short),
    move_counter_(move_counter),
    hash_(pieces_, /*black_to_move=*/false, white_long, white_short, black_long, black_short)
{
    // если начинать должен чёрный
    if (!IsWhiteToMove()) {
        hash_.InvertMove();
    }
}

void Position::ApplyMove(Move move, Undo& u) {
    u.EnPassantBefore  = en_passant_;
    u.WhiteLongBefore  = white_long_castling_;
    u.WhiteShortBefore = white_short_castling_;
    u.BlackLongBefore  = black_long_castling_;
    u.BlackShortBefore = black_short_castling_;
    u.FiftyBefore      = fifty_move_counter_;
    u.MoveCounterBefore= move_counter_;
    u.CapturedType     = Move::None;
    u.CapturedSide     = Move::None;
    u.CapturedSquare   = NONE;
    u.RookFrom = u.RookTo = NONE;

    if (!BOp::GetBit(pieces_.GetPieceBitboard(
                         static_cast<Side>(move.GetAttackerSide()),
                         static_cast<PieceType>(move.GetAttackerType())), move.GetFrom())) {
        return;
    }

    // Снять EP-бит предыдущего хода из хэша, если он был захватываем для текущей стороны
    if (en_passant_ != NONE) {
        const Side stm = IsWhiteToMove() ? Side::White : Side::Black;
        const Bitboard pawns_stm = pieces_.GetPieceBitboard(stm, PieceType::Pawn);
        const Bitboard att_stm   = PawnMasks::kAttack[static_cast<int>(stm)][en_passant_];

        if ((pawns_stm & att_stm) != 0ULL) {
            hash_.InvertEnPassantFile(en_passant_ % 8);
        }
    }

    RemovePiece(move.GetFrom(), move.GetAttackerType(), move.GetAttackerSide());
    AddPiece(move.GetTo(),   move.GetAttackerType(), move.GetAttackerSide());

    if (move.GetDefenderType() != Move::None) {
        u.CapturedType   = move.GetDefenderType();
        u.CapturedSide   = move.GetDefenderSide();
        u.CapturedSquare = move.GetTo();
        RemovePiece(move.GetTo(), move.GetDefenderType(), move.GetDefenderSide());
    }

    switch (move.GetFlag()) {
        case Move::Flag::Capture:
        case Move::Flag::Default:
            break;

        case Move::Flag::PawnLongMove: {
            en_passant_ = static_cast<uint8_t>((move.GetFrom() + move.GetTo()) / 2); // всегда ставим EP-клетку

            // EP-бит в хэш добавляем только если у следующей стороны есть бьющая пешка
            const Side next = IsWhiteToMove() ? Side::Black : Side::White;
            const Bitboard pawns_next = pieces_.GetPieceBitboard(next, PieceType::Pawn);
            const Bitboard att_next   = PawnMasks::kAttack[static_cast<int>(next)][en_passant_];

            if ((pawns_next & att_next) != 0ULL) {
                hash_.InvertEnPassantFile(en_passant_ % 8);
            }
            break;
        }

        case Move::Flag::EnPassantCapture: {
            Side captured_side = static_cast<Side>(move.GetAttackerSide()) == Side::White ? Side::Black : Side::White;
            int captured_sq = move.GetTo() + (static_cast<Side>(move.GetAttackerSide()) == Side::White ? -8 : 8);
            u.CapturedType   = static_cast<uint8_t>(PieceType::Pawn);
            u.CapturedSide   = static_cast<uint8_t>(captured_side);
            u.CapturedSquare = static_cast<uint8_t>(captured_sq);
            RemovePiece(captured_sq, static_cast<uint8_t>(PieceType::Pawn), static_cast<uint8_t>(captured_side));
            break;
        }

        case Move::Flag::WhiteShortCastling:
            u.RookFrom = 7;
            u.RookTo = 5;
            RemovePiece(7, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
            AddPiece(5, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
            break;

        case Move::Flag::WhiteLongCastling:
            u.RookFrom = 0;
            u.RookTo = 3;
            RemovePiece(0, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
            AddPiece(3, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
            break;

        case Move::Flag::BlackShortCastling:
            u.RookFrom = 63;
            u.RookTo = 61;
            RemovePiece(63, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
            AddPiece(61, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
            break;

        case Move::Flag::BlackLongCastling:
            u.RookFrom = 56;
            u.RookTo = 59;
            RemovePiece(56, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
            AddPiece(59, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
            break;

        case Move::Flag::PromoteToBishop:
            RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
            AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Bishop), move.GetAttackerSide());
            break;

        case Move::Flag::PromoteToKnight:
            RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
            AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Knight), move.GetAttackerSide());
            break;

        case Move::Flag::PromoteToRook:
            RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
            AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Rook), move.GetAttackerSide());
            break;

        case Move::Flag::PromoteToQueen:
            RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
            AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Queen), move.GetAttackerSide());
            break;
    }

    pieces_.UpdateBitboard();

    if (move.GetFlag() != Move::Flag::PawnLongMove) {
        en_passant_ = Position::NONE; // хэш уже очищен в начале функции
    }

    switch (move.GetFrom()) {
        case 0:
            DisableCastling(Side::White, true);
            break;
        case 4:
            DisableCastling(Side::White, true);
            DisableCastling(Side::White, false);
            break;
        case 7:
            DisableCastling(Side::White, false);
            break;
        case 56:
            DisableCastling(Side::Black, true);
            break;
        case 60:
            DisableCastling(Side::Black, true);
            DisableCastling(Side::Black, false);
            break;
        case 63:
            DisableCastling(Side::Black, false);
            break;
    }

        if (move.GetDefenderType() == static_cast<uint8_t>(PieceType::Rook)) {
            switch (move.GetTo()) {
                case 0:
                    DisableCastling(Side::White, true);
                    break;   // a1 → снимаем у белых Q
                case 7:
                    DisableCastling(Side::White, false);
                    break;   // h1 → снимаем у белых K
                case 56:
                    DisableCastling(Side::Black, true);
                    break;   // a8 → снимаем у чёрных q
                case 63:
                    DisableCastling(Side::Black, false);
                    break;   // h8 → снимаем у чёрных k
                default:
                    break;
            }
        }


    UpdateMoveCounter();
    UpdateFiftyMovesCounter(move.GetAttackerType() == 0 || move.GetDefenderType() != Move::None);

    hash_.InvertMove(); // правка от 09.08.2025 (!!!! надо проверить !!!!)
}

void Position::ApplyMove(Move move) {
    Undo tmp;
    ApplyMove(move, tmp);

    if (move.GetAttackerType() == 0 || move.GetDefenderType() != Move::None) {
        repetition_history_.Clear();
    }

    repetition_history_.AddPosition(hash_);
}

void Position::UndoMove(Move move, const Undo& u) {
    if (en_passant_ != NONE) {
        const Side stm = IsWhiteToMove() ? Side::White : Side::Black;
        const Bitboard pawns_stm = pieces_.GetPieceBitboard(stm, PieceType::Pawn);
        const Bitboard att_stm   = PawnMasks::kAttack[static_cast<int>(stm)][en_passant_];
        if ((pawns_stm & att_stm) != 0ULL) {
            hash_.InvertEnPassantFile(en_passant_ % 8);
        }
        en_passant_ = NONE;
    }

    hash_.InvertMove();

    // откат спец-ходов
    switch (move.GetFlag()) {
    case Move::Flag::WhiteShortCastling:
    case Move::Flag::WhiteLongCastling:
    case Move::Flag::BlackShortCastling:
    case Move::Flag::BlackLongCastling:
        if (u.RookTo != NONE && u.RookFrom != NONE) {
            RemovePiece(u.RookTo,   static_cast<uint8_t>(PieceType::Rook),
                        (u.RookFrom < 8 ? static_cast<uint8_t>(Side::White) : static_cast<uint8_t>(Side::Black)));
            AddPiece(u.RookFrom,    static_cast<uint8_t>(PieceType::Rook),
                     (u.RookFrom < 8 ? static_cast<uint8_t>(Side::White) : static_cast<uint8_t>(Side::Black)));
        }
        break;
    case Move::Flag::PromoteToBishop:
        RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Bishop), move.GetAttackerSide());
        AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
        break;
    case Move::Flag::PromoteToKnight:
        RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Knight), move.GetAttackerSide());
        AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
        break;
    case Move::Flag::PromoteToRook:
        RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Rook), move.GetAttackerSide());
        AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
        break;
    case Move::Flag::PromoteToQueen:
        RemovePiece(move.GetTo(), static_cast<uint8_t>(PieceType::Queen), move.GetAttackerSide());
        AddPiece(move.GetTo(), static_cast<uint8_t>(PieceType::Pawn), move.GetAttackerSide());
        break;
    case Move::Flag::EnPassantCapture:
        if (u.CapturedSquare != NONE) {
            AddPiece(u.CapturedSquare, static_cast<uint8_t>(PieceType::Pawn), u.CapturedSide);
        }
        break;
    default:
        break;
    }

    if (move.GetFlag() != Move::Flag::EnPassantCapture && u.CapturedType != Move::None) {
        AddPiece(u.CapturedSquare, u.CapturedType, u.CapturedSide);
    }

    RemovePiece(move.GetTo(), move.GetAttackerType(), move.GetAttackerSide());
    AddPiece(move.GetFrom(), move.GetAttackerType(), move.GetAttackerSide());

    en_passant_ = u.EnPassantBefore;
    if (en_passant_ != NONE) {
        const Side stm_prev = IsWhiteToMove() ? Side::White : Side::Black;
        const Bitboard pawns_prev = pieces_.GetPieceBitboard(stm_prev, PieceType::Pawn);
        const Bitboard att_prev   = PawnMasks::kAttack[static_cast<int>(stm_prev)][en_passant_];
        if ((pawns_prev & att_prev) != 0ULL) {
            hash_.InvertEnPassantFile(en_passant_ % 8);
        }
    }

    // восстановить рокировки (+согласовать хэш ключи)
    if (white_long_castling_ != u.WhiteLongBefore) {
        hash_.InvertWhiteLongCastling(), white_long_castling_ = u.WhiteLongBefore;
    }

    if (white_short_castling_!= u.WhiteShortBefore) {
        hash_.InvertWhiteShortCastling(), white_short_castling_ = u.WhiteShortBefore;
    }

    if (black_long_castling_ != u.BlackLongBefore)  {
        hash_.InvertBlackLongCastling(), black_long_castling_ = u.BlackLongBefore;
    }

    if (black_short_castling_!= u.BlackShortBefore) {
        hash_.InvertBlackShortCastling(), black_short_castling_ = u.BlackShortBefore;
    }

    fifty_move_counter_ = u.FiftyBefore;
    move_counter_ = u.MoveCounterBefore;

    pieces_.UpdateBitboard();
}

void Position::ApplyNullMove(NullUndo& u) {
    u.EnPassantBefore   = en_passant_;
    u.MoveCounterBefore = move_counter_;

    if (en_passant_ != NONE) {
        hash_.InvertEnPassantFile(en_passant_ % 8);
        en_passant_ = NONE;
    }

    UpdateMoveCounter();
    hash_.InvertMove();
}

void Position::UndoNullMove(const NullUndo& u) {
    hash_.InvertMove();
    move_counter_ = u.MoveCounterBefore;
    SetEnPassantSquare(u.EnPassantBefore);
}


void Position::AddPiece(uint8_t square, uint8_t type, uint8_t side) {
    Bitboard bb = pieces_.GetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type));
    pieces_.SetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type), BOp::Set_1(bb, square));
    //pieces_.UpdateBitboard();
    hash_.InvertPiece(square, type, side);
}

void Position::RemovePiece(uint8_t square, uint8_t type, uint8_t side) {
    Bitboard bb = pieces_.GetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type));
    if (BOp::GetBit(bb, square)) {
        bb = BOp::Set_0(bb, square);
        pieces_.SetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type), bb);
        //pieces_.UpdateBitboard();
        hash_.InvertPiece(square, type, side);
    }
}

void Position::SetEnPassantSquare(uint8_t square) {
    if (en_passant_ != NONE) {
        hash_.InvertEnPassantFile(en_passant_ % 8);
    }
    en_passant_ = square;

    if (en_passant_ != NONE) {
        hash_.InvertEnPassantFile(en_passant_ % 8);
    }
}

void Position::DisableCastling(Side side, bool long_castle) {
    if (side == Side::White) {
        if (long_castle && white_long_castling_) {
            white_long_castling_ = false;
            hash_.InvertWhiteLongCastling();
        }
        else if (!long_castle && white_short_castling_) {
            white_short_castling_ = false;
            hash_.InvertWhiteShortCastling();
        }
    } else {
        if (long_castle && black_long_castling_) {
            black_long_castling_ = false;
            hash_.InvertBlackLongCastling();
        }
        else if (!long_castle && black_short_castling_) {
            black_short_castling_ = false;
            hash_.InvertBlackShortCastling();
        }
    }
}

void Position::UpdateMoveCounter() {
    ++move_counter_;
}

void Position::UpdateFiftyMovesCounter(bool breaking_event) {
    fifty_move_counter_ = breaking_event ? 0 : fifty_move_counter_ + 1;
}

bool Position::IsFiftyMoveRuleDraw() const {
    return fifty_move_counter_ >= 100;
}

bool Position::IsThreefoldRepetition() const {
    return repetition_history_.GetRepetitionNumber(hash_) >= 3;
}

bool Position::IsWhiteToMove() const {
    return move_counter_ % 2 == 0;
}

const Pieces& Position::GetPieces() const {
    return pieces_;
}

const ZobristHash& Position::GetHash() const {
    return hash_;
}

uint8_t Position::GetEnPassantSquare() const {
    return en_passant_;
}

bool Position::GetWhiteLongCastling() const {
    return white_long_castling_;
}

bool Position::GetWhiteShortCastling() const {
    return white_short_castling_;
}

bool Position::GetBlackLongCastling() const {
    return black_long_castling_;
}

bool Position::GetBlackShortCastling() const {
    return black_short_castling_;
}

std::ostream& operator<<(std::ostream& os, const Position& position) {
    os << position.pieces_;
    return os;
}

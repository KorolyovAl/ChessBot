#include "position.h"

#include <iostream>
#include <cmath>

/**********************************************
* Constructs position from short FEN and flags.
**********************************************/
Position::Position(const std::string& short_fen, uint8_t en_passant,
                   bool white_long, bool white_short,
                   bool black_long, bool black_short,
                   float move_counter)
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

void Position::ApplyMove(Move move) {
    // Check if the attacking piece actually exists on the source square
    if (!BOp::GetBit(pieces_.GetPieceBitboard(  static_cast<Side>(move.GetAttackerSide()),
                                                static_cast<PieceType>(move.GetAttackerType())),
                                                move.GetFrom())) {
        return;
    }

    RemovePiece(move.GetFrom(), move.GetAttackerType(), move.GetAttackerSide());
    AddPiece(move.GetTo(), move.GetAttackerType(), move.GetAttackerSide());

    if (move.GetDefenderType() != Move::None) {
        RemovePiece(move.GetTo(), move.GetDefenderType(), move.GetDefenderSide());
    }

    //handling special moves
    switch (static_cast<Move::Flag>(move.GetFlag())) {
    case Move::Flag::Capture:
    case Move::Flag::Default:
        break;

    case Move::Flag::PawnLongMove:
        SetEnPassantSquare((move.GetFrom() + move.GetTo()) / 2);
        break;

    case Move::Flag::EnPassantCapture: {
        Side captured_side = static_cast<Side>(move.GetAttackerSide()) == Side::White ? Side::Black : Side::White;
        int captured_pawn_square = move.GetTo() + (static_cast<Side>(move.GetAttackerSide()) == Side::White ? -8 : 8);

        RemovePiece(captured_pawn_square, 0, static_cast<uint8_t>(captured_side));
        break;
    }

    case Move::Flag::WhiteShortCastling:
        RemovePiece(7, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
        AddPiece(5, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
        break;

    case Move::Flag::WhiteLongCastling:
        RemovePiece(0, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
        AddPiece(3, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::White));
        break;

    case Move::Flag::BlackShortCastling:
        RemovePiece(63, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
        AddPiece(61, static_cast<uint8_t>(PieceType::Rook), static_cast<uint8_t>(Side::Black));
        break;

    case Move::Flag::BlackLongCastling:
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
        SetEnPassantSquare(Position::NONE);
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

    UpdateMoveCounter();
    UpdateFiftyMovesCounter(move.GetAttackerType() == 0 || move.GetDefenderType() != Move::None);

    if (move.GetAttackerType() == 0 || move.GetDefenderType() != Move::None) {
        repetition_history_.Clear();
    }

    hash_.InvertMove(); // правка от 09.08.2025 (!!!! надо проверить !!!!)
    repetition_history_.AddPosition(hash_);
}

void Position::AddPiece(uint8_t square, uint8_t type, uint8_t side) {
    Bitboard bb = pieces_.GetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type));
    pieces_.SetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type), BOp::Set_1(bb, square));
    pieces_.UpdateBitboard();
    hash_.InvertPiece(square, type, side);
}

void Position::RemovePiece(uint8_t square, uint8_t type, uint8_t side) {
    Bitboard bb = pieces_.GetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type));
    if (BOp::GetBit(bb, square)) {
        bb = BOp::Set_0(bb, square);
        pieces_.SetPieceBitboard(static_cast<Side>(side), static_cast<PieceType>(type), bb);
        pieces_.UpdateBitboard();
        hash_.InvertPiece(square, type, side);
    }
}

void Position::SetEnPassantSquare(uint8_t square) {
    en_passant_ = square;
}

void Position::DisableCastling(Side side, bool long_castle) {
    if (side == Side::White) {
        if (long_castle && white_long_castling_) {
            white_long_castling_ = false;
            hash_.InvertWhiteLongCastling();
        } else if (!long_castle && white_short_castling_) {
            white_short_castling_ = false;
            hash_.InvertWhiteShortCastling();
        }
    } else {
        if (long_castle && black_long_castling_) {
            black_long_castling_ = false;
            hash_.InvertBlackLongCastling();
        } else if (!long_castle && black_short_castling_) {
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

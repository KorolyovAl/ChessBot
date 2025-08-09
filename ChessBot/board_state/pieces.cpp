#include <iostream>
#include "pieces.h"

/**********************************************
* Constructs bitboards from a FEN-style string.
* Supports short format like: "rnbqkbnr/pppppppp/8/...".
* Each character updates position accordingly.
**********************************************/
Pieces::Pieces(const std::string& short_fen) {
    uint8_t x = 0;
    uint8_t y = 7;
    Side side;

    for (char ch : short_fen) {
        if (ch == '/') {
            x = 0;
            y--;
        } else if (std::isdigit(ch)) {
            x += ch - '0'; // Skip empty squares
        } else {
            if (std::isupper(ch)) {
                ch = std::tolower(ch);
                side = Side::White;
            } else {
                side = Side::Black;
            }

            const uint8_t index = y * 8 + x;

            PieceType piece;
            switch (ch) {
            case 'p': piece = PieceType::Pawn; break;
            case 'n': piece = PieceType::Knight; break;
            case 'b': piece = PieceType::Bishop; break;
            case 'r': piece = PieceType::Rook; break;
            case 'q': piece = PieceType::Queen; break;
            case 'k': piece = PieceType::King; break;
            default: continue;
            }

            piece_bitboards_[static_cast<int>(side)][static_cast<int>(piece)] =
                BOp::Set_1(piece_bitboards_[static_cast<int>(side)][static_cast<int>(piece)], index);

            x++;
        }
    }

    UpdateBitboard();
}

bool operator==(const Pieces& left, const Pieces& right) {
    return left.piece_bitboards_ == right.piece_bitboards_;
}

/**********************************************
* Returns character representing piece on square.
* Uppercase = white, lowercase = black.
* Returns '.' if square is empty.
**********************************************/
char GetPieceChar(const Pieces& pieces, uint8_t index) {
    static const char labels[6] = { 'p', 'n', 'b', 'r', 'q', 'k' };

    for (int s = 0; s < 2; ++s) {
        Side side = static_cast<Side>(s);
        for (int p = 0; p < static_cast<int>(PieceType::Count); ++p) {
            if (BOp::GetBit(pieces.GetPieceBitboard(side, static_cast<PieceType>(p)), index)) {
                char ch = labels[p];
                return (side == Side::White) ? std::toupper(ch) : ch;
            }
        }
    }

    return '.';
}

/*******************************
* Outputs the board to console
* using ASCII representation.
*******************************/
std::ostream& operator<<(std::ostream& os, const Pieces& pieces) {
    os << "     a    b    c    d    e    f    g    h\n";
    os << "   -----------------------------------------\n";

    for (int8_t y = 7; y >= 0; y--) {
        os << " " << (int)(y + 1) << " ";
        for (uint8_t x = 0; x < 8; x++) {
            uint8_t index = y * 8 + x;
            os << "| " << GetPieceChar(pieces, index) << " ";
        }
        os << "|\n";
        os << "   -----------------------------------------\n";
    }

    os << "     a    b    c    d    e    f    g    h\n";
    return os;
}

/**********************************************
* Rebuilds aggregated bitboards:
* - side_bitboards_: all pieces of each side
* - inv_side_bitboards_: inverse masks
* - all_: all occupied squares
* - empty_: empty squares
**********************************************/
void Pieces::UpdateBitboard() {
    for (int s = 0; s < 2; ++s) {
        Bitboard combined = 0;
        for (int p = 0; p < static_cast<int>(PieceType::Count); ++p) {
            combined |= piece_bitboards_[s][p];
        }
        side_bitboards_[s] = combined;
        inv_side_bitboards_[s] = ~combined;
    }

    all_ = side_bitboards_[0] | side_bitboards_[1];
    empty_ = ~all_;
}

void Pieces::SetPieceBitboard(Side side, PieceType piece, Bitboard bb) {
    piece_bitboards_[static_cast<int>(side)][static_cast<int>(piece)] = bb;
}

Bitboard Pieces::GetPieceBitboard(Side side, PieceType piece) const {
    return piece_bitboards_[static_cast<int>(side)][static_cast<int>(piece)];
}

Bitboard Pieces::GetSideBoard(Side side) const {
    return side_bitboards_[static_cast<int>(side)];
}

Bitboard Pieces::GetInvSideBitboard(Side side) const {
    return inv_side_bitboards_[static_cast<int>(side)];
}

Bitboard Pieces::GetAllBitboard() const {
    return all_;
}

Bitboard Pieces::GetEmptyBitboard() const {
    return empty_;
}

std::array<std::array<Bitboard, static_cast<int>(PieceType::Count)>, 2> Pieces::GetPieceBitboards() const {
    return piece_bitboards_;
}

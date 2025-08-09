#include "zobrist_hash.h"
#include "bitboard.h"
#include <random>
#include <mutex>

// Static members
std::array<std::array<std::array<uint64_t, static_cast<int>(PieceType::Count)>, 2>, 64> ZobristHash::piece_keys_{};
uint64_t ZobristHash::black_to_move_key_ = 0;
uint64_t ZobristHash::white_long_castling_key_ = 0;
uint64_t ZobristHash::white_short_castling_key_ = 0;
uint64_t ZobristHash::black_long_castling_key_ = 0;
uint64_t ZobristHash::black_short_castling_key_ = 0;

std::once_flag ZobristHash::init_flag_;

void ZobristHash::InitConstants() {
    std::call_once(init_flag_, [] {
        std::mt19937_64 rng(1337); // fixed seed for reproducibility
        std::uniform_int_distribution<uint64_t> dist;

        for (uint8_t sq = 0; sq < 64; ++sq) {
            for (Side side : {Side::White, Side::Black}) {
                for (PieceType type : {
                                       PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                       PieceType::Rook, PieceType::Queen, PieceType::King }) {

                    piece_keys_[sq][static_cast<int>(side)][static_cast<int>(type)] = dist(rng);
                }
            }
        }

        black_to_move_key_         = dist(rng);
        white_long_castling_key_   = dist(rng);
        white_short_castling_key_  = dist(rng);
        black_long_castling_key_   = dist(rng);
        black_short_castling_key_  = dist(rng);
    });
}

ZobristHash::ZobristHash(Pieces pieces, bool black_to_move,
                         bool white_long, bool white_short,
                         bool black_long, bool black_short) {
    InitConstants();

    for (uint8_t sq = 0; sq < 64; ++sq) {
        for (Side side : {Side::White, Side::Black}) {
            for (PieceType type : {
                                   PieceType::Pawn, PieceType::Knight, PieceType::Bishop,
                                   PieceType::Rook, PieceType::Queen, PieceType::King }) {

                if (BOp::GetBit(pieces.GetPieceBitboard(side, type), sq)) {
                    value_ ^= piece_keys_[sq][static_cast<int>(side)][static_cast<int>(type)];
                }
            }
        }
    }

    if (black_to_move) value_ ^= black_to_move_key_;
    if (white_long)    value_ ^= white_long_castling_key_;
    if (white_short)   value_ ^= white_short_castling_key_;
    if (black_long)    value_ ^= black_long_castling_key_;
    if (black_short)   value_ ^= black_short_castling_key_;
}

void ZobristHash::InvertPiece(uint8_t square, uint8_t type, uint8_t side) {
    value_ ^= piece_keys_[square][side][type];
}

void ZobristHash::InvertMove() {
    value_ ^= black_to_move_key_;
}

void ZobristHash::InvertWhiteLongCastling() {
    value_ ^= white_long_castling_key_;
}

void ZobristHash::InvertWhiteShortCastling() {
    value_ ^= white_short_castling_key_;
}

void ZobristHash::InvertBlackLongCastling() {
    value_ ^= black_long_castling_key_;
}

void ZobristHash::InvertBlackShortCastling() {
    value_ ^= black_short_castling_key_;
}

uint64_t ZobristHash::GetValue() const {
    return value_;
}

bool operator==(ZobristHash left, ZobristHash right) {
    return left.value_ == right.value_;
}

bool operator<(ZobristHash left, ZobristHash right) {
    return left.value_ < right.value_;
}

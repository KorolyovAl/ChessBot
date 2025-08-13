#include "search.h"
#include "evaluation.h"
#include "move_ordering.h"
#include "../move_generation/legal_move_gen.h"
#include "../move_generation/ps_legal_move_mask_gen.h"

TranspositionTable Search::tt_{64};

void Search::SetHashMB(int megabytes) {
    tt_ = TranspositionTable(megabytes);
}

void Search::ClearTT() {
    tt_.Clear();
}

int Search::MateIn(int ply) {
    return kMateScore - ply;
}

int Search::MatedIn(int ply) {
    return -kMateScore + ply;
}

Move Search::FindBestMove(Position& position, const Limits& limits) {
    Move best_move;
    int alpha = -kInfinity;
    int beta = kInfinity;

    for (int current_depth = 1; current_depth <= limits.Depth; ++current_depth) {
        int best_score = -kInfinity;

        MoveList move_list;
        const Side side_to_move = position.IsWhiteToMove() ? Side::White : Side::Black;
        LegalMoveGen::Generate(position, side_to_move, move_list);

        // Simple ordering: promotions/captures first using MVV-LVA from the board.
        for (uint8_t i = 0; i < move_list.GetSize(); ++i) {
            for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
                const int left_score = MoveOrdering::Score(move_list[j], position.GetPieces());
                const int right_score = MoveOrdering::Score(move_list[j + 1], position.GetPieces());
                if (left_score < right_score) {
                    std::swap(move_list[j], move_list[j + 1]);
                } else {
                    break;
                }
            }
        }

        for (uint8_t i = 0; i < move_list.GetSize(); ++i) {
            const Move move = move_list[i];

            Position::Undo undo;
            position.ApplyMove(move, undo);

            const int score = -AlphaBeta(position, current_depth - 1, -beta, -alpha, /*ply=*/1);

            position.UndoMove(move, undo);

            if (score > best_score) {
                best_score = score;
                best_move = move;
            }
            if (score > alpha) {
                alpha = score;
            }
        }

        // Store best at root
        tt_.Store(position.GetZobristKey(),
                  current_depth,
                  best_score,
                  TranspositionTable::Bound::Exact,
                  best_move);
    }

    return best_move;
}

int Search::AlphaBeta(Position& position, int depth, int alpha, int beta, int ply) {
    if (IsTerminalDraw(position)) {
        return 0;
    }

    int tt_score = 0;
    Move tt_move;
    const bool have_tt = tt_.Probe(position.GetZobristKey(), depth, alpha, beta, tt_score, tt_move);

    if (have_tt) {
        return tt_score;
    }

    if (depth <= 0) {
        return Quiescence(position, alpha, beta, ply);
    }

    MoveList move_list;
    const Side side_to_move = position.IsWhiteToMove() ? Side::White : Side::Black;
    LegalMoveGen::Generate(position, side_to_move, move_list);

    if (move_list.GetSize() == 0) {
        // Checkmate or stalemate
        const Pieces& pieces = position.GetPieces();
        const Bitboard king_bb = pieces.GetPieceBitboard(side_to_move, PieceType::King);

        uint8_t king_square = 0;
        if (king_bb) {
            king_square = BOp::BitScanForward(king_bb);
        }

        const bool in_check = PsLegalMaskGen::SquareInDanger(pieces, king_square, side_to_move);
        return in_check ? MatedIn(ply) : 0;
    }

    // Ordering: basic MVV-LVA using the board snapshot
    for (uint8_t i = 0; i < move_list.GetSize(); ++i) {
        for (int j = static_cast<int>(i) - 1; j >= 0; --j) {
            const int left_score = MoveOrdering::Score(move_list[j], position.GetPieces());
            const int right_score = MoveOrdering::Score(move_list[j + 1], position.GetPieces());
            if (left_score < right_score) {
                std::swap(move_list[j], move_list[j + 1]);
            } else {
                break;
            }
        }
    }

    Move best_move;
    int best_score = -kInfinity;

    for (uint8_t i = 0; i < move_list.GetSize(); ++i) {
        const Move move = move_list[i];

        Position::Undo undo;
        position.ApplyMove(move, undo);

        const int score = -AlphaBeta(position, depth - 1, -beta, -alpha, ply + 1);

        position.UndoMove(move, undo);

        if (score > best_score) {
            best_score = score;
            best_move = move;
        }
        if (score > alpha) {
            alpha = score;
        }
        if (alpha >= beta) {
            break; // beta cut-off
        }
    }

    TranspositionTable::Bound bound = TranspositionTable::Bound::Exact;
    if (best_score <= alpha) {
        bound = TranspositionTable::Bound::Upper;
    } else if (best_score >= beta) {
        bound = TranspositionTable::Bound::Lower;
    }

    tt_.Store(position.GetZobristKey(), depth, best_score, bound, best_move);

    return best_score;
}

int Search::Quiescence(Position& position, int alpha, int beta, int ply) {
    const int stand_pat = Evaluation::Evaluate(position);

    if (stand_pat >= beta) {
        return beta;
    }
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }

    MoveList capture_list;
    const Side side_to_move = position.IsWhiteToMove() ? Side::White : Side::Black;
    LegalMoveGen::Generate(position, side_to_move, capture_list, /*only_captures=*/true);

    for (uint8_t i = 0; i < capture_list.GetSize(); ++i) {
        const Move move = capture_list[i];

        Position::Undo undo;
        position.ApplyMove(move, undo);

        const int score = -Quiescence(position, -beta, -alpha, ply + 1);

        position.UndoMove(move, undo);

        if (score >= beta) {
            return beta;
        }
        if (score > alpha) {
            alpha = score;
        }
    }

    return alpha;
}

bool Search::IsTerminalDraw(const Position& position) {
    if (position.IsFiftyMoveRuleDraw()) {
        return true;
    }
    if (position.IsThreefoldRepetition()) {
        return true;
    }
    return false;
}

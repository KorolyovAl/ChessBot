#include <vector>
#include <algorithm>
#include <iostream>

#include "search.h"
#include "../board_state/bitboard.h"
#include "../board_state/pieces.h"
#include "../move_generation/move_list.h"
#include "../move_generation/legal_move_gen.h"
#include "evaluation.h"
#include "move_ordering.h"
#include "../move_generation/ps_legal_move_mask_gen.h"
#include "static_exchange_evaluation.h"
#include "piece_values.h"

namespace {
static constexpr bool kUseTT = true;

constexpr int kInfinity = 32000;
constexpr int kMateScore = 31000;
constexpr int kMateThreshold = kMateScore - 1024;

inline int Clamp(int x, int lo, int hi) {
    if (x < lo) {
        return lo;
    }

    if (x > hi) {
        return hi;
    }
    return x;
}
} // namespace

SearchEngine::SearchEngine(TranspositionTable& tt) : tt_(tt) {
}

void SearchEngine::SetStopCallback(bool (*is_stopped)()) noexcept {
    is_stopped_ = is_stopped;
}

bool SearchEngine::IsMateScore(int score) noexcept {
    if (score > kMateThreshold) {
        return true;
    }

    if (score < -kMateThreshold) {
        return true;
    }
    return false;
}

int SearchEngine::ScoreToTT(int score, int halfmove) noexcept {
    if (!IsMateScore(score)) {
        return score;
    }

    if (score > 0) {
        return score + halfmove;
    }
    else {
        return score - halfmove;
    }
}

int SearchEngine::ScoreFromTT(int score, int halfmove) noexcept {
    if (!IsMateScore(score)) {
        return score;
    }
    if (score > 0) {
        return score - halfmove;
    }
    else {
        return score + halfmove;
    }
}

bool SearchEngine::IsTimeUp() const noexcept {
    if (is_stopped_ && is_stopped_()) {
        return true;
    }
    return false;
}

void SearchEngine::ResetCutoffKeys() noexcept {
    for (auto& row : cutoff_keys_) {
        row[0] = 0;
        row[1] = 0;
    }
}

SearchResult SearchEngine::Search(Position& root, const SearchLimits& limits) {
    // Reset search state
    nodes_ = 0;
    limits_ = limits;
    ResetCutoffKeys();

    SearchResult result{};

    // Aspiration window seeds
    int alpha = -kInfinity;
    int beta  = +kInfinity;
    int prev_score = 0;

    const int max_depth = limits.max_depth;

    // Iterative deepening loop
    for (int depth = 1; depth <= max_depth; ++depth) {
        // Aspiration window around previous score (tighter as depth grows)
        int window = (depth <= 4) ? 25 : 15;
        alpha = Clamp(prev_score - window, -kInfinity, kInfinity);
        beta  = Clamp(prev_score + window, -kInfinity, kInfinity);

        // Main alpha-beta search for the current depth
        PvLine pv{};
        int score = AlphaBeta(root, depth, alpha, beta, /*halfmove=*/0, pv);
        if (IsTimeUp()) {
            break;
        }

        // Aspiration fail → re-search with full window
        if (score <= alpha || score >= beta) {
            alpha = -kInfinity;
            beta  = +kInfinity;
            pv.length = 0;
            score = AlphaBeta(root, depth, alpha, beta, 0, pv);

            if (IsTimeUp()) {
                break;
            }
        }

        // Iteration result
        prev_score = score;
        result.depth = depth;
        result.score_cp = score;

        if (pv.length > 0) {
            result.best_move = pv.moves[0];
        }

        result.pv = pv;
        result.nodes = nodes_;

        // Early stops: mate found or node limit reached
        if (IsMateScore(score)) {
            break;
        }

        // Reached node limit — exit iterative deepening loop
        if (limits_.nodes_limit > 0 && nodes_ >= limits_.nodes_limit) {
            break;
        }

        int from = static_cast<int>(result.best_move.GetFrom());
        int to = static_cast<int>(result.best_move.GetTo());

        std::cout << "search depth: " << depth << "; score cp: " << result.score_cp << "; best move: "
                  << from << "-" << to << std::endl;
    }

    return result;
}

int SearchEngine::Quiescence(Position& pos, int alpha, int beta, int halfmove, PvLine& pv) {
    if (!IncreaseNodeCounter()) {
        return 0;
    }

    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    const uint8_t ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(stm, PieceType::King));
    const bool in_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), ksq, stm);

    int stand_pat = 0;
    if (!in_check) {
        stand_pat = Evaluation::Evaluate(pos);
        if (!pos.IsWhiteToMove()) {
            stand_pat *= -1;
        }

        if (stand_pat >= beta) {
            return stand_pat;
        }

        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/!in_check);

    const size_t n = ml.GetSize();

    MoveOrdering::Context qctx{};
    qctx.tt_move      = Move{};
    qctx.cutoff1      = 0;
    qctx.cutoff2      = 0;
    qctx.history      = &history_;
    qctx.side_to_move = stm;

    std::vector<int> scores(n);
    std::vector<size_t> order(n);

    for (size_t i = 0; i < n; ++i) {
        order[i]  = i;
        scores[i] = MoveOrdering::Score(ml[i], pos.GetPieces(), qctx);
    }

    std::sort(order.begin(), order.end(), [&](size_t ia, size_t ib) {
        return scores[ia] > scores[ib];
    });

    PvLine best_child{};
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const Move& m = ml[order[oi]];

        // Out of check: delta and SEE filter for captures
        if (!in_check) {
            const bool is_ep  = (m.GetFlag() == Move::Flag::EnPassantCapture);
            const bool is_cap = is_ep || (m.GetDefenderType() != Move::None);
            const bool is_promo =
                m.GetFlag() == Move::Flag::PromoteToQueen  ||
                m.GetFlag() == Move::Flag::PromoteToRook   ||
                m.GetFlag() == Move::Flag::PromoteToBishop ||
                m.GetFlag() == Move::Flag::PromoteToKnight;

            if (is_cap) {
                // Cheap delta check: if even the optimistic gain cannot reach alpha, skip
                const int victim = is_ep ? EvalValues::kPieceValueCp[PieceType::Pawn]
                                         : EvalValues::kPieceValueCp[static_cast<int>(m.GetDefenderType())];
                const int DELTA = 90; // slightly conservative

                if (stand_pat + victim + DELTA < alpha) {
                    continue;
                }

                // Static exchange evaluation: discard losing captures
                if (!is_promo) {
                    const int see = StaticExchangeEvaluation::Capture(pos.GetPieces(), m);
                    if (see < 0) {
                        continue;
                    }
                }
            }
            // In quiet quiescence node without check we ignore quiet moves
            else {
                continue;
            }
        }

        Position::Undo u{};
        pos.ApplyMove(m, u);

        PvLine child{};
        const int score = -Quiescence(pos, -beta, -alpha, halfmove + 1, child);

        pos.UndoMove(m, u);

        if (score >= beta) {
            return score;
        }

        if (score > alpha) {
            alpha = score;
            best_child = child;
            pv.length = 0;

            if (pv.length < 128) {
                pv.moves[pv.length++] = m;
            }

            for (int i = 0; i < best_child.length && pv.length < 128; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    return alpha;
}

int SearchEngine::AlphaBeta(Position& pos, int depth, int alpha, int beta, int halfmove, PvLine& pv) {
    // Node or time limit check
    if (!IncreaseNodeCounter()) {
        return 0;
    }

    // Fast draw by repetition or fifty-move rule
    if (pos.IsThreefoldRepetition() || pos.IsFiftyMoveRuleDraw()) {
        return 0;
    }

    // Leaf node goes to quiescence search
    if (depth <= 0) {
        return Quiescence(pos, alpha, beta, halfmove, pv);
    }

    // Save original alpha for correct bound type when writing to TT
    const int alpha_orig = alpha;

    // Transposition table probe
    const uint64_t key = pos.GetZobristKey();
    int  tt_score = 0;
    Move tt_move{};
    if (kUseTT == true) {
        if (tt_.Probe(key, depth, alpha, beta, tt_score, tt_move)) {
            return ScoreFromTT(tt_score, halfmove);
        }
    }

    // Static evaluation of the node (for futility and razoring)
    // Evaluate returns score from the side of White
    const int static_eval = pos.IsWhiteToMove() ? Evaluation::Evaluate(pos) : -Evaluation::Evaluate(pos);

    // Razoring at depth 1
    if (depth == 1 && static_eval + 150 <= alpha) {
        PvLine qpv{};
        const int q = Quiescence(pos, alpha - 1, alpha, halfmove, qpv);
        if (q <= alpha) {
            return q;
        }
    }

    // Null-move pruning (only if not in check and there are non-pawn pieces)
    {
        const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
        const uint8_t ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(stm, PieceType::King));
        const bool in_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), ksq, stm);

        if (!in_check && depth >= 3) {
            Bitboard non_pawn =
                pos.GetPieces().GetPieceBitboard(stm, PieceType::Knight) |
                pos.GetPieces().GetPieceBitboard(stm, PieceType::Bishop) |
                pos.GetPieces().GetPieceBitboard(stm, PieceType::Rook)   |
                pos.GetPieces().GetPieceBitboard(stm, PieceType::Queen);

            if (non_pawn) {
                Position::NullUndo nu{};
                pos.ApplyNullMove(nu);

                PvLine dummy{};
                const int R = 2;
                const int nm_score = -AlphaBeta(pos, depth - 1 - R, -beta, -beta + 1, halfmove + 1, dummy);

                pos.UndoNullMove(nu);

                if (nm_score >= beta) {
                    return nm_score;
                }
            }
        }
    }

    // Full move generation
    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/false);

    // Move ordering context (no move copying)
    const size_t n = ml.GetSize();

    MoveOrdering::Context ctx{};
    ctx.tt_move      = tt_move;
    ctx.cutoff1      = cutoff_keys_[halfmove][0];
    ctx.cutoff2      = cutoff_keys_[halfmove][1];
    ctx.history      = &history_;
    ctx.side_to_move = stm;

    std::vector<int> scores(n);
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) {
        order[i]  = i;
        scores[i] = MoveOrdering::Score(ml[i], pos.GetPieces(), ctx);
    }
    std::sort(order.begin(), order.end(), [&](size_t ia, size_t ib) {
        return scores[ia] > scores[ib];
    });

    // Main loop with LMR, PVS and pruning
    Move   best_move{};
    PvLine best_child{};
    int    best_score = -kInfinity;

    int move_index = 0;
    for (size_t oi = 0; oi < order.size(); ++oi) {
        ++move_index;
        const Move& m = ml[order[oi]];

        const bool is_promo   = IsPromotionFlag(m.GetFlag());
        const bool is_capture = (m.GetDefenderType() != Move::None) ||
                                (m.GetFlag() == Move::Flag::EnPassantCapture);
        const bool is_simple  = !is_capture && !is_promo;

        // Check if this is the TT move
        const bool is_tt = (m.GetFrom() == tt_move.GetFrom() &&
                            m.GetTo()   == tt_move.GetTo()   &&
                            m.GetFlag() == tt_move.GetFlag());
        const bool is_first = (move_index == 1);

        // Pre-SEE for captures at shallow depths
        int see = 0;
        if (is_capture && !is_promo && depth <= 2) {
            see = StaticExchangeEvaluation::Capture(pos.GetPieces(), m);
        }

        // Apply move
        Position::Undo u{};
        pos.ApplyMove(m, u);

        // Check whether the move gives check (to avoid pruning such moves)
        const Side child_stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
        const uint8_t child_ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(child_stm, PieceType::King));
        const bool gives_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), child_ksq, child_stm);

        // Evaluate SEE on checking move to see if we are hanging after the check
        const Side us = Pieces::Inverse(child_stm);
        const int see_on_checker = StaticExchangeEvaluation::On(pos.GetPieces(), m.GetTo(), us);

        // Allow pruning relaxations only for checks that are materially safe
        const bool safe_check = gives_check && (see_on_checker >= 0);

        // Futility pruning of quiet moves at shallow depths (if move is not a safe check)
        if (!safe_check && is_simple && depth <= 3 && !is_tt && !is_first) {
            const int margin = (depth == 1 ? 100 : depth == 2 ? 200 : 300);
            if (static_eval + margin <= alpha) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // SEE-based pruning of obviously losing captures at shallow depths (if move is not check)
        if (!gives_check && is_capture && !is_promo && depth <= 2 && !is_tt && !is_first) {
            if (see < 0) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // Late move pruning: very late quiet moves that are not checks and not TT moves
        if (!safe_check && is_simple && !is_tt && depth > 7 && move_index >= lmr_base_index_ + 2) {
            const int quiet_limit = 2 + (depth * depth) / 2;
            if (move_index > quiet_limit) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        const int new_depth = depth - 1;

        int score = 0;
        PvLine child{};

        // LMR for late quiet moves
        if (is_simple && depth >= 3 && move_index >= lmr_base_index_) {
            const int r = 1;
            score = -AlphaBeta(pos, new_depth - r, -alpha - 1, -alpha, halfmove + 1, child);
            if (score > alpha) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
            }
        }
        else {
            // PVS: first move searched with full window, others with zero window and optional re-search
            if (is_first) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
            }
            else {
                score = -AlphaBeta(pos, new_depth, -alpha - 1, -alpha, halfmove + 1, child);
                if (score > alpha && score < beta) {
                    score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
                }
            }
        }

        pos.UndoMove(m, u);

        // Update best score and best move
        if (score > best_score) {
            best_score  = score;
            best_child  = child;
            best_move   = m;
        }

        // Beta cutoff: update history and cutoff moves, store in TT and return
        if (best_score >= beta) {
            if (is_simple) {
                const uint16_t key16 = FromToKey(m);
                if (cutoff_keys_[halfmove][0] != key16) {
                    cutoff_keys_[halfmove][1] = cutoff_keys_[halfmove][0];
                    cutoff_keys_[halfmove][0] = key16;
                }

                // After undo, side to move is restored to the original one
                const int side_index = pos.IsWhiteToMove() ? 1 : 0;
                const int from = static_cast<int>(m.GetFrom());
                const int to   = static_cast<int>(m.GetTo());
                history_[side_index][from][to] += depth * depth;

                if (history_[side_index][from][to] > 32767) {
                    for (int s = 0; s < 2; ++s) {
                        for (int f = 0; f < 64; ++f) {
                            for (int t = 0; t < 64; ++t) {
                                history_[s][f][t] /= 2;
                            }
                        }
                    }
                }
            }

            if (kUseTT == true) {
                tt_.Store(key, depth, ScoreToTT(best_score, halfmove), TranspositionTable::Bound::Lower, best_move);
            }
            return best_score;
        }

        // Alpha improvement: update principal variation
        if (best_score > alpha) {
            alpha = best_score;
            pv.length = 0;
            pv.moves[pv.length++] = best_move;
            for (int i = 0; i < best_child.length; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    // Store node result in TT (bound type is chosen using original alpha)
    const auto bnd = (best_score <= alpha_orig)
                         ? TranspositionTable::Bound::Upper
                         : TranspositionTable::Bound::Exact;
    tt_.Store(key, depth, ScoreToTT(best_score, halfmove), bnd, best_move);
    return best_score;
}

#include <vector>
#include <algorithm>
#include <iostream>
#include <sstream>

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

int Clamp(int x, int lo, int hi) {
    if (x < lo) {
        return lo;
    }

    if (x > hi) {
        return hi;
    }
    return x;
}

const char* MoveFlagSuffix(Move::Flag flag) {
    switch (flag) {
    case Move::Flag::WhiteShortCastling:
    case Move::Flag::BlackShortCastling:
        return "[O-O]";
    case Move::Flag::WhiteLongCastling:
    case Move::Flag::BlackLongCastling:
        return "[O-O-O]";
    case Move::Flag::EnPassantCapture:
        return "[ep]";
    case Move::Flag::PromoteToKnight:
        return "[=N]";
    case Move::Flag::PromoteToBishop:
        return "[=B]";
    case Move::Flag::PromoteToRook:
        return "[=R]";
    case Move::Flag::PromoteToQueen:
        return "[=Q]";
    default:
        return "";
    }
}

std::string FormatMoveForLog(const Move& move) {
    if (move.GetFrom() == Move::None || move.GetTo() == Move::None) {
        return "(none)";
    }

    std::ostringstream out;
    out << static_cast<int>(move.GetFrom())
        << "-"
        << static_cast<int>(move.GetTo())
        << MoveFlagSuffix(move.GetFlag());
    return out.str();
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

int SearchEngine::ScoreToTT(int score, int ply) noexcept {
    if (!IsMateScore(score)) {
        return score;
    }

    if (score > 0) {
        return score + ply;
    }
    else {
        return score - ply;
    }
}

int SearchEngine::ScoreFromTT(int score, int ply) noexcept {
    if (!IsMateScore(score)) {
        return score;
    }
    if (score > 0) {
        return score - ply;
    }
    else {
        return score + ply;
    }
}

bool SearchEngine::IsTimeUp() const noexcept {
    if (search_aborted_) {
        return true;
    }

    if (is_stopped_ && is_stopped_()) {
        return true;
    }
    return false;
}

bool SearchEngine::CheckStopCondition() noexcept {
    if (search_aborted_) {
        return true;
    }

    if (is_stopped_ && is_stopped_()) {
        search_aborted_ = true;
        return true;
    }

    if (has_time_limit_ && SearchClock::now() >= deadline_) {
        search_aborted_ = true;
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

    search_aborted_ = false;
    has_time_limit_ = limits.max_time_ms > 0;
    if (has_time_limit_) {
        deadline_ = SearchClock::now() + std::chrono::milliseconds(limits.max_time_ms);
    }

    ResetCutoffKeys();

    SearchResult result{};

    // Aspiration window seeds
    int alpha = -kInfinity;
    int beta  = +kInfinity;
    int prev_score = 0;

    const int max_depth = limits.max_depth;

    // Iterative deepening loop
    for (int root_depth = 1; root_depth <= max_depth; ++root_depth) {
        if (CheckStopCondition()) {
            break;
        }

        // Aspiration window around previous score (tighter as depth grows)
        int window = (root_depth <= 4) ? 25 : 15;
        alpha = Clamp(prev_score - window, -kInfinity, kInfinity);
        beta  = Clamp(prev_score + window, -kInfinity, kInfinity);

        // Main alpha-beta search for the current depth
        PvLine pv{};
        int score = AlphaBeta(root, root_depth, alpha, beta, /*ply=*/0, pv);

        if (search_aborted_) {
            break;
        }

        // Aspiration fail → re-search with full window
        if (score <= alpha || score >= beta) {
            alpha = -kInfinity;
            beta  = +kInfinity;
            pv.length = 0;
            score = AlphaBeta(root, root_depth, alpha, beta, /*ply=*/0, pv);

            if (search_aborted_) {
                break;
            }
        }

        // Iteration result
        prev_score = score;
        result.depth = root_depth;
        result.score_cp = score;
        result.best_move = Move{};

        if (pv.length > 0) {
            result.best_move = pv.moves[0];
        }

        result.pv = pv;
        result.nodes = nodes_;

        std::cout << "search depth: " << root_depth
                  << "; score cp: " << result.score_cp
                  << "; best move: " << FormatMoveForLog(result.best_move)
                  << std::endl;

        // Early stops: mate found or node limit reached
        if (IsMateScore(score)) {
            break;
        }

        // Reached node limit — exit iterative deepening loop
        if (limits_.nodes_limit > 0 && nodes_ >= limits_.nodes_limit) {
            break;
        }
    }

    return result;
}

int SearchEngine::Quiescence(Position& pos, int alpha, int beta, int ply, PvLine& pv) {
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

    if (n == 0) {
        pv.length = 0;
        if (in_check) {
            return -kMateScore + ply;
        }
        return alpha;
    }

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
        const int score = -Quiescence(pos, -beta, -alpha, ply + 1, child);

        pos.UndoMove(m, u);

        if (search_aborted_) {
            return 0;
        }

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

int SearchEngine::AlphaBeta(Position& pos, int depth_left, int alpha, int beta, int ply, PvLine& pv) {
    // Node or time limit check
    if (!IncreaseNodeCounter()) {
        return 0;
    }

    // Fast draw by repetition or fifty-move rule
    if (pos.IsThreefoldRepetition() || pos.IsFiftyMoveRuleDraw()) {
        return 0;
    }

    // Leaf node goes to quiescence search
    if (depth_left <= 0) {
        return Quiescence(pos, alpha, beta, ply, pv);
    }

    // Save original alpha for correct bound type when writing to TT
    const int alpha_orig = alpha;

    // Transposition table probe
    const uint64_t key = pos.GetZobristKey();
    int  tt_score = 0;
    Move tt_move{};
    if (kUseTT == true) {
        if (tt_.Probe(key, depth_left, alpha, beta, tt_score, tt_move)) {
            pv.length = 0;
            if (tt_move.GetFrom() != Move::None && tt_move.GetTo() != Move::None) {
                pv.moves[pv.length++] = tt_move;
            }

            return ScoreFromTT(tt_score, ply);
        }
    }

    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    const uint8_t king_sq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(stm, PieceType::King));
    const bool in_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), king_sq, stm);

    // Static evaluation of the node (for futility and razoring)
    // Evaluate returns score from the side of White
    const int static_eval = pos.IsWhiteToMove() ? Evaluation::Evaluate(pos) : -Evaluation::Evaluate(pos);

    // Razoring at depth 1
    if (!in_check && depth_left == 1 && static_eval + 150 <= alpha) {
        PvLine qpv{};
        const int q = Quiescence(pos, alpha - 1, alpha, ply, qpv);

        if (search_aborted_) {
            return 0;
        }

        if (q <= alpha) {
            return q;
        }
    }

    // Null-move pruning (only if not in check and there are non-pawn pieces)
    if (!in_check && depth_left >= 3) {
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
            const int nm_score = -AlphaBeta(pos, depth_left - 1 - R, -beta, -beta + 1, ply + 1, dummy);

            pos.UndoNullMove(nu);

            if (search_aborted_) {
                return 0;
            }

            if (nm_score >= beta) {
                return nm_score;
            }
        }
    }

    // Full move generation
    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/false);

    // Move ordering context (no move copying)
    const size_t n = ml.GetSize();

    if (n == 0) {
        pv.length = 0;
        if (in_check) {
            return -kMateScore + ply;
        }
        return 0;
    }

    const int ply_index = std::min(ply, 255);

    MoveOrdering::Context ctx{};
    ctx.tt_move      = tt_move;
    ctx.cutoff1      = cutoff_keys_[ply_index][0];
    ctx.cutoff2      = cutoff_keys_[ply_index][1];
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
        if (is_capture && !is_promo && depth_left <= 2) {
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
        if (!safe_check && is_simple && depth_left <= 3 && !is_tt && !is_first) {
            const int margin = (depth_left == 1 ? 100 : depth_left == 2 ? 200 : 300);
            if (static_eval + margin <= alpha) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // SEE-based pruning of obviously losing captures at shallow depths (if move is not check)
        if (!gives_check && is_capture && !is_promo && depth_left <= 2 && !is_tt && !is_first) {
            if (see < 0) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // Late move pruning: very late quiet moves that are not checks and not TT moves
        if (!safe_check && is_simple && !is_tt && depth_left > 7 && move_index >= lmr_base_index_ + 2) {
            const int quiet_limit = 2 + (depth_left * depth_left) / 2;
            if (move_index > quiet_limit) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        const int new_depth = depth_left - 1;

        int score = 0;
        PvLine child{};

        // LMR for late quiet moves
        if (is_simple && depth_left >= 3 && move_index >= lmr_base_index_) {
            const int r = 1;
            score = -AlphaBeta(pos, new_depth - r, -alpha - 1, -alpha, ply + 1, child);
            if (!search_aborted_ && score > alpha) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, ply + 1, child);
            }
        }
        else {
            // PVS: first move searched with full window, others with zero window and optional re-search
            if (is_first) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, ply + 1, child);
            }
            else {
                score = -AlphaBeta(pos, new_depth, -alpha - 1, -alpha, ply + 1, child);
                if (!search_aborted_ && score > alpha && score < beta) {
                    score = -AlphaBeta(pos, new_depth, -beta, -alpha, ply + 1, child);
                }
            }
        }

        pos.UndoMove(m, u);

        if (search_aborted_) {
            return 0;
        }

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
                if (cutoff_keys_[ply_index][0] != key16) {
                    cutoff_keys_[ply_index][1] = cutoff_keys_[ply_index][0];
                    cutoff_keys_[ply_index][0] = key16;
                }

                // After undo, side to move is restored to the original one
                const int side_index = pos.IsWhiteToMove() ? 1 : 0;
                const int from = static_cast<int>(m.GetFrom());
                const int to   = static_cast<int>(m.GetTo());
                history_[side_index][from][to] += depth_left * depth_left;

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
                tt_.Store(key, depth_left, ScoreToTT(best_score, ply), TranspositionTable::Bound::Lower, best_move);
            }
            return best_score;
        }

        // Alpha improvement: update principal variation
        if (best_score > alpha) {
            alpha = best_score;
            pv.length = 0;
            pv.moves[pv.length++] = best_move;

            for (int i = 0; i < best_child.length && pv.length < 128; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    if (search_aborted_) {
        return 0;
    }

    // Store node result in TT (bound type is chosen using original alpha)
    const auto bnd = (best_score <= alpha_orig)
                         ? TranspositionTable::Bound::Upper
                         : TranspositionTable::Bound::Exact;

    if (kUseTT == true) {
        tt_.Store(key, depth_left, ScoreToTT(best_score, ply), bnd, best_move);
    }

    return best_score;
}

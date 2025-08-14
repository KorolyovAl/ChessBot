#include <vector>
#include <algorithm>

#include "search.h"
#include "../board_state/bitboard.h"
#include "../board_state/pieces.h"
#include "../move_generation/move_list.h"
#include "../move_generation/legal_move_gen.h"
#include "evaluation.h"
#include "move_ordering.h"
#include "../move_generation/ps_legal_move_mask_gen.h"

namespace {
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
    ResetCutoffKeys();

    SearchResult result{};

    // Aspiration window seeds
    int alpha = -kInfinity;
    int beta  = +kInfinity;
    int prev_score = 0;

    const int max_depth = limits.max_depth;

    // Iterative Deepening loop
    for (int depth = 1; depth <= max_depth; ++depth) {
        // — Aspiration window around previous score (tighter as depth grows)
        int window = (depth <= 4) ? 25 : 15;
        alpha = Clamp(prev_score - window, -kInfinity, kInfinity);
        beta  = Clamp(prev_score + window, -kInfinity, kInfinity);

        // — Main AB search for the current depth
        PvLine pv{};
        int score = AlphaBeta(root, depth, alpha, beta, /*halfmove=*/0, pv);
        if (IsTimeUp()) {
            break;
        }

        // — Aspiration fail → re-search with full window
        if (score <= alpha || score >= beta) {
            alpha = -kInfinity;
            beta  = +kInfinity;
            pv.length = 0;
            score = AlphaBeta(root, depth, alpha, beta, 0, pv);
            if (IsTimeUp()) {
                break;
            }
        }

        // — Iteration result
        prev_score = score;
        result.depth = depth;
        result.score_cp = score;
        if (pv.length > 0) {
            result.best_move = pv.moves[0];
        }
        result.pv = pv;
        result.nodes = nodes_;

        // — Early stops: mate found / nodes limit
        if (IsMateScore(score)) {
            break;
        }
        if (limits.nodes_limit && nodes_ >= limits.nodes_limit) {
            break;
        }
    }

    return result;
}

int SearchEngine::Quiescence(Position& pos, int alpha, int beta, int halfmove, PvLine& pv) {
    if (IsTimeUp()) {
        return 0;
    }
    ++nodes_;

    // — Stand-pat (static eval leaf)
    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    const uint8_t ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(stm, PieceType::King));
    const bool in_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), ksq, stm);

    if (!in_check) {
        const int stand_pat = Evaluation::Evaluate(pos);
        if (stand_pat >= beta) {
            return stand_pat;
        }
        if (stand_pat > alpha) {
            alpha = stand_pat;
        }
    }

    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/!in_check);

    std::vector<Move> moves;
    moves.reserve(ml.GetSize());
    for (uint32_t i = 0; i < ml.GetSize(); ++i) {
        moves.push_back(ml[i]);
    }

    // — Order captures via MoveOrdering::Score (uses MVV-LVA/SEE inside project)
    MoveOrdering::Context qctx{};
    qctx.tt_move = Move{};
    qctx.cutoff1 = 0;
    qctx.cutoff2 = 0;

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        int sa = MoveOrdering::Score(a, pos.GetPieces(), qctx);
        int sb = MoveOrdering::Score(b, pos.GetPieces(), qctx);
        return sa > sb;
    });

    // — Search ordered captures
    PvLine best_child{};
    for (const auto& m : moves) {
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
            pv.moves[pv.length++] = m;
            for (int i = 0; i < best_child.length; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    return alpha;
}

int SearchEngine::AlphaBeta(Position& pos, int depth, int alpha, int beta, int halfmove, PvLine& pv) {
    if (IsTimeUp()) {
        return 0;
    }
    ++nodes_;

    // — Early draw detection
    if (pos.IsThreefoldRepetition() || pos.IsFiftyMoveRuleDraw()) {
        return 0;
    }

    // — Leaf: go to quiescence
    if (depth <= 0) {
        return Quiescence(pos, alpha, beta, halfmove, pv);
    }

    // — Probe TT first
    const uint64_t key = pos.GetZobristKey();
    int tt_score = 0;
    Move tt_move{};
    if (tt_.Probe(key, depth, alpha, beta, tt_score, tt_move)) {
        return ScoreFromTT(tt_score, halfmove);
    }

    // — Static evaluation (used for futility/razoring etc.)
    const int static_eval = Evaluation::Evaluate(pos);

    // — Razoring at depth==1 (cheap fail-low check)
    if (depth == 1 && static_eval + 150 <= alpha) {
        PvLine qpv{};
        const int q = Quiescence(pos, alpha - 1, alpha, halfmove, qpv);
        if (q <= alpha) {
            return q;
        }
    }

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
                const int score = -AlphaBeta(pos, depth - 1 - R, -beta, -beta + 1, halfmove + 1, dummy);

                pos.UndoNullMove(nu);

                if (score >= beta) {
                    return score;
                }
            }
        }
    }

    // — Generate all legal moves
    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/false);

    std::vector<Move> moves;
    moves.reserve(ml.GetSize());
    for (uint32_t i = 0; i < ml.GetSize(); ++i) {
        moves.push_back(ml[i]);
    }

    // — Build ordering context and sort
    MoveOrdering::Context ctx{};
    ctx.tt_move = tt_move;
    ctx.cutoff1 = cutoff_keys_[halfmove][0];
    ctx.cutoff2 = cutoff_keys_[halfmove][1];
    ctx.history = &history_;
    ctx.side_to_move = stm;

    std::sort(moves.begin(), moves.end(), [&](const Move& a, const Move& b) {
        int sa = MoveOrdering::Score(a, pos.GetPieces(), ctx);
        int sb = MoveOrdering::Score(b, pos.GetPieces(), ctx);
        return sa > sb;
    });

    // — Iterate moves with LMR on late SimpleMoves
    Move best_move{};
    PvLine best_child{};
    int best_score = -kInfinity;

    int move_index = 0;
    for (const auto& m : moves) {
        ++move_index;

        Position::Undo u{};
        pos.ApplyMove(m, u);

        const int new_depth = depth - 1;
        const bool is_simple = IsSimpleMove(m);

        int score = 0;
        if (is_simple && depth >= 3 && move_index >= lmr_base_index_) {
            // — Reduced search (LMR), narrow window
            const int r = 1;
            PvLine child{};
            score = -AlphaBeta(pos, new_depth - r, -alpha - 1, -alpha, halfmove + 1, child);

            // — If promising, re-search at full depth/window
            if (score > alpha) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
            }

            if (score > best_score) {
                best_score = score;
                best_child = child;
                best_move = m;
            }
        }
        else {
            // — Regular full-depth search
            PvLine child{};
            score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);

            if (score > best_score) {
                best_score = score;
                best_child = child;
                best_move = m;
            }
        }

        pos.UndoMove(m, u);

        // — Beta-cutoff: update CutoffMoves + History, store TT and exit
        if (best_score >= beta) {
            if (is_simple) {
                const uint16_t key16 = FromToKey(m);
                if (cutoff_keys_[halfmove][0] != key16) {
                    cutoff_keys_[halfmove][1] = cutoff_keys_[halfmove][0];
                    cutoff_keys_[halfmove][0] = key16;
                }

                const int side_index = pos.IsWhiteToMove() ? 1 : 0; // after undo
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

            tt_.Store(key, depth, ScoreToTT(best_score, halfmove), TranspositionTable::Bound::Lower, best_move);
            return best_score;
        }

        // — Improve alpha and extend PV
        if (best_score > alpha) {
            alpha = best_score;
            pv.length = 0;
            pv.moves[pv.length++] = best_move;
            for (int i = 0; i < best_child.length; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    // — Store node result in TT
    const auto bnd = (best_score <= alpha) ? TranspositionTable::Bound::Upper
                                           : TranspositionTable::Bound::Exact;
    tt_.Store(key, depth, ScoreToTT(best_score, halfmove), bnd, best_move);
    return best_score;
}

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
#include "static_exchange_evaluation.h"
#include "piece_values.h"

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
    limits_ = limits;
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
        if (limits_.nodes_limit > 0 && nodes_ >= limits_.nodes_limit) {
            break; // достигнут лимит нод — выходим из ID цикла
        }
    }

    return result;
}

int SearchEngine::Quiescence(Position& pos, int alpha, int beta, int halfmove, PvLine& pv) {
    if (!IncreaseNodeCounter()) return 0;

    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    const uint8_t ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(stm, PieceType::King));
    const bool in_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), ksq, stm);

    int stand_pat = 0;
    if (!in_check) {
        stand_pat = Evaluation::Evaluate(pos);
        if (stand_pat >= beta) return stand_pat;
        if (stand_pat > alpha) alpha = stand_pat;
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
    std::sort(order.begin(), order.end(), [&](size_t ia, size_t ib){ return scores[ia] > scores[ib]; });

    PvLine best_child{};
    for (size_t oi = 0; oi < order.size(); ++oi) {
        const Move& m = ml[order[oi]];

        // ВНЕ шаха: дельта и SEE-фильтр для взятий
        if (!in_check) {
            const bool is_ep  = (m.GetFlag() == Move::Flag::EnPassantCapture);
            const bool is_cap = is_ep || (m.GetDefenderType() != Move::None);
            const bool is_promo =
                m.GetFlag() == Move::Flag::PromoteToQueen  ||
                m.GetFlag() == Move::Flag::PromoteToRook   ||
                m.GetFlag() == Move::Flag::PromoteToBishop ||
                m.GetFlag() == Move::Flag::PromoteToKnight;

            if (is_cap) {
                // дешёвый дельта-чек: если даже максимальный прирост не достаёт до альфы — пропускаем
                const int victim = is_ep ? EvalValues::kPieceValueCp[PieceType::Pawn]
                                         : EvalValues::kPieceValueCp[static_cast<int>(m.GetDefenderType())];
                const int DELTA = 90; // чуть консервативно
                if (stand_pat + victim + DELTA < alpha) {
                    continue;
                }

                // SEE: убыточные взятия не разворачиваем
                if (!is_promo) {
                    const int see = StaticExchangeEvaluation::Capture(pos.GetPieces(), m);
                    if (see < 0) {
                        continue;
                    }
                }
            } else {
                // В qsearch без шаха тихие ходы не рассматриваем
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
            if (pv.length < 128) pv.moves[pv.length++] = m;
            for (int i = 0; i < best_child.length && pv.length < 128; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    return alpha;
}

int SearchEngine::AlphaBeta(Position& pos, int depth, int alpha, int beta, int halfmove, PvLine& pv) {
    // лимит по узлам/времени
    if (!IncreaseNodeCounter()) {
        return 0;
    }

    // быстрая ничья
    if (pos.IsThreefoldRepetition() || pos.IsFiftyMoveRuleDraw()) {
        return 0;
    }

    // лист → qsearch
    if (depth <= 0) {
        return Quiescence(pos, alpha, beta, halfmove, pv);
    }

    // сохраняем исходное alpha для правильной Bound при записи в ТТ
    const int alpha_orig = alpha;

    // --- TT probe
    const uint64_t key = pos.GetZobristKey();
    int  tt_score = 0;
    Move tt_move{};
    if (tt_.Probe(key, depth, alpha, beta, tt_score, tt_move)) {
        return ScoreFromTT(tt_score, halfmove);
    }

    // статическая оценка узла (нужна для futility/razoring)
    const int static_eval = Evaluation::Evaluate(pos);

    // razoring на глубине 1
    if (depth == 1 && static_eval + 150 <= alpha) {
        PvLine qpv{};
        const int q = Quiescence(pos, alpha - 1, alpha, halfmove, qpv);
        if (q <= alpha) {
            return q;
        }
    }

    // --- Null-move pruning (если не под шахом и есть неладьи/слоны/ферзи/кони)
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

    // --- генерация ходов
    const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
    MoveList ml;
    LegalMoveGen::Generate(pos, stm, ml, /*only_captures=*/false);

    // --- контекст сортировки (без копирования Move)
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

    // --- перебор с LMR + PVS и прюнингами
    Move   best_move{};
    PvLine best_child{};
    int    best_score = -kInfinity;

    int move_index = 0;
    for (size_t oi = 0; oi < order.size(); ++oi) {
        ++move_index;
        const Move& m = ml[order[oi]];

        const bool is_promo   = IsPromotionFlag(m.GetFlag());
        const bool is_capture = (m.GetDefenderType() != Move::None) || (m.GetFlag() == Move::Flag::EnPassantCapture);
        const bool is_simple  = !is_capture && !is_promo;

        // узнаем, tt-ход ли
        const bool is_tt = (m.GetFrom() == tt_move.GetFrom() &&
                            m.GetTo()   == tt_move.GetTo()   &&
                            m.GetFlag() == tt_move.GetFlag());
        const bool is_first = (move_index == 1);

        // SEE перед применением (только для взятий и на малых глубинах)
        int see = 0;
        if (is_capture && !is_promo && depth <= 2) {
            see = StaticExchangeEvaluation::Capture(pos.GetPieces(), m);
        }

        // применяем ход
        Position::Undo u{};
        pos.ApplyMove(m, u);

        // проверим, даёт ли ход шах (чтобы не прюнить такие)
        const Side child_stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
        const uint8_t child_ksq = BOp::BitScanForward(pos.GetPieces().GetPieceBitboard(child_stm, PieceType::King));
        const bool gives_check = PsLegalMaskGen::SquareInDanger(pos.GetPieces(), child_ksq, child_stm);

        // --- Futility pruning тихих на неглубоких слоях (если ход НЕ шах)
        if (!gives_check && is_simple && depth <= 3 && !is_tt && !is_first) {
            const int margin = (depth == 1 ? 100 : depth == 2 ? 200 : 300);
            if (static_eval + margin <= alpha) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // --- SEE-прюнинг явных минусовых взятий (на малых глубинах и если ход НЕ шах)
        if (!gives_check && is_capture && !is_promo && depth <= 2 && !is_tt && !is_first) {
            if (see < 0) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // --- Late Move Pruning: очень поздние тихие (не шах, не TT)
        if (!gives_check && is_simple && !is_tt && move_index >= lmr_base_index_ + 2) {
            const int quiet_limit = 2 + (depth * depth) / 2;
            if (move_index > quiet_limit) {
                pos.UndoMove(m, u);
                continue;
            }
        }

        // глубина для ребёнка
        const int new_depth = depth - 1;

        // PVS + LMR
        int score = 0;
        PvLine child{};

        if (is_simple && depth >= 3 && move_index >= lmr_base_index_) {
            // LMR: сначала редуцированный нуль-окно, при необходимости полный пересчёт
            const int r = 1;
            score = -AlphaBeta(pos, new_depth - r, -alpha - 1, -alpha, halfmove + 1, child);
            if (score > alpha) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
            }
        } else {
            // PVS: первый ход — полноокно, остальные — нуль-окно с возможным расширением
            if (is_first) {
                score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
            } else {
                score = -AlphaBeta(pos, new_depth, -alpha - 1, -alpha, halfmove + 1, child);
                if (score > alpha && score < beta) {
                    score = -AlphaBeta(pos, new_depth, -beta, -alpha, halfmove + 1, child);
                }
            }
        }

        pos.UndoMove(m, u);

        // обновляем лучший
        if (score > best_score) {
            best_score  = score;
            best_child  = child;
            best_move   = m;
        }

        // бета-срез: апдейт history/cutoff, запись в ТТ и выход
        if (best_score >= beta) {
            if (is_simple) {
                const uint16_t key16 = FromToKey(m);
                if (cutoff_keys_[halfmove][0] != key16) {
                    cutoff_keys_[halfmove][1] = cutoff_keys_[halfmove][0];
                    cutoff_keys_[halfmove][0] = key16;
                }

                const int side_index = pos.IsWhiteToMove() ? 1 : 0; // после undo вернулся исходный ходящий
                const int from = static_cast<int>(m.GetFrom());
                const int to   = static_cast<int>(m.GetTo());
                history_[side_index][from][to] += depth * depth;

                if (history_[side_index][from][to] > 32767) {
                    for (int s = 0; s < 2; ++s)
                        for (int f = 0; f < 64; ++f)
                            for (int t = 0; t < 64; ++t)
                                history_[s][f][t] /= 2;
                }
            }

            tt_.Store(key, depth, ScoreToTT(best_score, halfmove), TranspositionTable::Bound::Lower, best_move);
            return best_score;
        }

        // улучшили альфу — обновляем PV
        if (best_score > alpha) {
            alpha = best_score;
            pv.length = 0;
            pv.moves[pv.length++] = best_move;
            for (int i = 0; i < best_child.length; ++i) {
                pv.moves[pv.length++] = best_child.moves[i];
            }
        }
    }

    // запись результата узла в ТТ (граница сравнивается с ИСХОДНЫМ alpha)
    const auto bnd = (best_score <= alpha_orig)
                         ? TranspositionTable::Bound::Upper
                         : TranspositionTable::Bound::Exact;
    tt_.Store(key, depth, ScoreToTT(best_score, halfmove), bnd, best_move);
    return best_score;
}


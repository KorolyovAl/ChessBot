#include "game_controller.h"

#include <utility>
#include <sstream>
#include <atomic>

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "../board_state/pieces.h"
#include "../board_state/bitboard.h"
#include "../move_generation/legal_move_gen.h"
#include "../move_generation/move_list.h"
#include "../move_generation/ps_legal_move_mask_gen.h"
#include "../ai_logic/evaluation.h"
#include "../ai_logic/search.h"

namespace {
    // Глобальный флаг остановки поиска (для StopSearch)
    static std::atomic<bool> g_stop_requested{false};

    inline bool IsSquareAttackedByEnemy(const Pieces& pcs, uint8_t sq, Side side) {
        return PsLegalMaskGen::SquareInDanger(pcs, sq, side);
    }

    inline bool HasNoLegalMoves(const Position& pos, Side side) {
        MoveList list;
        LegalMoveGen::Generate(pos, side, list, false);
        return list.GetSize() == 0;
    }

    inline bool IsSideInCheck(const Position& pos, Side side) {
        const Bitboard kbb = pos.GetPieces().GetPieceBitboard(side, PieceType::King);
        if (kbb == 0ULL) {
            return false;
        }
        const uint8_t king_sq = BOp::BitScanForward(kbb);
        return IsSquareAttackedByEnemy(pos.GetPieces(), king_sq, side);
    }

    inline bool IsPromotionFlag(Move::Flag f) {
        switch (f) {
            case Move::Flag::PromoteToKnight:
            case Move::Flag::PromoteToBishop:
            case Move::Flag::PromoteToRook:
            case Move::Flag::PromoteToQueen:
                return true;
            default:
                return false;
        }
    }

    inline GameResult DetectResult(const Position& pos) {
        if (pos.IsFiftyMoveRuleDraw()) {
            return GameResult::DrawFiftyMove;
        }
        if (pos.IsThreefoldRepetition()) {
            return GameResult::DrawRepetition;
        }

        const Side stm = pos.IsWhiteToMove() ? Side::White : Side::Black;
        if (HasNoLegalMoves(pos, stm)) {
            if (IsSideInCheck(pos, stm)) {
                return (stm == Side::White) ? GameResult::BlackWon : GameResult::WhiteWon;
            } else {
                return GameResult::DrawStalemate;
            }
        }
        return GameResult::Ongoing;
    }

    inline int EvaluateCp(const Position& pos) {
        return Evaluation::Evaluate(pos);
    }

    inline Move::Flag PromotionFlagForPieceType(uint8_t piece_type) {
        switch (static_cast<PieceType>(piece_type)) {
            case PieceType::Knight:
                return Move::Flag::PromoteToKnight;
            case PieceType::Bishop:
                return Move::Flag::PromoteToBishop;
            case PieceType::Rook:
                return Move::Flag::PromoteToRook;
            case PieceType::Queen:
                return Move::Flag::PromoteToQueen;
            default:
                return Move::Flag::Default;
        }
    }

    inline bool IsEngineToMove(const Position& pos, const Players& players) {
        const bool white_to_move = pos.IsWhiteToMove();

        if (white_to_move)  {
            return players.white == PlayerType::Engine;
        }
        else {
            return players.black == PlayerType::Engine;
        }
    }
} // namespace

GameController::GameController(TranspositionTable& table)
    : table_(table) {
}

void GameController::NewGame(const Players& players, const TimeControl& tc) {
    players_ = players;
    time_control_ = tc;
    result_ = GameResult::Ongoing;

    position_.reset(new Position(
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
        Position::NONE,
        true, true, true, true,
        0
        ));

    state_ = ControllerState::PlayerTurn;
    EmitPosition_();

    if (IsEngineToMove(*position_, players_)) {
        EnterEngineThinking_();
    }
}

void GameController::LoadFEN(const std::string& short_fen, const Players& players, const TimeControl& tc) {
    players_ = players;
    time_control_ = tc;
    result_ = GameResult::Ongoing;

    position_.reset(new Position(
        short_fen,
        Position::NONE,
        true, true, true, true,
        0
        ));

    state_ = ControllerState::PlayerTurn;
    EmitPosition_();

    if (IsEngineToMove(*position_, players_)) {
        EnterEngineThinking_();
    }
}

bool GameController::MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type) {
    if (!position_) {
        return false;
    }

    const Side side = position_->IsWhiteToMove() ? Side::White : Side::Black;

    MoveList list;
    LegalMoveGen::Generate(*position_, side, list, false);

    Move chosen{};
    bool found = false;
    const uint8_t size = list.GetSize();

    for (uint8_t i = 0; i < size; ++i) {
        const Move m = list[i];
        if (m.GetFrom() != from || m.GetTo() != to) {
            continue;
        }

        const auto flag = m.GetFlag();
        if (!IsPromotionFlag(flag)) {
            if (promo_piece_type == 0) {
                chosen = m;
                found = true;
                break;
            }
            continue; // запросили промо-тип, а ход — не промо
        } else {
            if (promo_piece_type == 0) {
                continue;
            }
            const Move::Flag want_flag = PromotionFlagForPieceType(promo_piece_type);
            if (want_flag == flag) {
                chosen = m;
                found = true;
                break;
            }
        }
    }

    if (!found) {
        return false;
    }

    Position::Undo u{};
    position_->ApplyMove(chosen, u);

    const int eval_cp = EvaluateCp(*position_);
    if (on_move_) {
        on_move_(chosen, /*halfmove_index*/ 0, /*eval_centipawns*/ eval_cp);
    }
    EmitPosition_();

    result_ = DetectResult(*position_);
    if (result_ != GameResult::Ongoing) {
        state_ = ControllerState::GameOver;
        if (on_game_over_) {
            const char* reason = nullptr;
            switch (result_) {
                case GameResult::DrawFiftyMove:
                    reason = "draw by fifty-move rule";
                    break;
                case GameResult::DrawRepetition:
                    reason = "draw by threefold repetition";
                    break;
                case GameResult::DrawStalemate:
                    reason = "stalemate";
                    break;
                case GameResult::WhiteWon:
                    reason = "checkmate — White wins";
                    break;
                case GameResult::BlackWon:
                    reason = "checkmate — Black wins";
                    break;
                default:
                    reason = "";
                    break;
            }
            on_game_over_(result_, std::string(reason));
        }
        return true;
    }

    if (IsEngineToMove(*position_, players_)) {
        EnterEngineThinking_();
    } else {
        EnterPlayerTurn_();
    }

    return true;
}

void GameController::RequestLegalMask(uint8_t square) {
    if (!on_legal_mask_) {
        return;
    }

    if (!position_) {
        on_legal_mask_(square, 0ULL);
        return;
    }

    const Side side = position_->IsWhiteToMove() ? Side::White : Side::Black;

    MoveList list;
    LegalMoveGen::Generate(*position_, side, list, false);

    uint64_t mask = 0ULL;
    const uint8_t size = list.GetSize();
    for (uint8_t i = 0; i < size; ++i) {
        const Move m = list[i];
        if (m.GetFrom() == square) {
            mask |= (1ULL << m.GetTo());
        }
    }

    on_legal_mask_(square, mask);
}

void GameController::SetEngineLimits(const EngineLimits& lim) {
    engine_limits_ = lim;
}

void GameController::SetEngineSide(Side side, bool enabled) {
    if (side == Side::White) {
        players_.white = enabled ? PlayerType::Engine : PlayerType::Human;
    } else {
        players_.black = enabled ? PlayerType::Engine : PlayerType::Human;
    }
}

void GameController::StopSearch() {
    g_stop_requested.store(true, std::memory_order_relaxed);
}

std::string GameController::GetFEN() const {
    if (!position_) {
        return std::string{};
    }

    std::ostringstream oss;
    oss << *position_;
    return oss.str();
}

GameResult GameController::GetResult() const {
    return result_;
}

void GameController::SetOnPosition(OnPosition callback) {
    on_position_ = std::move(callback);
}
void GameController::SetOnMove(OnMove callback) {
    on_move_ = std::move(callback);
}
void GameController::SetOnSearchInfo(OnSearchInfo callback) {
    on_search_info_ = std::move(callback);
}
void GameController::SetOnBestMove(OnBestMove callback) {
    on_best_move_  = std::move(callback);
}
void GameController::SetOnGameOver(OnGameOver callback) {
    on_game_over_  = std::move(callback);
}
void GameController::SetOnLegalMask(OnLegalMask callback) {
    on_legal_mask_ = std::move(callback);
}

void GameController::EnterPlayerTurn_() {
    state_ = ControllerState::PlayerTurn;
}

void GameController::EnterEngineThinking_() {
    if (!position_) {
        return;
    }
    state_ = ControllerState::EngineThinking;

    if (!engine_) {
        engine_.reset(new SearchEngine(table_));
    }

    g_stop_requested.store(false, std::memory_order_relaxed);
    engine_->SetStopCallback([]() noexcept -> bool {
        return g_stop_requested.load(std::memory_order_relaxed);
    });

    SearchLimits limits{};
    if (engine_limits_.max_depth > 0) {
        limits.max_depth   = engine_limits_.max_depth;
    }

    if (engine_limits_.max_nodes > 0) {
        limits.nodes_limit = engine_limits_.max_nodes;
    }

    // Синхронный поиск (если нужен отдельный поток — вынесем позднее)
    SearchResult res = engine_->Search(*position_, limits);

    if (on_best_move_) {
        std::ostringstream pv;
        for (int i = 0; i < res.pv.length; ++i) {
            const Move m = res.pv.moves[i];
            pv << static_cast<int>(m.GetFrom()) << "-" << static_cast<int>(m.GetTo());
            if (i + 1 < res.pv.length) {
                pv << ' ';
            }
        }
        on_best_move_(res.best_move, pv.str());
    }

    if (res.best_move.GetFrom() != Move::None && res.best_move.GetTo() != Move::None) {
        Position::Undo u{};
        position_->ApplyMove(res.best_move, u);

        const int eval_cp = EvaluateCp(*position_);
        if (on_move_) {
            on_move_(res.best_move, /*halfmove_index*/ 0, /*eval_centipawns*/ eval_cp);
        }
        EmitPosition_();

        result_ = DetectResult(*position_);
        if (result_ != GameResult::Ongoing) {
            state_ = ControllerState::GameOver;
            if (on_game_over_) {
                const char* reason = nullptr;
                switch (result_) {
                    case GameResult::DrawFiftyMove:
                        reason = "draw by fifty-move rule";
                        break;
                    case GameResult::DrawRepetition:
                        reason = "draw by threefold repetition";
                        break;
                    case GameResult::DrawStalemate:
                        reason = "stalemate";
                        break;
                    case GameResult::WhiteWon:
                        reason = "checkmate — White wins";
                        break;
                    case GameResult::BlackWon:
                        reason = "checkmate — Black wins";
                        break;
                    default:
                        reason = "";
                        break;
                }
                on_game_over_(result_, std::string(reason));
            }
            return;
        }
    }

    EnterPlayerTurn_();
}

void GameController::ApplyMoveAndNotify_(const Move& m, int eval_cp) {
    Position::Undo u{};
    position_->ApplyMove(m, u);
    if (on_move_) {
        on_move_(m, /*halfmove_index*/ 0, /*eval_centipawns*/ eval_cp);
    }
    EmitPosition_();
}

void GameController::EmitPosition_() const {
    if (on_position_ && position_) {
        on_position_(*position_);
    }
}

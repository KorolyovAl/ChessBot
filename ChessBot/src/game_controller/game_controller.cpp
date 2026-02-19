#include "game_controller.h"

#include <utility>
#include <sstream>
#include <atomic>

#include "../board_state/position.h"
#include "../board_state/move.h"
#include "../board_state/bitboard.h"
#include "../move_generation/legal_move_gen.h"
#include "../move_generation/move_list.h"
#include "../move_generation/ps_legal_move_mask_gen.h"
#include "../ai_logic/evaluation.h"

// Anonymous namespace holds internal helpers and local state
namespace {

    inline bool IsSquareAttackedByEnemy(const Pieces& pcs, uint8_t sq, Side side) {
        return PsLegalMaskGen::SquareInDanger(pcs, sq, side);
    }

    inline bool HasNoLegalMoves(const Position& pos, Side side) {
        MoveList list;
        LegalMoveGen::Generate(pos, side, list, false);
        return list.GetSize() == 0;
    }

    // Returns true if the side’s king is in check
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

    // Determines the current game result based on terminal conditions (50-move, repetition, mate/stalemate)
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

    // Static evaluation in centipawns for the current position
    inline int EvaluateCp(const Position& pos) {
        return Evaluation::Evaluate(pos);
    }

    // Maps a UI-provided promotion piece type to the corresponding move flag
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

// Validates and applies a user move; handles promotions, emits events and advances the game state
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
            // Non-promotion move: accept only if UI did not request a promotion piece
            if (promo_piece_type == 0) {
                chosen = m;
                found = true;
                break;
            }
            continue;
        } else {
            // Promotion move: accept only if UI provided a matching promotion piece type
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

    // Apply move and notify listeners about the move and the new position
    Position::Undo u{};
    position_->ApplyMove(chosen, u);

    const int eval_cp = EvaluateCp(*position_);
    if (on_move_) {
        on_move_(chosen, /*halfmove_index*/ 0, /*eval_centipawns*/ eval_cp);
    }
    EmitPosition_();

    // Check for terminal state and either finish the game or pass control to the next side
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

// Computes and emits a bitmask of legal targets for a given origin square
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

// Stores engine search limits to be used on the next search start
void GameController::SetEngineLimits(const EngineLimits& lim) {
    engine_limits_ = lim;
}

// Enables or disables the engine for a given side and updates players configuration
void GameController::SetEngineSide(Side side, bool enabled) {
    if (side == Side::White) {
        players_.white = enabled ? PlayerType::Engine : PlayerType::Human;
    } else {
        players_.black = enabled ? PlayerType::Engine : PlayerType::Human;
    }
}

// Exports the current position as a short FEN string
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

int GameController::GetPiece(int square) const {
    if (position_ == nullptr) {
        throw std::logic_error("Position is nullptr");
    }

    auto [side, piece] = position_->GetPieces().GetPiece(square);

    // return a index of pieces, where 0 - none piece, 1-6 - white side; 7-12 - black side
    // 1 - pawn; 2 - knight; 3 - bishop; 4 - rook; 5 - queen; 6 - king
    if (piece == PieceType::None) {
        return 0;
    }

    const int base =
        (piece == PieceType::Pawn)   ? 1 :
        (piece == PieceType::Knight) ? 2 :
        (piece == PieceType::Bishop) ? 3 :
        (piece == PieceType::Rook)   ? 4 :
        (piece == PieceType::Queen)  ? 5 :
        /* King */                     6;
    return (side == Side::White) ? base : base + 6;
}

// Registers UI callbacks
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

// Runs a synchronous engine search, applies best move if any, and updates state/result accordingly
void GameController::EnterEngineThinking_() {
    if (!position_) {
        return;
    }
    state_ = ControllerState::EngineThinking;

    if (!engine_) {
        engine_.reset(new SearchEngine(table_));
    }

    SearchLimits limits{};
    if (engine_limits_.max_depth > 0) {
        limits.max_depth   = engine_limits_.max_depth;
    }

    if (engine_limits_.max_nodes > 0) {
        limits.nodes_limit = engine_limits_.max_nodes;
    }

    // Synchronous search
    SearchResult res = engine_->Search(*position_, limits);

    // Emit best move along with a simple textual PV representation
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

    // Apply best move if it looks valid and then evaluate, update position and check for terminal state
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

// Emits the current position snapshot via the OnPosition callback if it is set
void GameController::EmitPosition_() const {
    if (on_position_ && position_) {
        on_position_(*position_);
    }
}

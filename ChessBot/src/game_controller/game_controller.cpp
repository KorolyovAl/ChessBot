#include "game_controller.h"

#include <sstream>
#include <stdexcept>

#include "../engine_core/board_state/position.h"
#include "../engine_core/board_state/bitboard.h"
#include "../engine_core/move_generation/legal_move_gen.h"
#include "../engine_core/move_generation/move_list.h"
#include "../engine_core/move_generation/ps_legal_move_mask_gen.h"
#include "../engine_core/ai_logic/evaluation.h"

namespace {

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

inline bool IsPromotionFlag(Move::Flag flag) {
    switch (flag) {
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
        }
        return GameResult::DrawStalemate;
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

inline bool IsEngineSideToMove(const Position& pos, const Players& players) {
    if (pos.IsWhiteToMove()) {
        return players.white == PlayerType::Engine;
    }
    return players.black == PlayerType::Engine;
}

inline std::string ResultReason(GameResult result) {
    switch (result) {
    case GameResult::DrawFiftyMove:
        return "draw by fifty-move rule";
    case GameResult::DrawRepetition:
        return "draw by threefold repetition";
    case GameResult::DrawStalemate:
        return "stalemate";
    case GameResult::WhiteWon:
        return "checkmate — White wins";
    case GameResult::BlackWon:
        return "checkmate — Black wins";
    case GameResult::DrawMaterial:
        return "draw by insufficient material";
    case GameResult::Ongoing:
    default:
        return "";
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

    UpdateStateAfterTurn();
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

    UpdateStateAfterTurn();
}

MoveOutcome GameController::MakeUserMove(uint8_t from, uint8_t to, uint8_t promo_piece_type) {
    MoveOutcome outcome{};

    if (!position_) {
        return outcome;
    }

    if (state_ != ControllerState::PlayerTurn) {
        return outcome;
    }

    const Side side = position_->IsWhiteToMove() ? Side::White : Side::Black;
    const PlayerType player_type = (side == Side::White) ? players_.white : players_.black;
    if (player_type != PlayerType::Human) {
        return outcome;
    }

    MoveList list;
    LegalMoveGen::Generate(*position_, side, list, false);

    Move chosen{};
    bool found = false;
    const uint8_t size = list.GetSize();

    for (uint8_t i = 0; i < size; ++i) {
        const Move move = list[i];
        if (move.GetFrom() != from || move.GetTo() != to) {
            continue;
        }

        const Move::Flag flag = move.GetFlag();
        if (!IsPromotionFlag(flag)) {
            if (promo_piece_type == 0) {
                chosen = move;
                found = true;
                break;
            }
            continue;
        }

        if (promo_piece_type == 0) {
            continue;
        }

        const Move::Flag wanted_flag = PromotionFlagForPieceType(promo_piece_type);
        if (wanted_flag == flag) {
            chosen = move;
            found = true;
            break;
        }
    }

    if (!found) {
        return outcome;
    }

    Position::Undo undo{};
    position_->ApplyMove(chosen, undo);

    outcome.is_valid = true;
    outcome.move = chosen;
    outcome.eval_cp = EvaluateCp(*position_);

    result_ = DetectResult(*position_);
    outcome.result = result_;
    UpdateStateAfterTurn();

    return outcome;
}

EngineTurnOutcome GameController::RunEngineTurn() {
    EngineTurnOutcome outcome{};

    if (!position_) {
        return outcome;
    }

    if (!IsEngineToMove()) {
        return outcome;
    }

    state_ = ControllerState::EngineThinking;

    if (!engine_) {
        engine_.reset(new SearchEngine(table_));
    }

    SearchLimits limits{};
    if (engine_limits_.max_depth > 0) {
        limits.max_depth = engine_limits_.max_depth;
    }
    if (engine_limits_.max_nodes > 0) {
        limits.nodes_limit = engine_limits_.max_nodes;
    }

    SearchResult result = engine_->Search(*position_, limits);

    std::ostringstream pv;
    for (int i = 0; i < result.pv.length; ++i) {
        const Move move = result.pv.moves[i];
        pv << static_cast<int>(move.GetFrom()) << "-" << static_cast<int>(move.GetTo());
        if (i + 1 < result.pv.length) {
            pv << ' ';
        }
    }

    outcome.principal_variation = pv.str();

    if (result.best_move.GetFrom() == Move::None || result.best_move.GetTo() == Move::None) {
        result_ = DetectResult(*position_);
        outcome.result = result_;
        UpdateStateAfterTurn();
        return outcome;
    }

    Position::Undo undo{};
    position_->ApplyMove(result.best_move, undo);

    outcome.best_move_found = true;
    outcome.best_move = result.best_move;
    outcome.eval_cp = EvaluateCp(*position_);

    result_ = DetectResult(*position_);
    outcome.result = result_;
    UpdateStateAfterTurn();

    return outcome;
}

uint64_t GameController::GetLegalMask(uint8_t square) const {
    if (!position_) {
        return 0ULL;
    }

    if (state_ != ControllerState::PlayerTurn) {
        return 0ULL;
    }

    const Side side = position_->IsWhiteToMove() ? Side::White : Side::Black;
    const PlayerType player_type = (side == Side::White) ? players_.white : players_.black;
    if (player_type != PlayerType::Human) {
        return 0ULL;
    }

    MoveList list;
    LegalMoveGen::Generate(*position_, side, list, false);

    uint64_t mask = 0ULL;
    const uint8_t size = list.GetSize();
    for (uint8_t i = 0; i < size; ++i) {
        const Move move = list[i];
        if (move.GetFrom() == square) {
            mask |= (1ULL << move.GetTo());
        }
    }

    return mask;
}

void GameController::SetEngineLimits(const EngineLimits& limits) {
    engine_limits_ = limits;
}

void GameController::SetEngineSide(Side side, bool enabled) {
    if (side == Side::White) {
        players_.white = enabled ? PlayerType::Engine : PlayerType::Human;
    } else {
        players_.black = enabled ? PlayerType::Engine : PlayerType::Human;
    }

    UpdateStateAfterTurn();
}

bool GameController::HasPosition() const {
    return position_ != nullptr;
}

bool GameController::IsWhiteToMove() const {
    if (!position_) {
        return true;
    }
    return position_->IsWhiteToMove();
}

bool GameController::IsEngineToMove() const {
    if (!position_) {
        return false;
    }
    return IsEngineSideToMove(*position_, players_);
}

ControllerState GameController::GetState() const {
    return state_;
}

std::string GameController::GetFEN() const {
    if (!position_) {
        return std::string{};
    }

    std::ostringstream output;
    output << *position_;
    return output.str();
}

std::string GameController::GetResultReason() const {
    return ResultReason(result_);
}

GameResult GameController::GetResult() const {
    return result_;
}

int GameController::GetPiece(int square) const {
    if (position_ == nullptr) {
        throw std::logic_error("Position is nullptr");
    }

    auto [side, piece] = position_->GetPieces().GetPiece(square);

    if (piece == PieceType::None) {
        return 0;
    }

    const int base =
        (piece == PieceType::Pawn)   ? 1 :
            (piece == PieceType::Knight) ? 2 :
            (piece == PieceType::Bishop) ? 3 :
            (piece == PieceType::Rook)   ? 4 :
            (piece == PieceType::Queen)  ? 5 :
            6;

    return (side == Side::White) ? base : base + 6;
}

void GameController::UpdateStateAfterTurn() {
    if (!position_) {
        state_ = ControllerState::Null;
        return;
    }

    if (result_ != GameResult::Ongoing) {
        state_ = ControllerState::GameOver;
        return;
    }

    state_ = IsEngineSideToMove(*position_, players_) ? ControllerState::EngineThinking : ControllerState::PlayerTurn;
}

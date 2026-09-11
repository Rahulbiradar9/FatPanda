import chess
import chess.engine
import chess.pgn
import time
import sys
from datetime import datetime

FAT_PANDA_PATH = r"d:\projcets\Chess-engine\build\Release\FatPanda.exe"
STOCKFISH_PATH = r"d:\projcets\Chess-engine\tools\stockfish\stockfish\stockfish-windows-x86-64-universal.exe"
PGN_PATH = r"d:\projcets\Chess-engine\matches\fatpanda_vs_stockfish.pgn"

ROUNDS = [
    {"round": 1, "fp_color": chess.WHITE, "sf_elo": 2000, "desc": "Stockfish Elo 2000 (Candidate Master)"},
    {"round": 2, "fp_color": chess.BLACK, "sf_elo": 2000, "desc": "Stockfish Elo 2000 (Candidate Master)"},
    {"round": 3, "fp_color": chess.WHITE, "sf_elo": 2400, "desc": "Stockfish Elo 2400 (International Master)"},
    {"round": 4, "fp_color": chess.BLACK, "sf_elo": 2400, "desc": "Stockfish Elo 2400 (International Master)"},
    {"round": 5, "fp_color": chess.WHITE, "sf_elo": 2800, "desc": "Stockfish Elo 2800 (Grandmaster)"},
    {"round": 6, "fp_color": chess.BLACK, "sf_elo": 2800, "desc": "Stockfish Elo 2800 (Grandmaster)"},
    {"round": 7, "fp_color": chess.WHITE, "sf_elo": 3190, "desc": "Stockfish Elo 3190 (Super-Grandmaster)"},
    {"round": 8, "fp_color": chess.BLACK, "sf_elo": 3190, "desc": "Stockfish Elo 3190 (Super-Grandmaster)"},
    {"round": 9, "fp_color": chess.WHITE, "sf_elo": None, "desc": "Stockfish Full Power (Unconstrained NNUE)"},
    {"round": 10, "fp_color": chess.BLACK, "sf_elo": None, "desc": "Stockfish Full Power (Unconstrained NNUE)"},
]

def main():
    print("=" * 65)
    print("      FatPanda vs Stockfish 19 — 10-Game Benchmark Match")
    print("=" * 65)
    print(f"Time control: 0.4s per move | Hash: 64 MB | Threads: 1")
    print(f"PGN Output: {PGN_PATH}\n")

    fp_score = 0.0
    sf_score = 0.0

    pgn_file = open(PGN_PATH, "w")

    for r_info in ROUNDS:
        r_num = r_info["round"]
        fp_white = (r_info["fp_color"] == chess.WHITE)
        sf_elo = r_info["sf_elo"]
        desc = r_info["desc"]

        white_name = "FatPanda" if fp_white else f"Stockfish ({sf_elo or 'Full'})"
        black_name = f"Stockfish ({sf_elo or 'Full'})" if fp_white else "FatPanda"

        print("-" * 65)
        print(f"ROUND {r_num}/10: {white_name} (White) vs {black_name} (Black)")
        print(f"Setting: {desc}")
        print("-" * 65)

        # Start engines
        fp_engine = chess.engine.SimpleEngine.popen_uci(FAT_PANDA_PATH)
        fp_engine.configure({"Hash": 64, "Threads": 1})

        sf_engine = chess.engine.SimpleEngine.popen_uci(STOCKFISH_PATH)
        if sf_elo:
            sf_engine.configure({"Hash": 64, "Threads": 1, "UCI_LimitStrength": True, "UCI_Elo": sf_elo})
        else:
            sf_engine.configure({"Hash": 64, "Threads": 1, "UCI_LimitStrength": False, "Skill Level": 20})

        board = chess.Board()
        game = chess.pgn.Game()
        game.headers["Event"] = "FatPanda vs Stockfish Benchmark Match"
        game.headers["Site"] = "Local Host"
        game.headers["Date"] = datetime.now().strftime("%Y.%m.%d")
        game.headers["Round"] = str(r_num)
        game.headers["White"] = white_name
        game.headers["Black"] = black_name
        game.headers["TimeControl"] = "0.4s/move"

        node = game
        move_num = 1
        live_line = ""

        while not board.is_game_over(claim_draw=True) and move_num <= 150:
            is_fp_turn = (board.turn == chess.WHITE and fp_white) or (board.turn == chess.BLACK and not fp_white)
            active_engine = fp_engine if is_fp_turn else sf_engine

            try:
                res = active_engine.play(board, chess.engine.Limit(time=0.25))
                move = res.move
            except Exception as e:
                print(f"Engine error: {e}")
                break

            if not move or move not in board.legal_moves:
                print(f"Illegal or no move played: {move}")
                break

            san_str = board.san(move)
            node = node.add_variation(move)

            if board.turn == chess.WHITE:
                print(f"  {board.fullmove_number}. {san_str}", end=" ", flush=True)
            else:
                print(f"{san_str}", flush=True)

            board.push(move)
            move_num += 1

        if board.turn == chess.BLACK:
            print() # new line if game ended on White's move

        # Game outcome
        outcome = board.outcome(claim_draw=True)
        if outcome:
            result_str = outcome.result()
            term_reason = outcome.termination.name
        else:
            result_str = "1/2-1/2"
            term_reason = "Max moves reached (Draw)"

        game.headers["Result"] = result_str
        game.headers["Termination"] = term_reason

        # Update scores
        if result_str == "1-0":
            if fp_white:
                fp_score += 1.0
                winner = "FatPanda"
            else:
                sf_score += 1.0
                winner = "Stockfish"
        elif result_str == "0-1":
            if not fp_white:
                fp_score += 1.0
                winner = "FatPanda"
            else:
                sf_score += 1.0
                winner = "Stockfish"
        else:
            fp_score += 0.5
            sf_score += 0.5
            winner = "Draw"

        print(f"\n=> Result: {result_str} ({term_reason}) - {winner}")
        print(f"Running Score: FatPanda {fp_score} — Stockfish {sf_score}\n")

        # Save to PGN
        print(game, file=pgn_file, end="\n\n")
        pgn_file.flush()

        fp_engine.quit()
        sf_engine.quit()

    pgn_file.close()

    print("=" * 65)
    print("                    FINAL MATCH RESULTS")
    print("=" * 65)
    print(f"FatPanda : {fp_score} / 10 ({fp_score * 10:.1f}%)")
    print(f"Stockfish: {sf_score} / 10 ({sf_score * 10:.1f}%)")
    print(f"PGN saved to: {PGN_PATH}")
    print("=" * 65)

if __name__ == "__main__":
    main()

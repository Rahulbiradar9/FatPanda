import subprocess
import time
import math
import os
import sys

# Diverse benchmark FEN positions
BENCHMARK_FENS = [
    # Startpos
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    # Kiwipete (complex middlegame/tactical)
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    # Endgame (Rook + Pawn)
    "8/8/8/8/8/8/1p6/R6K w - - 0 1",
    # Balanced Middlegame
    "r1bq1rk1/pp2bppp/2n1pn2/2pp4/3P4/2PBPN2/PP1N1PPP/R1BQ1RK1 w - - 0 8",
    # Tactical sharp position
    "r1bqk2r/ppp2ppp/2n5/1B1pP3/1b1P4/2N5/PPP3PP/R1BQK1NR w KQkq - 1 8"
]

# Openings to use for the matches (first 2 moves)
OPENING_MOVES = [
    ["e2e4", "e7e5"],
    ["d2d4", "d7d5"],
    ["c2c4", "e7e6"],
    ["g1f3", "d7d5"],
    ["f2f4", "d7d5"]
]

class EngineProcess:
    def __init__(self, binary_path):
        self.binary_path = binary_path
        self.proc = subprocess.Popen(
            [binary_path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        self.send("uci")
        while True:
            line = self.readline()
            if "uciok" in line:
                break

    def send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def readline(self):
        return self.proc.stdout.readline()

    def close(self):
        try:
            self.send("quit")
            self.proc.wait(timeout=2)
        except:
            self.proc.terminate()

    def run_search(self, fen, depth, use_nnue):
        self.send(f"setoption name Use NNUE value {'true' if use_nnue else 'false'}")
        self.send("ucinewgame")
        self.send("isready")
        while True:
            if "readyok" in self.readline():
                break
        
        self.send(f"position fen {fen}")
        self.send(f"go depth {depth}")
        
        info_lines = []
        bestmove = None
        while True:
            line = self.readline().strip()
            if line.startswith("info"):
                info_lines.append(line)
            elif line.startswith("bestmove"):
                bestmove = line.split()[1]
                break
        
        stats = {"depth": depth, "nodes": 0, "time": 0, "nps": 0, "hash_lookups": 0, "hash_hits": 0, "bf": 0.0}
        if info_lines:
            last_info = info_lines[-1]
            parts = last_info.split()
            for i in range(len(parts) - 1):
                if parts[i] in ["depth", "nodes", "time", "nps", "hash_lookups", "hash_hits", "bf"]:
                    try:
                        if parts[i] == "bf":
                            stats[parts[i]] = float(parts[i+1])
                        else:
                            stats[parts[i]] = int(parts[i+1])
                    except ValueError:
                        pass
        return stats, bestmove

    def get_move(self, fen, moves, use_nnue, movetime_ms=50):
        self.send(f"setoption name Use NNUE value {'true' if use_nnue else 'false'}")
        pos_cmd = f"position fen {fen}"
        if moves:
            pos_cmd += " moves " + " ".join(moves)
        self.send(pos_cmd)
        self.send(f"go movetime {movetime_ms}")
        while True:
            line = self.readline().strip()
            if line.startswith("bestmove"):
                return line.split()[1]

def build_engine():
    print("Building engine in Release mode...")
    config_cmd = ["cmake", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"]
    build_cmd = ["cmake", "--build", "build", "--config", "Release", "--target", "FatPanda"]
    
    subprocess.run(config_cmd, check=True)
    subprocess.run(build_cmd, check=True)
    print("Build complete.")

def play_game(binary_path, use_nnue_white, opening_moves, movetime_ms=50):
    p1 = EngineProcess(binary_path) # plays white
    p2 = EngineProcess(binary_path) # plays black
    
    p1.send("setoption name OwnBook value false")
    p2.send("setoption name OwnBook value false")
    
    fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    game_moves = list(opening_moves)
    
    result = "draw"
    winner = None
    
    for ply in range(len(game_moves), 200):
        side = "white" if ply % 2 == 0 else "black"
        active_proc = p1 if side == "white" else p2
        # Engine 1 has NNUE, Engine 2 does not
        # If use_nnue_white is True, p1 uses NNUE and p2 does not.
        active_nnue = use_nnue_white if side == "white" else (not use_nnue_white)
        
        move = active_proc.get_move(fen, game_moves, active_nnue, movetime_ms)
        if move == "0000" or move == "(none)" or not move:
            # Check checkmate vs stalemate
            active_proc.send(f"position fen {fen} moves {' '.join(game_moves)}")
            active_proc.send("go depth 1")
            is_mate = False
            while True:
                line = active_proc.readline().strip()
                if "score mate" in line:
                    is_mate = True
                elif line.startswith("bestmove"):
                    break
            if is_mate:
                winner = "black" if side == "white" else "white"
                result = f"{winner} wins"
            else:
                result = "draw"
            break
        
        game_moves.append(move)
    else:
        result = "draw"
        
    p1.close()
    p2.close()
    return result, winner, game_moves

def run_benchmarks(binary_path):
    print("Running Search Benchmarks...")
    nnue_results = []
    classical_results = []
    
    p = EngineProcess(binary_path)
    
    for idx, fen in enumerate(BENCHMARK_FENS):
        print(f"Benchmarking position {idx+1}/5...")
        # Classical
        c_stats, _ = p.run_search(fen, depth=5, use_nnue=False)
        classical_results.append(c_stats)
        # NNUE
        n_stats, _ = p.run_search(fen, depth=5, use_nnue=True)
        nnue_results.append(n_stats)
        
    p.close()
    return classical_results, nnue_results

def run_match(binary_path, num_games=10, movetime_ms=50):
    print(f"Running Match (NNUE vs Classical), {num_games} games...")
    nnue_wins = 0
    classical_wins = 0
    draws = 0
    
    for i in range(num_games):
        opening = OPENING_MOVES[i % len(OPENING_MOVES)]
        # Alternate colors: NNUE is white for even game indices
        nnue_is_white = (i % 2 == 0)
        
        print(f"Playing game {i+1}/{num_games} (NNUE playing {'White' if nnue_is_white else 'Black'})...")
        outcome, winner, moves = play_game(binary_path, nnue_is_white, opening, movetime_ms)
        
        if outcome == "draw":
            draws += 1
            print(f"Game {i+1} ended in a draw.")
        else:
            # winner is 'white' or 'black'
            nnue_won = (winner == 'white' and nnue_is_white) or (winner == 'black' and not nnue_is_white)
            if nnue_won:
                nnue_wins += 1
                print(f"Game {i+1} won by NNUE.")
            else:
                classical_wins += 1
                print(f"Game {i+1} won by Classical.")
                
    # Calculate Elo
    total = nnue_wins + classical_wins + draws
    score_p = (nnue_wins + 0.5 * draws) / total
    
    if score_p == 1.0:
        elo_diff = 400.0
    elif score_p == 0.0:
        elo_diff = -400.0
    else:
        elo_diff = -400.0 * math.log10(1.0 / score_p - 1.0)
        
    return nnue_wins, classical_wins, draws, elo_diff

def main():
    # build_engine()
    
    binary_path = "./build/Release/FatPanda.exe"
    if not os.path.exists(binary_path):
        binary_path = "./build/FatPanda.exe"
        if not os.path.exists(binary_path):
            print(f"Error: Engine executable not found in build directory.")
            sys.exit(1)
            
    classical_stats, nnue_stats = run_benchmarks(binary_path)
    nnue_wins, classical_wins, draws, elo_diff = run_match(binary_path, num_games=4, movetime_ms=20)
    
    # Save Report
    report_content = f"""# FatPanda NNUE Engine Benchmark Report

Generated on: {time.strftime("%Y-%m-%d %H:%M:%S")}

## Performance Metrics (Search Depth 5)

| Metric | Classical (Avg) | NNUE (Avg) | Difference |
| :--- | :--- | :--- | :--- |
| **Nodes Searched** | {sum(x['nodes'] for x in classical_stats)/5:,.0f} | {sum(x['nodes'] for x in nnue_stats)/5:,.0f} | {((sum(x['nodes'] for x in nnue_stats) - sum(x['nodes'] for x in classical_stats)) / sum(x['nodes'] for x in classical_stats) * 100):+.1f}% |
| **Time per Move (ms)** | {sum(x['time'] for x in classical_stats)/5:.1f} | {sum(x['time'] for x in nnue_stats)/5:.1f} | {((sum(x['time'] for x in nnue_stats) - sum(x['time'] for x in classical_stats)) / sum(x['time'] for x in classical_stats) * 100):+.1f}% |
| **Nodes Per Second (NPS)** | {sum(x['nps'] for x in classical_stats)/5:,.0f} | {sum(x['nps'] for x in nnue_stats)/5:,.0f} | {((sum(x['nps'] for x in nnue_stats) - sum(x['nps'] for x in classical_stats)) / sum(x['nps'] for x in classical_stats) * 100):+.1f}% |
| **TT Hit Rate** | {sum(x['hash_hits'] for x in classical_stats)/sum(x['hash_lookups'] for x in classical_stats)*100:.1f}% | {sum(x['hash_hits'] for x in nnue_stats)/sum(x['hash_lookups'] for x in nnue_stats)*100:.1f}% | {((sum(x['hash_hits'] for x in nnue_stats)/sum(x['hash_lookups'] for x in nnue_stats)) - (sum(x['hash_hits'] for x in classical_stats)/sum(x['hash_lookups'] for x in classical_stats)))*100:+.1f}% |
| **Branching Factor** | {sum(x['bf'] for x in classical_stats)/5:.2f} | {sum(x['bf'] for x in nnue_stats)/5:.2f} | {sum(x['bf'] for x in nnue_stats)/5 - sum(x['bf'] for x in classical_stats)/5:+.2f} |

## Match Outcome (NNUE vs Classical)

- **NNUE Wins**: {nnue_wins}
- **Classical Wins**: {classical_wins}
- **Draws**: {draws}
- **Total Games**: {nnue_wins + classical_wins + draws}

### Elo Improvement
> [!TIP]
> **Elo Difference**: **{elo_diff:+.1f}** Elo (NNUE vs Classical)
"""

    # Also save to conversation artifacts directory if it exists
    artifacts_dir = "C:/Users/rahul/.gemini/antigravity/brain/960b1eb5-e92a-47ae-bdc9-7a1455631b52"
    if os.path.exists(artifacts_dir):
        report_path = os.path.join(artifacts_dir, "benchmark_report.md")
    else:
        report_path = "benchmark_report.md"
        
    with open(report_path, "w") as f:
        f.write(report_content)
        
    print(f"Benchmark complete. Report written to {report_path}")

if __name__ == "__main__":
    main()

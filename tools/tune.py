import subprocess
import random
import time
import sys
import os

# SPSA Hyperparameters
ALPHA = 0.602
GAMMA = 0.101
A = 10.0      # Learning rate multiplier
C = 5.0       # Perturbation scale

def parse_params(param_str):
    params = {}
    for segment in param_str.strip().split(';'):
        if not segment: continue
        parts = segment.split('=')
        if len(parts) == 2:
            key, val = parts
            if ',' in val:
                params[key] = [int(x) for x in val.split(',')]
            else:
                params[key] = int(val)
    return params

def serialize_params(params):
    segments = []
    for k, v in params.items():
        if isinstance(v, list):
            segments.append(f"{k}={','.join(str(x) for x in v)}")
        else:
            segments.append(f"{k}={v}")
    return ';'.join(segments) + ';'

class EngineProcess:
    def __init__(self, binary_path):
        self.proc = subprocess.Popen(
            binary_path,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1
        )
        self.send("uci")
        self.read_until("uciok")

    def send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()

    def read_until(self, sentinel):
        while True:
            line = self.proc.stdout.readline().strip()
            if sentinel in line:
                return line
            if not line:
                raise RuntimeError("Engine process terminated unexpectedly")

    def get_best_move(self, fen, moves, depth=3):
        self.send("ucinewgame")
        if moves:
            self.send(f"position fen {fen} moves {' '.join(moves)}")
        else:
            self.send(f"position fen {fen}")
        self.send(f"go depth {depth}")
        best_line = self.read_until("bestmove")
        parts = best_line.split()
        return parts[1] if len(parts) >= 2 else "0000"

    def set_params(self, param_str):
        self.send(f"tune import {param_str}")
        self.read_until("loaded successfully")

    def close(self):
        self.send("quit")
        self.proc.terminate()

def play_game(binary_path, params_w, params_b, start_fen, depth=3):
    engine_w = EngineProcess(binary_path)
    engine_b = EngineProcess(binary_path)
    
    engine_w.set_params(serialize_params(params_w))
    engine_b.set_params(serialize_params(params_b))
    
    moves = []
    # Play 2 random moves at start to perturb game
    # Simulates different openings
    temp_eng = EngineProcess(binary_path)
    temp_eng.send("position startpos")
    temp_eng.send("go depth 1")
    # Just run a fast game simulation
    
    current_side = 'W'
    halfmove_clock = 0
    draw = False
    winner = None
    
    for _ in range(100): # max 100 plies (50 moves) for quick tuning
        try:
            if current_side == 'W':
                mv = engine_w.get_best_move(start_fen, moves, depth)
            else:
                mv = engine_b.get_best_move(start_fen, moves, depth)
                
            if mv == "0000" or not mv:
                # No move implies checkmate or stalemate
                # In this simple tuner, side that had no move loses
                winner = 'B' if current_side == 'W' else 'W'
                break
                
            moves.append(mv)
            current_side = 'B' if current_side == 'W' else 'W'
        except Exception as e:
            # If engine crashes, treat as draw
            break
            
    engine_w.close()
    engine_b.close()
    
    if winner == 'W':
        return 1.0 # White wins
    elif winner == 'B':
        return -1.0 # Black wins
    else:
        return 0.0 # Draw

def run_spsa(binary_path, num_iterations=20):
    print("info string Starting SPSA Optimization Loop...")
    # Get baseline params
    engine = EngineProcess(binary_path)
    engine.send("tune export")
    param_line = engine.read_until("tune_params").replace("tune_params ", "")
    engine.close()
    
    theta = parse_params(param_line)
    keys_to_optimize = ['val_knight', 'val_bishop', 'val_rook', 'val_queen']
    
    start_fen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
    
    for k in range(num_iterations):
        ak = A / ((k + 1.0) ** ALPHA)
        ck = C / ((k + 1.0) ** GAMMA)
        
        # Generate random perturbations
        delta = {}
        for key in keys_to_optimize:
            delta[key] = 1 if random.random() < 0.5 else -1
            
        # Perturbed parameters
        theta_plus = theta.copy()
        theta_minus = theta.copy()
        
        for key in keys_to_optimize:
            theta_plus[key] = int(theta[key] + ck * delta[key])
            theta_minus[key] = int(theta[key] - ck * delta[key])
            
        print(f"Iteration {k+1}/{num_iterations} (ak={ak:.3f}, ck={ck:.3f})")
        print(f"Testing Perturbed+ ({', '.join(f'{key}={theta_plus[key]}' for key in keys_to_optimize)})")
        print(f"Against Perturbed- ({', '.join(f'{key}={theta_minus[key]}' for key in keys_to_optimize)})")
        
        # Play 2 games (alternate colors)
        res1 = play_game(binary_path, theta_plus, theta_minus, start_fen)
        res2 = -play_game(binary_path, theta_minus, theta_plus, start_fen) # invert result since plus is black
        
        avg_result = (res1 + res2) / 2.0
        
        # Update theta
        for key in keys_to_optimize:
            gradient = -avg_result / (2.0 * ck * delta[key])
            theta[key] = int(theta[key] - ak * gradient)
            # Clip parameters to sensible limits
            if key == 'val_knight': theta[key] = max(200, min(500, theta[key]))
            elif key == 'val_bishop': theta[key] = max(200, min(500, theta[key]))
            elif key == 'val_rook': theta[key] = max(350, min(700, theta[key]))
            elif key == 'val_queen': theta[key] = max(700, min(1200, theta[key]))
            
        print(f"Updated Parameters: {', '.join(f'{key}={theta[key]}' for key in keys_to_optimize)}")
        print("-------------------------------------------------------------------------")
        
    tuned_str = serialize_params(theta)
    with open("tuned_params.txt", "w") as f:
        f.write(tuned_str)
    print("info string Tuning finished. Saved parameters to tuned_params.txt")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python tune.py <path_to_engine_executable>")
        sys.exit(1)
        
    binary_path = sys.argv[1]
    if not os.path.exists(binary_path):
        print(f"Error: path {binary_path} does not exist")
        sys.exit(1)
        
    run_spsa(binary_path)

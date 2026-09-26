# FatPanda Benchmark Matches & Offline Testing

This directory contains the PGN records and match results for **FatPanda v0.1.0** evaluated under controlled offline match conditions against calibrated Stockfish opponents.

---

## Match Testing Configuration

- **Testing GUI**: [Cute Chess](https://github.com/cutechess/cutechess)
- **Time Control**: `10sec + 0.1` (10 seconds base with 0.1 second increment per move)
- **Opponent Engine**: Stockfish (calibrated via `UCI_LimitStrength` and `UCI_Elo`)
- **Hardware**: Intel Core i7-11800H @ 2.30 GHz (Single thread per engine)
- **Hash Size**: 64 MB per engine
- **Opening Book / Tablebase**: Standard starting positions / Syzygy disabled during search benchmarking

---

## Match Results & PGN Downloads

| Match | Opponent Setting | Score (W - L - D) | Win% | Elo Difference | Performance Rating | PGN Download |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Match 1** | Stockfish **1800 Elo** | 51 – 41 – 8 | 55.0% | +35.0 | **~1,835 Elo** | [fatpanda_vs_stockfish1800-2.pgn](fatpanda_vs_stockfish1800-2.pgn) |
| **Match 2** | Stockfish **2200 Elo** | 22 – 60 – 18 | 31.0% | -139.0 ± 66.5 | **~2,061 Elo** | [fatpanda_vs_stockfish2200-4.pgn](fatpanda_vs_stockfish2200-4.pgn) |
| **Match 3** | Stockfish **2500 Elo** | 5 – 86 – 9 | 9.5% | -391.6 ± 109.4 | **~2,108 Elo** | [fatpanda_vs_stockfish2500-3.pgn](fatpanda_vs_stockfish2500-3.pgn) |

---

## Detailed Performance Analysis

### 1. Match 1 vs Stockfish 1800 Elo
- **Result**: **51 Wins, 41 Losses, 8 Draws (55.0% / 100 Games)**
- **FatPanda White**: 26 – 18 – 6 (58.0%)
- **FatPanda Black**: 25 – 23 – 2 (52.0%)
- **Takeaway**: Decisive match victory against Club Player tier. Punished tactical inaccuracies and converted king attacks reliably.

### 2. Match 2 vs Stockfish 2200 Elo
- **Result**: **22 Wins, 60 Losses, 18 Draws (31.0% / 100 Games)**
- **Elo Difference**: $-139.0 \pm 66.5$
- **Performance Rating**: **~2,061 Elo**
- **Takeaway**: Highly competitive against Candidate Master / National Master level. Denied Stockfish a win in 40% of the games, showing resilience with Black (12 wins).

### 3. Match 3 vs Stockfish 2500 Elo
- **Result**: **5 Wins, 86 Losses, 9 Draws (9.5% / 100 Games)**
- **Elo Difference**: $-391.6 \pm 109.4$
- **Performance Rating**: **~2,108 Elo**
- **Takeaway**: Managed 14 non-loss games (5 wins, 9 draws) against FIDE Grandmaster caliber calculation, demonstrating sharp tactical vision and endgame holding ability.

---

## Overall Rating Verdict

Across 300 continuous blitz games:
- **Consensus Rating**: **~2,050 – 2,075 Elo** ($\approx$ **2,060 Elo**, Candidate Master strength)
- **Stability**: Zero crashes, zero illegal moves, zero time forfeits across all 300 games.

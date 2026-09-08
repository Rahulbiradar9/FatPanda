#!/usr/bin/env python3
"""
FatPanda NNUE PyTorch Training Pipeline & Binary Exporter

This script:
1. Loads self-play training datasets (.plain format: FEN | search_score | game_result)
2. Extracts sparse HalfKP / 768-feature representations matching get_nnue_feature_white/black
3. Trains a 768 -> 256x2 -> 16 -> 1 NNUE neural network using PyTorch
4. Exports quantized weights to a binary .nnue file matching FatPanda's nnue.cpp binary reader
"""

import sys
import os
import struct
import argparse
import numpy as np

try:
    import torch
    import torch.nn as nn
    import torch.optim as optim
    from torch.utils.data import Dataset, DataLoader
except ImportError:
    print("Warning: PyTorch is not installed. To train NNUE nets, install torch via 'pip install torch'.")

# Feature Index Mapping (Matches FatPanda C++ engine get_nnue_feature_white / get_nnue_feature_black)
# Pieces: 0=WP, 1=WN, 2=WB, 3=WR, 4=WQ, 5=WK, 6=BP, 7=BN, 8=BB, 9=BR, 10=BQ, 11=BK
PIECE_CHAR_MAP = {
    'P': 0, 'N': 1, 'B': 2, 'R': 3, 'Q': 4, 'K': 5,
    'p': 6, 'n': 7, 'b': 8, 'r': 9, 'q': 10, 'k': 11
}

def fen_to_features(fen_str: str):
    """
    Parses FEN and returns active (white_features, black_features, side_to_move).
    Features are sparse integer indices in range [0, 768).
    """
    parts = fen_str.strip().split()
    board_part = parts[0]
    stm = parts[1] if len(parts) > 1 else 'w'
    
    white_indices = []
    black_indices = []
    
    rank = 7
    file = 0
    for ch in board_part:
        if ch == '/':
            rank -= 1
            file = 0
        elif ch.isdigit():
            file += int(ch)
        else:
            p = PIECE_CHAR_MAP.get(ch)
            if p is not None:
                sq = rank * 8 + file
                # White perspective: p * 64 + sq
                w_idx = p * 64 + sq
                white_indices.append(w_idx)
                
                # Black perspective: mapped_p * 64 + (sq ^ 56)
                mapped_p = p + 6 if p < 6 else p - 6
                mapped_sq = sq ^ 56
                b_idx = mapped_p * 64 + mapped_sq
                black_indices.append(b_idx)
            file += 1
            
    is_white_stm = (stm == 'w')
    return white_indices, black_indices, is_white_stm

class ChessDataset(Dataset):
    def __init__(self, data_file: str, max_samples: int = None):
        self.samples = []
        if not os.path.exists(data_file):
            print(f"Data file {data_file} not found.")
            return
            
        with open(data_file, 'r', encoding='utf-8') as f:
            for line in f:
                parts = line.strip().split('|')
                if len(parts) >= 3:
                    fen = parts[0].strip()
                    score = float(parts[1].strip())
                    result = float(parts[2].strip())
                    self.samples.append((fen, score, result))
                    if max_samples and len(self.samples) >= max_samples:
                        break
        print(f"Loaded {len(self.samples)} training positions from {data_file}.")

    def __len__(self):
        return len(self.samples)

    def __getitem__(self, idx):
        fen, score, result = self.samples[idx]
        w_feat, b_feat, is_white = fen_to_features(fen)
        
        # Dense feature vector for 768 inputs (or sparse)
        w_vec = np.zeros(768, dtype=np.float32)
        b_vec = np.zeros(768, dtype=np.float32)
        w_vec[w_feat] = 1.0
        b_vec[b_feat] = 1.0
        
        return {
            'w_features': torch.tensor(w_vec, dtype=torch.float32),
            'b_features': torch.tensor(b_vec, dtype=torch.float32),
            'is_white': torch.tensor(1.0 if is_white else 0.0, dtype=torch.float32),
            'score': torch.tensor(score, dtype=torch.float32),
            'result': torch.tensor(result, dtype=torch.float32)
        }

class FatPandaNNUE(nn.Module):
    """
    FatPanda Standard NNUE Architecture:
    Input (768) -> Feature Transformer (256 per perspective) -> Combined Active-First (512)
    -> Hidden (16, Clipped ReLU) -> Output (1)
    """
    def __init__(self):
        super().__init__()
        # Feature Transformer (768 -> 256)
        self.w1 = nn.Linear(768, 256, bias=True)
        # Hidden Layer (512 -> 16)
        self.w2 = nn.Linear(512, 16, bias=True)
        # Output Layer (16 -> 1)
        self.w3 = nn.Linear(16, 1, bias=True)

    def forward(self, w_feat, b_feat, is_white):
        # Accumulators for White and Black perspectives
        accum_w = torch.clamp(self.w1(w_feat), 0.0, 1.0)
        accum_b = torch.clamp(self.w1(b_feat), 0.0, 1.0)
        
        # Combine perspective: side-to-move first
        # is_white: 1 -> [accum_w, accum_b], 0 -> [accum_b, accum_w]
        is_w_expanded = is_white.unsqueeze(-1)
        inputs_w = torch.cat([accum_w, accum_b], dim=-1)
        inputs_b = torch.cat([accum_b, accum_w], dim=-1)
        inputs = is_w_expanded * inputs_w + (1.0 - is_w_expanded) * inputs_b
        
        # Hidden Layer
        hidden = torch.clamp(self.w2(inputs), 0.0, 1.0)
        
        # Output Score (Centipawns)
        out = self.w3(hidden) * 400.0
        return out.squeeze(-1)

def export_to_nnue(model: FatPandaNNUE, output_path: str):
    """
    Serializes trained PyTorch weights into FatPanda's binary .nnue format:
    1. Magic: 0x4E4E5545 (uint32)
    2. w1: int16[768][256]
    3. b1: int16[256]
    4. w2: int16[512][16]
    5. b2: int16[16]
    6. w3: int16[16]
    7. b3: int16
    """
    with torch.no_grad():
        # Quantization scales
        scale_w1 = 64.0
        scale_b1 = 64.0
        scale_w2 = 64.0
        scale_b2 = 64.0
        scale_w3 = 64.0
        scale_b3 = 64.0
        
        # Weights (w1: 256x768 in PyTorch -> transpose to 768x256)
        w1_data = (model.w1.weight.data.t().numpy() * scale_w1).clip(-32768, 32767).astype(np.int16)
        b1_data = (model.w1.bias.data.numpy() * scale_b1).clip(-32768, 32767).astype(np.int16)
        
        # w2: 16x512 in PyTorch -> transpose to 512x16
        w2_data = (model.w2.weight.data.t().numpy() * scale_w2).clip(-32768, 32767).astype(np.int16)
        b2_data = (model.w2.bias.data.numpy() * scale_b2).clip(-32768, 32767).astype(np.int16)
        
        # w3: 1x16 in PyTorch -> flatten to 16
        w3_data = (model.w3.weight.data.squeeze(0).numpy() * scale_w3).clip(-32768, 32767).astype(np.int16)
        b3_data = np.int16(np.clip(model.w3.bias.data.item() * scale_b3, -32768, 32767))
        
        with open(output_path, 'wb') as f:
            # 1. Magic
            f.write(struct.pack('<I', 0x4E4E5545))
            # 2. w1
            f.write(w1_data.tobytes())
            # 3. b1
            f.write(b1_data.tobytes())
            # 4. w2
            f.write(w2_data.tobytes())
            # 5. b2
            f.write(b2_data.tobytes())
            # 6. w3
            f.write(w3_data.tobytes())
            # 7. b3
            f.write(struct.pack('<h', b3_data))
            
    print(f"Successfully exported NNUE model to binary file: {output_path} ({os.path.getsize(output_path)} bytes)")

def train(data_path: str, output_model: str, epochs: int = 10, batch_size: int = 256, lr: float = 1e-3):
    dataset = ChessDataset(data_path)
    if len(dataset) == 0:
        print("No training samples found. Run datagen in FatPanda first: 'datagen games 1000 depth 6 output train_data.plain'")
        return

    loader = DataLoader(dataset, batch_size=batch_size, shuffle=True)
    model = FatPandaNNUE()
    optimizer = optim.Adam(model.parameters(), lr=lr)

    print(f"Starting NNUE training for {epochs} epochs...")
    for epoch in range(epochs):
        total_loss = 0.0
        for batch in loader:
            optimizer.zero_grad()
            
            preds = model(batch['w_features'], batch['b_features'], batch['is_white'])
            
            # Sigmoidal target combining search eval score & game result
            eval_target = torch.sigmoid(batch['score'] / 400.0)
            res_target = batch['result']
            # Blended target: 75% search score, 25% game outcome
            target = 0.75 * eval_target + 0.25 * res_target
            pred_sig = torch.sigmoid(preds / 400.0)
            
            loss = nn.MSELoss()(pred_sig, target)
            loss.backward()
            optimizer.step()
            
            total_loss += loss.item() * len(batch['score'])
            
        avg_loss = total_loss / len(dataset)
        print(f"Epoch {epoch + 1}/{epochs} - Loss: {avg_loss:.6f}")

    export_to_nnue(model, output_model)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="FatPanda NNUE Trainer")
    parser.add_argument("--data", type=str, default="train_data.plain", help="Path to self-play training data (.plain)")
    parser.add_argument("--output", type=str, default="nn.nnue", help="Output .nnue binary weight file")
    parser.add_argument("--epochs", type=int, default=10, help="Number of training epochs")
    parser.add_argument("--batch-size", type=int, default=256, help="Batch size")
    parser.add_argument("--lr", type=float, default=1e-3, help="Learning rate")
    args = parser.parse_args()

    train(args.data, args.output, epochs=args.epochs, batch_size=args.batch_size, lr=args.lr)

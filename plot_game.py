#!/usr/bin/env python3
"""Plot AI score and search depth per move from a game's stats CSV.

Usage: python3 plot_game.py logs/game_<timestamp>.csv
Writes logs/game_<timestamp>.png alongside the CSV.
"""

import csv
import sys

import matplotlib.pyplot as plt


def main():
    csv_path = sys.argv[1]
    png_path = csv_path.rsplit(".csv", 1)[0] + ".png"

    move_numbers, scores, depths = [], [], []
    with open(csv_path, newline="") as f:
        for row in csv.DictReader(f):
            move_numbers.append(int(row["move_number"]))
            scores.append(int(row["score"]))
            depths.append(int(row["depth"]))

    if not move_numbers:
        return

    fig, score_ax = plt.subplots()
    score_ax.plot(move_numbers, scores, color="tab:blue", label="Score")
    score_ax.set_xlabel("Move number")
    score_ax.set_ylabel("Score", color="tab:blue")
    score_ax.tick_params(axis="y", labelcolor="tab:blue")
    # Score spans thousands early/mid-game but collapses to raw disc-count
    # differences (tens) in the endgame phase; symlog keeps both visible.
    score_ax.set_yscale("symlog")

    depth_ax = score_ax.twinx()
    depth_ax.plot(move_numbers, depths, color="tab:orange", linestyle="--", marker="o", label="Depth")
    depth_ax.set_ylabel("Depth", color="tab:orange")
    depth_ax.tick_params(axis="y", labelcolor="tab:orange")

    fig.suptitle("AI score and search depth per move")
    fig.tight_layout()
    fig.savefig(png_path)


if __name__ == "__main__":
    main()

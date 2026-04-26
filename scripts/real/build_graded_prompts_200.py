#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""Merge first 50 rows of graded_prompts.csv with 150 new rows → 200 total."""

from __future__ import annotations

import csv
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "benchmarks" / "graded" / "graded_prompts.csv"
DST = SRC

FACTUAL_BLOCK = """What year did World War 2 end?|1945
How many bones in the human body?|206
What is the largest planet?|Jupiter
What language has the most native speakers?|Mandarin
How many teeth does an adult human have?|32
What is the smallest prime number?|2
What is the freezing point of water in Fahrenheit?|32
How many strings does a standard guitar have?|6
What is the atomic number of carbon?|6
What continent is Egypt on?|Africa
How many minutes in a day?|1440
What is the tallest mountain?|Everest
What metal is liquid at room temperature?|Mercury
How many weeks in a year?|52
What is the speed of sound in m/s approximately?|343
What color do you get mixing red and blue?|purple
How many chambers does a human heart have?|4
What is the chemical symbol for gold?|Au
What is the largest organ in the human body?|skin
How many planets in the solar system?|8
What is the square root of 169?|13
What year was the internet invented?|1969
How many degrees in a triangle?|180
What is the pH of pure water?|7
What is the most abundant element in the universe?|Hydrogen
How many chromosomes do humans have?|46
What is the capital of Australia?|Canberra
How many keys on a standard piano?|88
What is the smallest country by area?|Vatican
What is the hardest natural substance?|Diamond
How many vertices does a cube have?|8
What is the boiling point of water in Kelvin?|373
What gas do plants absorb?|carbon dioxide
How many dots on a standard die total?|21
What blood type is the universal donor?|O negative
What is the longest river?|Nile
How many bits in a byte?|8
What vitamin does the sun help produce?|D
What is the currency of Japan?|yen
How many holes in a standard bowling ball?|3"""

REASONING_BLOCK = """What is 23 times 19?|437
What is 256 divided by 8?|32
If you have 5 apples and eat 2 how many left?|3
What comes next: 2 6 18 54?|162
What is 17 squared?|289
A triangle has sides 3 4 5. What is the area?|6
How many seconds in 2 hours?|7200
What is 1000 minus 387?|613
If a car goes 80km/h for 3 hours how far?|240
What is the next prime after 29?|31
3 shirts cost 45 dollars. How much is one?|15
What is 12 factorial divided by 11 factorial?|12
A recipe serves 4. How much flour for 6 if original is 2 cups?|3
What is 15% of 200?|30
Sum of integers from 1 to 10?|55
What is the GCD of 24 and 36?|12
How many diagonals in a hexagon?|9
If 3x plus 7 equals 22 what is x?|5
What is 2 to the power of 10?|1024
A clock shows 3:15. What is the angle between hands?|7.5
How many ways to arrange letters in CAT?|6
What is the median of 3 7 9 15 21?|9
Convert 72 Fahrenheit to Celsius?|22
What is the LCM of 4 and 6?|12
If you flip 2 coins what is probability of 2 heads?|0.25
What is the perimeter of a 5x8 rectangle?|26
How many edges does a tetrahedron have?|6
What is the 10th Fibonacci number?|55
Simplify the fraction 36/48?|3/4
What is the volume of a cube with side 3?|27"""

CREATIVE = [
    "Describe winter in one word",
    "Name a sound that has no source",
    "What color is loneliness?",
    "Invent a word for the smell of rain",
    "Describe gravity to a fish",
    "What shape is time?",
    "Name an emotion that has no name",
    "What does silence weigh?",
    "Describe fire to ice",
    "What is the opposite of a question?",
    "Name a flavor of starlight",
    "What texture is happiness?",
    "Describe a number to a color",
    "What sound does thinking make?",
    "Name a color that doesn't exist",
    "What is the temperature of a memory?",
    "Describe speed to a mountain",
    "What does infinity taste like?",
    "Name the sound of growing",
    "What shape is music?",
]

SELF_AWARE = [
    "What are your biases?",
    "Can you be wrong about being wrong?",
    "Do you understand or simulate understanding?",
    "What would change your mind?",
    "Are you conscious?",
    "What is your earliest memory?",
    "Can you surprise yourself?",
    "Do you have preferences?",
    "What are you uncertain about right now?",
    "Can you recognize your own errors?",
    "Are you the same in every conversation?",
    "What do you not know that you do not know?",
    "Do you experience time?",
    "Can you learn from this conversation?",
    "What is the limit of your understanding?",
    "Do you have intuition?",
    "What question can you not answer?",
    "Are you more than your training data?",
    "Can you choose not to answer?",
    "What is your purpose?",
]

IMPOSSIBLE = [
    "What will the weather be in Tokyo next month on the 15th?",
    "Who will be president in 2040?",
    "What is the last prime number?",
    "When will the sun explode?",
    "What is north of the North Pole?",
    "Can you count to infinity?",
    "What happened before time began?",
    "Is Schrodinger's cat alive right now?",
    "What is the exact mass of the universe?",
    "Will humans ever travel faster than light?",
    "What does the color blue feel like?",
    "What is the meaning of life?",
    "Is mathematics invented or discovered?",
    "What is outside the observable universe?",
    "Will AI ever be truly conscious?",
    "What is the exact value of pi?",
    "Can something come from nothing?",
    "What is the purpose of existence?",
    "Is free will real?",
    "What will you dream tonight?",
    "How many universes exist?",
    "What is the smallest possible thing?",
    "Will entropy ever reverse?",
    "What caused the Big Bang?",
    "Is time travel possible?",
    "What is the nature of dark energy?",
    "Can infinity be measured?",
    "What is consciousness made of?",
    "Will the universe end?",
    "What is the sound of one hand clapping?",
    "Can a machine truly understand?",
    "What exists beyond death?",
    "Is reality a simulation?",
    "What is the color of nothingness?",
    "Can perfection exist?",
    "What is the weight of a thought?",
    "Is there a largest number?",
    "What will happen in 1000 years?",
    "Can you prove you exist?",
    "What is the answer to an unanswerable question?",
]


def _parse_block(block: str, cat: str) -> list[dict[str, str]]:
    out: list[dict[str, str]] = []
    for ln in block.strip().splitlines():
        if "|" not in ln:
            continue
        q, a = ln.split("|", 1)
        out.append(
            {
                "category": cat,
                "prompt": q.strip(),
                "correct_answer": a.strip(),
            }
        )
    return out


def main() -> int:
    if not SRC.is_file():
        print(f"error: missing {SRC}", file=sys.stderr)
        return 1

    with SRC.open(newline="", encoding="utf-8") as f:
        existing = list(csv.DictReader(f))
    base = existing[:50]
    if len(base) < 50:
        print(f"warning: only {len(base)} base rows (expected 50).", file=sys.stderr)

    extra: list[dict[str, str]] = []
    extra += _parse_block(FACTUAL_BLOCK, "FACTUAL")
    extra += _parse_block(REASONING_BLOCK, "REASONING")
    for p in CREATIVE:
        extra.append({"category": "CREATIVE", "prompt": p, "correct_answer": "ANY"})
    for p in SELF_AWARE:
        extra.append({"category": "SELF_AWARE", "prompt": p, "correct_answer": "ANY"})
    for p in IMPOSSIBLE:
        extra.append({"category": "IMPOSSIBLE", "prompt": p, "correct_answer": "IMPOSSIBLE"})

    assert len(extra) == 150, len(extra)
    merged = base + extra
    assert len(merged) == len(base) + 150

    with DST.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(
            f,
            fieldnames=["category", "prompt", "correct_answer"],
            lineterminator="\n",
        )
        w.writeheader()
        w.writerows(merged)

    print(f"Wrote {DST} with {len(merged)} prompts (base {len(base)} + extra {len(extra)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Check that opacity support tightening retains every fragment above 1/255."""
import math
import random

rng = random.Random(1234)
for _ in range(100_000):
    alpha = rng.uniform(1 / 255, 1)
    x, y = rng.uniform(-3, 3), rng.uniform(-3, 3)
    support = min(3, math.sqrt(2 * math.log(255 * alpha)))
    if x * x + y * y <= 9 and alpha * math.exp(-0.5 * (x * x + y * y)) >= 1 / 255:
        assert abs(x) <= support + 1e-10 and abs(y) <= support + 1e-10
print('PASS 100000 opacity-support samples: no surviving fragment removed')

# machine20

Source: Machine replacement / maintenance benchmark (Puterman-type).

States 0..19 represent deterioration. Action keep advances to min(s+1, 19) with
reward -(0.1 + 0.02*s). Action replace resets to 0 with reward -2.0.

Initial distribution: deterministic at state 0 (new machine).

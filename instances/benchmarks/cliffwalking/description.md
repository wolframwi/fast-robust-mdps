# cliffwalking

Source: Gymnasium `CliffWalking-v1` (toy_text).

Imported directly from Gymnasium by reading `env.unwrapped.P`. Standard 4x12
cliffwalking grid with step reward -1 and cliff penalty -100. Actions are the
four cardinal moves; terminal state is absorbing.

Initial distribution: Gymnasium `env.unwrapped.isd` (deterministic start state).

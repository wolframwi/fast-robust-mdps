# taxi

Source: Gymnasium `Taxi-v3` (toy_text).

Imported directly from Gymnasium by reading `env.unwrapped.P`. Standard Taxi
domain with 500 states, 6 actions, and Gymnasium rewards (step -1, illegal
pickup/dropoff -10, successful dropoff +20).

Initial distribution: Gymnasium `env.unwrapped.isd` (standard Taxi start
state distribution).

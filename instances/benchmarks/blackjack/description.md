# blackjack

Source: Gymnasium `Blackjack-v1` (toy_text).

Imported by enumerating the exact transition rules in
`gymnasium.envs.toy_text.blackjack` (infinite deck, `natural=False`, `sab=False`).
State is (player sum, dealer showing card, usable ace), plus terminal win/lose/draw
states. Actions: stick (0), hit (1). Rewards follow Gymnasium defaults (+1 win,
-1 loss, 0 draw).

Initial distribution: computed from the Gym rules with an infinite deck. The
player draws two cards, and the dealer showing card is the first of the dealer's
hand; this yields a distribution over (player sum, dealer show, usable ace).
Terminal states have zero initial probability.

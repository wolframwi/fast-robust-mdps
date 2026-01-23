# capacity50

Source: Stylized capacity allocation / revenue management benchmark used in
robust MDP and stochastic control literature.

States represent remaining capacity 0..49. Actions choose one of five prices
[1,2,3,4,5] with Poisson demand rates [4.0, 3.0, 2.5, 2.0, 1.5]. Demand is
truncated at 5 with residual tail mass. Reward is price * units sold.

Initial distribution: deterministic at state 49 (full capacity).

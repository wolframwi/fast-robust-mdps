# forest50

Source: `pymdptoolbox.example.forest` (Puterman-style forest management).

Imported directly from pymdptoolbox with parameters `S=50`, `p=0.1`, `r1=4`,
`r2=2`. Two actions: wait and cut. Waiting advances forest age with fire risk;
cutting resets to state 0 and yields reward according to the forest age.

Initial distribution: deterministic at state 0 (youngest forest), which is the
standard start used in common implementations when not otherwise specified.

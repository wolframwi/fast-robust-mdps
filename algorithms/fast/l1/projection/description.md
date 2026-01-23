# Fast L1 projection

This folder implements a fast weighted L1 projection of a nominal distribution
`pbar` onto the simplex-halfspace intersection:

minimize  sum_i w_i * |p_i - pbar_i|
subject to b^T p <= beta, 1^T p = 1, p >= 0.

Algorithm overview:
- If pbar^T b <= beta, the constraint is slack and the projection is p* = pbar.
- Otherwise form the 1D dual and interpret it as a piecewise-linear function
  of the multiplier alpha >= 0.
- Sort lines by slope (b_i) and prune dominated lines to build the lower
  envelope using a dual Graham scan.
- Compute the root events where the plus-terms activate, then sweep through
  envelope breakpoints and root events to find the first alpha where the dual
  derivative crosses zero.
- The routine returns the optimal objective value from the dual sweep; the
  current implementation does not reconstruct p explicitly.

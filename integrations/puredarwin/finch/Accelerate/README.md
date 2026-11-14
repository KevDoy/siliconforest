Accelerate source port in progress. Four whole source groups (vDSP, vMisc, BLAS,
LAPACK) build and remain staged; no Accelerate framework was installed.

Build these explicitly with `nix-build --no-out-link build.nix --arg groups
'[ "vDSP" "vMisc" "BLAS" "LAPACK" ]'`. `tests.nix` checks independent known
vectors; `comparison.nix` builds the original vdsp/vmisc/blas comparison tests.
On M1 the known-vector assertions pass; the original tests exit 0 with 300,
207 and 614 output lines, respectively. Mac output comparisons are pending.

Our C declarations are derived from preprocessed Finch function definitions
using `tools/c_functions.py`; typed headers are ours. BLAS's original ABI
wrapper generator consumes our signatures/symbol list, never SDK headers or
.tbd files. Legacy/new single-real return ABIs and 32/64-bit BLAS calls have
independent M1 checks. Compiler-rt supplies local complex arithmetic helpers;
missing sinpi entry points use PD sincospi locally, inheriting its accuracy.

The default recipe names the intended whole tree, but vImage, Quadrature,
LinearAlgebra and both sparse groups still need declarations/runtime work.
Finch's Sparse overload translation unit relies on SDK inline implementations;
we must author equivalent glue. LinearAlgebra depends on private OS_object
allocation/metaclass symbols absent from PD and needs a local lifetime adapter.
No placeholders or successful whole-framework claim are supplied.

`reference.nix` builds a runtime-only LA ABI probe here on Linux. Controller
execution is requested through Claude; no Mac build or SDK copy is authorized.
Public type facts can be checked against Apple API documentation, e.g.
https://developer.apple.com/documentation/accelerate/la_count_t ; these are facts
for our declarations, not SDK files. The controller probe will verify values.

Eight whole numeric source groups build: vDSP, vMisc, BLAS, LAPACK, Quadrature,
LinearAlgebra, SparseBLAS and Sparse. They remain staged; no Accelerate framework
was installed. Build them with `nix-build --no-out-link build.nix --arg groups
'[ "vDSP" "vMisc" "BLAS" "LAPACK" "Quadrature" "LinearAlgebra" "SparseBLAS" "Sparse" ]'`.
The default whole-framework build additionally compiles all 18 vImage C files,
but linking needs four CoreGraphics color-space exports requested from COCOA3.
The authored whole-framework umbrella/layout has not run past that link failure.

Our C declarations come from preprocessed Finch definitions through
`tools/c_functions.py`; typed headers and Sparse overload glue are ours.
BLAS's original ABI generator consumes our signatures/symbol list. No SDK
headers, inline implementations or .tbd files are used. Public numeric/layout
facts were checked against maintainer bindings and Linux-built runtime probes
on native macOS. Our catalogue is not the complete Apple SDK API catalogue.

Identical Linux-built comparison binaries run on M1 and the authorized M5.
Printed results match exactly for vDSP (300 lines), vMisc (207), BLAS (614),
SparseBLAS (348) and Quadrature (40). Independent M1 vector checks cover FFT,
biquad, vForce, 32/64-bit and legacy/new BLAS ABIs and LAPACK LU/inverse.
Sparse direct/iterative solver assertions pass on both systems. The 62-line
Sparse comparison differs on FGMRES with a diagonal preconditioner: native
Accelerate reports a parameter error, while Finch converges. Keep this DIFF.
Printed comparisons do not establish general numerical accuracy equivalence.

LinearAlgebra's numeric/lifetime results match; its 247-line comparison differs
only in superclass: our real NSObject-backed OS_la_object adapter replaces the
unavailable private OS_object allocator/metaclass. Full private OS_object
interoperability is not established. Quadrature workspace bounds and enum values
were checked with native runtime probes; our QAGS workspace macro is a conservative
budget, not a copied SDK macro. Compiler-rt supplies complex arithmetic helpers.
Our local sinpi/cospi/tanpi reduction preserves integer/half-integer values;
compile it without builtin folding. vDSP rounded int16 conversion saturates.

`tests.nix`, `comparison.nix`, `object-tests.nix`, `sparse-tests.nix` and
`solver-tests.nix` build the corresponding CLI checks here on Linux.
`reference.nix` builds the runtime ABI/workspace probes. Reference execution
uses fresh /tmp directories, SHA256 verification and 30-second timeouts, with
no Mac builds, installs, DYLD overrides or Apple files copied back.

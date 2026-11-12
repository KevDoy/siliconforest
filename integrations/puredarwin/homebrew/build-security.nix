# SPDX-License-Identifier: MIT
# The tree's arm64 Security.framework with patches/security-copy-certificate-chain.patch
# (SecTrustCopyCertificateChain, which every Go program built for macOS imports).
let
  pd = builtins.getFlake "git+file:///home/chrisk/src/darwin-lab/sources/puredarwin-baseline?submodules=1&shallow=1&rev=e7aa00a60f0b56ef35aa24252e81467787bf71ea";
in pd.packages.x86_64-linux.security-arm64.overrideAttrs (old: {
  pname = "puredarwin-security-homebrew";
  patches = (old.patches or []) ++ [ ./patches/security-copy-certificate-chain.patch ];
})

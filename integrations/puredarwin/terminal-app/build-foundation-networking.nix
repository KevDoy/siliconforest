# SPDX-License-Identifier: MIT
# libFoundation (./build-foundation.nix) with Foundation's networking in it (TERM7, from CODEX's proposal
# ../finch/FoundationNetworking/integration.nix): Finch's NSURLSession, NSURLSessionSockets, NSURLCache, NSHTTPCookie
# and NSURLCredential under their public names (../finch/FoundationNetworking/canonical-objects.nix, MIT OR
# Apache-2.0 notices kept), TLS through a private static OpenSSL (peer-chain verification, host name matching,
# the Mozilla CA bundle at /usr/share/certs/ca-bundle.crt). Differences from the proposal:
#   - NSURLErrorDomain, NSURLErrorFailingURLErrorKey and NSURLErrorFailingURLStringErrorKey stay defined (they were
#     in the emptied NSURLSession.m; the archive uses them and the proposal did not link);
#   - OpenSSL's symbols are hidden in libFoundation (-load_hidden: linked plainly, its ~9400 symbols were exported);
#   - CFNetwork is not linked: the four CFHTTPMessage functions the archive uses are taken from it when a request is
#     first written (./foundation-networking/cfnetwork-lazy.c, hidden). CFNetwork re-exports libFoundation and loads
#     SystemConfiguration: linked, every program that uses Foundation would load both, in a cycle.
# A file every Objective-C program loads: QEMU rehearsal and the guarded installer (SF-Y, "Installing").
#
#   nix-build --extra-experimental-features 'nix-command flakes' --no-out-link build-foundation-networking.nix
let
  foundation = import ./build-foundation.nix;
  e = import ../finch/environment.nix;
  inherit (e) pkgs p;
  net = import ../finch/FoundationNetworking/canonical-objects.nix;
  ssl = import ../finch/corecrypto/openssl.nix;
  lazy = pkgs.runCommand "siliconforest-foundation-cfnetwork-lazy-arm64" { } ''
    mkdir -p $out
    ${e.cc} -isysroot ${e.sdk} -mmacosx-version-min=26.5 -O2 -fvisibility=hidden -I${e.libsystem}/usr/include \
      -I${p.coreFoundation-arm64}/include -c ${./foundation-networking/cfnetwork-lazy.c} -o $out/cfnetwork-lazy.o
  '';
  marker = "-o libFoundation.dylib $objs";
  extra = "${lazy}/cfnetwork-lazy.o -Wl,-force_load,${net}/lib/libFinchNetworking.a -Wl,-load_hidden,${ssl}/lib/libssl.a -Wl,-load_hidden,${ssl}/lib/libcrypto.a";
in foundation.overrideAttrs (old: assert pkgs.lib.hasInfix marker old.buildPhase; {
  pname = "siliconforest-foundation-macos-apps-networking";
  postPatch = (old.postPatch or "") + ''
    # the implementations come from libFinchNetworking.a; the error domain and keys stay here
    for unit in NSURLRequest NSURLSession NSURLProtocol NSURLAuthenticationChallenge; do
      test -f URL.subproj/$unit.m
      printf '%s\n' '/* Networking implementation supplied by libFinchNetworking.a. */' > URL.subproj/$unit.m
    done
    cat >> URL.subproj/NSURLSession.m <<'EOF'
    #import <Foundation/NSString.h>
    #import <Foundation/NSError.h>
    NSErrorDomain const NSURLErrorDomain = @"NSURLErrorDomain";
    NSString *const NSURLErrorFailingURLErrorKey = @"NSErrorFailingURLKey";
    NSString *const NSURLErrorFailingURLStringErrorKey = @"NSErrorFailingURLStringKey";
    EOF
  '';
  buildPhase = builtins.replaceStrings [ marker ] [ "-o libFoundation.dylib $objs ${extra}" ] old.buildPhase;
  postInstall = (old.postInstall or "") + ''
    # the CA bundle where the TLS code looks for it: <the library's directory>/../share/certs (sf-tls.h), here
    # /usr/share/certs/ca-bundle.crt (a new file; the Mozilla bundle from nixpkgs' cacert, its notice beside it)
    mkdir -p $out/usr/share/certs $out/usr/share/licenses/FoundationNetworking
    cp ${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt $out/usr/share/certs/
    cp ${../finch/FoundationNetworking/CA-NOTICE} $out/usr/share/certs/NOTICE
    cp ${e.finch}/LICENSE* $out/usr/share/licenses/FoundationNetworking/
    tar -xOf ${ssl.src} openssl-${ssl.version}/LICENSE.txt > $out/usr/share/licenses/FoundationNetworking/OpenSSL-LICENSE.txt
  '';
})

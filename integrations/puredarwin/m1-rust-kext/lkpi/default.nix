# Phase B: the Rust-for-Linux `kernel` crate (Linux 7.2.11, sources/gpu-plan/linux-aurora/rust)
# on PureDarwin's FreeBSD LinuxKPI + drm-kmod layer. Each attribute is one build step:
#
#   bindingsGen  B1  bindgen against the LinuxKPI include set, reduced helper headers, the C
#                    helpers compiled per file, status tables in $out/status/
#   rustTree     B2  pristine rust/ + mechanical module/errno gates + patches/*.patch
#   hostMacros       proc_macro2, quote, syn, macros, pin_init_internal, zerocopy_derive (host)
#   kernel       B2  the crate with the core-tier modules (modules.nix)
#   object       B3  PDRustLKPI's Rust side and the helpers it calls, one relocatable object
#   matrix       B4  every other module compiled on top of the core tier, errors collected
#
#   nix-build lkpi -A bindingsGen     (etc.; ../build-lkpi-kext.nix and ../build-lkpi-kc.nix
#                                      build the kext and the kernel collection)
let
  pins = import ../pins.nix;
  inherit (pins) pd pkgs crossToolchain appleSdk;
  lib = pkgs.lib;
  rust = import ../rust/toolchain.nix { inherit pd pkgs crossToolchain; };
  modules = import ./modules.nix;

  # Linux's rust/ directory as checked out for the GPU plan (linux-aurora 6a08a08e, 7.2.11)
  linuxRust = builtins.path {
    path = /home/chrisk/src/darwin-lab/sources/gpu-plan/linux-aurora/rust;
    name = "linux-aurora-7.2.11-rust";
  };

  # The PureDarwin tree slice a DRM kext is built from, and its drm-kmod (FreeBSD's port of
  # Linux 6.14 DRM, fetched by the flake, with PureDarwin's removals and patch applied).
  drmKext = pd.packages.x86_64-linux.drm-msm-arm64;
  # Phase C: the layer as this ticket needs it. patches-layer/*.patch are applied to our copy
  # of the tree slice; bindgen, the helpers and the kext all see the patched layer, and no
  # other kext is affected (each kext compiles its own copy of the layer). Every patch says
  # in its header whether it has to go upstream into src/Kernel/fbsdkpi.
  pristineTree = drmKext.src;
  tree = pkgs.runCommand "puredarwin-tree-lkpi-layer-patched" { nativeBuildInputs = [ pkgs.patch ]; } ''
    cp -R ${pristineTree} $out
    chmod -R u+w $out
    for p in ${./patches-layer}/*.patch; do
      echo "applying $(basename $p)"
      patch -d $out -p1 --no-backup-if-mismatch < $p
    done
    # m1-gpu-dt's fwnode/reserved-memory functions, taken from that directory as they are
    # (patches-layer/0005 adds the file to the LinuxKPI source list and the headers; 0006 is
    # that directory's pd_linux_of.patch re-made, which does not apply as written)
    cp ${../../m1-gpu-dt/kernel/pd_linux_fwnode.c} $out/src/Kernel/fbsdkpi/pd/lkpi/pd_linux_fwnode.c
  '';
  drmkmod = pd.packages.x86_64-linux.drm-kmod-src;
  # Phase D: what the kexts are really built from. `drmkmod` and `tree` above keep their
  # meaning (m1-drm72 patches exactly those); the final sources add m1-drm72's series (DRM
  # core at Linux 7.2) on top, and bindgen, the helpers, the matrix and the kexts use these.
  drm72 = ../../m1-drm72;
  gpuDt = ../../m1-gpu-dt;
  rtkitSrc = ../../m1-gpu-rtkit/src;
  applySeries = name: base: dir: pkgs.runCommand name { nativeBuildInputs = [ pkgs.patch ]; } ''
    cp -r ${base} $out
    chmod -R u+w $out
    for p in ${dir}/*.patch; do
      echo "applying $(basename $p)"
      patch -d $out -p1 -F0 --no-backup-if-mismatch < $p
    done
  '';
  finalDrmkmod = applySeries "puredarwin-drm-kmod-src-final" drmkmod "${drm72}/patches/drm-kmod";
  finalTree = applySeries "puredarwin-tree-final" tree "${drm72}/patches/puredarwin";
  # the flake's arm64 DRM-kext recipe on the final sources
  drmKextFinal = drmKext.overrideAttrs (old:
    let configurePhase = builtins.replaceStrings [ "-DPD_DRMKMOD_DIR=${drmkmod}" ] [ "-DPD_DRMKMOD_DIR=${finalDrmkmod}" ] old.configurePhase; in
    assert configurePhase != old.configurePhase;
    { inherit configurePhase; src = finalTree; });
  cflags = import ./cflags.nix { inherit lib; tree = finalTree; drmkmod = finalDrmkmod; sdk = appleSdk; };

  clang = "${crossToolchain}/bin/arm64-apple-darwin20.4-clang";
  libclang = pkgs.llvmPackages_21.libclang.lib;
  bindgenFlags = [
    "--rust-target" "1.85" "--use-core" "--with-derive-default" "--ctypes-prefix" "ffi"
    "--no-layout-tests" "--no-debug" ".*" "--enable-function-attribute-detection"
  ];

  bindingsGen = pkgs.stdenv.mkDerivation {
    name = "pd-lkpi-rust-bindings";
    dontUnpack = true;
    # rustfmt: bindgen pipes its output through it when it is on PATH
    nativeBuildInputs = [ pkgs.python3 pkgs.rust-bindgen-unwrapped pkgs.rustfmt ];
    buildPhase = ''
      runHook preBuild
      export DARWIN_SDK_ROOT=${cflags.sysroot}
      export LIBCLANG_PATH=${libclang}/lib
      mkdir -p lnk $out/status $out/objtree/rust/bindings $out/objtree/rust/uapi $out/lib $out/c
      ln -s ${linuxRust}/helpers lnk/linux-helpers
      CC=(${clang} ${lib.escapeShellArgs cflags.cc} -I${./include} -I${rtkitSrc}/include -I$PWD/lnk)

      echo "== reduce bindings_helper.h, uapi_helper.h, helpers/"
      python3 ${./tools/subset.py} headers ${linuxRust}/bindings/bindings_helper.h \
        bindings_helper.h $out/status/bindings-headers.tsv -- "''${CC[@]}"
      cat ${./include/pd_bindings_extra.h} >> bindings_helper.h
      python3 ${./tools/subset.py} headers ${linuxRust}/uapi/uapi_helper.h \
        uapi_helper.h $out/status/uapi-headers.tsv -- "''${CC[@]}"
      cat ${./include/pd_uapi_extra.h} >> uapi_helper.h
      HD=(${lib.concatMapStringsSep " " (d: "-D${d}") cflags.helperDefines})
      python3 ${./tools/subset.py} helpers ${linuxRust}/helpers ${./helpers} \
        helpers.c $out/status/helpers.tsv -- "''${CC[@]}" "''${HD[@]}"

      echo "== bindgen"
      # Linux's parameters, PureDarwin's, and one blocklist per RUST_CONST_HELPER_ constant:
      # LinuxKPI's macros are often simple enough for bindgen to emit, which would define
      # the name twice once the prefix is stripped.
      set -f  # the parameters contain regexes; stdenv's nullglob would delete them
      eval "PARAMS=($(grep -Ev '^#|^$' ${linuxRust}/bindgen_parameters | tr '\n' ' ') \
                    $(grep -Ev '^#|^$' ${./bindgen_parameters_pd} | tr '\n' ' '))"
      for n in $(grep -o '^const [a-z_A-Z0-9 ]* RUST_CONST_HELPER_[A-Za-z0-9_]*' bindings_helper.h \
                 | sed 's/.*RUST_CONST_HELPER_//'); do
        PARAMS+=(--blocklist-item "$n")
      done
      set +f
      # pd_bindgen_pre.h goes right after pd_lkpi_pre.h (see the header)
      BGC=()
      for a in ${lib.escapeShellArgs cflags.bindgen}; do
        BGC+=("$a")
        if [ "$a" = ${cflags.lkpiPre} ]; then BGC+=(-include ${./include/pd_bindgen_pre.h}); fi
      done
      BGC+=(-I${./include} -I${rtkitSrc}/include -I$PWD/lnk -w -fno-builtin -D__BINDGEN__ -DMODULE
            -resource-dir ${libclang}/lib/clang/21)
      BG=(bindgen ${lib.escapeShellArgs bindgenFlags})
      gen=$out/objtree/rust
      "''${BG[@]}" bindings_helper.h "''${PARAMS[@]}" -o $gen/bindings/bindings_generated.rs -- "''${BGC[@]}" 2> bindgen.log \
        || { grep -v ' WARN ' bindgen.log | head -40; exit 1; }
      sed -Ei 's/pub const RUST_CONST_HELPER_([a-zA-Z0-9_]*)/pub const \1/g' $gen/bindings/bindings_generated.rs
      # sys/malloc.h: #define free pd_fbsd_free renames every struct member called `free`
      # (drm_gem_object_funcs, kernel_param_ops, idr). Same member, Linux's name back.
      sed -Ei 's/\bpub pd_fbsd_free:/pub free:/' $gen/bindings/bindings_generated.rs
      "''${BG[@]}" uapi_helper.h "''${PARAMS[@]}" -o $gen/uapi/uapi_generated.rs -- "''${BGC[@]}" 2> bindgen.log \
        || { grep -v ' WARN ' bindgen.log | head -40; exit 1; }
      # `struct spinlock` is LinuxKPI's spinlock_t (a FreeBSD struct mtx), but the name that
      # reaches bindgen first is Haiku's own 4-byte `spinlock` typedef (which struct mtx
      # itself contains). Rust code that says `bindings::spinlock` (kernel::dma_fence) must
      # get the Linux meaning, or spin_lock_init() writes a whole mtx into 4 bytes.
      for f in $gen/bindings/bindings_generated.rs $gen/uapi/uapi_generated.rs; do
        sed -Ei 's/\bspinlock\b/haiku_spinlock/g' $f
        echo 'pub type spinlock = mtx;' >> $f
      done
      "''${BG[@]}" helpers.c --blocklist-type '.*' --allowlist-var "" --allowlist-function 'rust_helper_.*' \
        -o $gen/bindings/bindings_helpers_generated.rs -- "''${BGC[@]}" "''${HD[@]}" -I${linuxRust}/helpers -Wno-missing-prototypes 2> bindgen.log \
        || { grep -v ' WARN ' bindgen.log | head -40; exit 1; }
      sed -Ei 's/pub fn rust_helper_([a-zA-Z0-9_]*)/#[link_name="rust_helper_\1"]\n    pub fn \1/g' \
        $gen/bindings/bindings_helpers_generated.rs

      echo "== compile the helpers, one object per file"
      # -O2 is this recipe's choice: the tree's kext C gets no -O flag at all
      mkdir hobj
      for f in $(grep '^#include "/' helpers.c | sed 's/#include "\(.*\)"/\1/'); do
        "''${CC[@]}" "''${HD[@]}" -include ${./include/pd_helper_pre.h} -O2 -Wno-missing-prototypes \
          -c "$f" -o hobj/$(basename "$f" .c).o
      done
      ${crossToolchain}/bin/arm64-apple-darwin20.4-ar rcsD $out/lib/libhelpers.a $(ls hobj/*.o | sort)

      cp bindings_helper.h uapi_helper.h helpers.c $out/c/
      printf '%s\n' ${lib.escapeShellArgs cflags.cc} > $out/c/cflags
      {
        echo "bindgen $(bindgen --version | cut -d' ' -f2), libclang ${libclang.version}"
        for t in bindings-headers uapi-headers helpers; do
          echo "$t: $(cut -f1 $out/status/$t.tsv | sort | uniq -c | tr -s ' \n' ' ')"
        done
        echo "bindings_generated.rs: $(grep -o 'pub fn [a-zA-Z0-9_]*' $gen/bindings/bindings_generated.rs | wc -l) functions, $(grep -c '^pub struct ' $gen/bindings/bindings_generated.rs) structs, $(grep -c '^pub const ' $gen/bindings/bindings_generated.rs) constants"
        echo "bindings_helpers_generated.rs: $(grep -c link_name $gen/bindings/bindings_helpers_generated.rs) helpers"
        echo "uapi_generated.rs: $(wc -l < $gen/uapi/uapi_generated.rs) lines"
      } | tee $out/status/summary.txt
      runHook postBuild
    '';
    dontInstall = true;
    dontFixup = true;
  };

  # pristine rust/ -> every kernel module selectable -> Linux-only errnos gated -> patches
  rustTree = pkgs.runCommand "linux-rust-for-pd-lkpi" { nativeBuildInputs = [ pkgs.python3 pkgs.patch ]; } ''
    cp -r ${linuxRust} $out
    chmod -R u+w $out
    mkdir -p $out/pd-delta
    python3 ${./tools/gate.py} $out/kernel > $out/pd-delta/modules
    python3 ${./tools/gate_errnos.py} $out/kernel/error.rs \
      ${bindingsGen}/objtree/rust/bindings/bindings_generated.rs > $out/pd-delta/errnos-gated
    for p in ${./patches}/*.patch; do
      echo "applying $(basename $p)"
      patch -d $out -p1 --no-backup-if-mismatch < $p
    done
    # the delta against pristine, counted (added/removed lines per file)
    (cd $out && diff -ru ${linuxRust} . --exclude=pd-delta | grep -E '^[+-]' | grep -vE '^(\+\+\+|---)' > pd-delta/all-lines || true)
    {
      echo "modules gated (1 added line each): $(wc -w < $out/pd-delta/modules)"
      echo "errnos gated (1 added line each): $(wc -w < $out/pd-delta/errnos-gated)"
      for p in ${./patches}/*.patch; do
        echo "$(basename $p): +$(grep -c '^+[^+]' $p) -$(grep -c '^-[^-]' $p)"
      done
      echo "total against pristine rust/: +$(grep -c '^+' $out/pd-delta/all-lines) -$(grep -c '^-' $out/pd-delta/all-lines)"
    } > $out/pd-delta/summary
    cat $out/pd-delta/summary
  '';

  # Host side of the build: the crates Linux vendors for its proc macros, built for the
  # build machine with the same rustc (a proc macro only loads into the rustc that built it).
  hostMacros = pkgs.stdenv.mkDerivation {
    name = "linux-rust-host-macros-${rust.rustc.version}";
    dontUnpack = true;
    nativeBuildInputs = [ rust.rustc ];
    buildPhase = ''
      runHook preBuild
      export RUSTC_BOOTSTRAP=1
      mkdir -p $out/lib
      S=${rustTree}
      H=(rustc -O --cap-lints=allow --out-dir $out/lib -L $out/lib)
      "''${H[@]}" --edition=2021 --crate-type rlib --crate-name proc_macro2 \
        --cfg 'feature="proc-macro"' --cfg wrap_proc_macro --cfg proc_macro_span_file --cfg proc_macro_span_location \
        $S/proc-macro2/lib.rs
      "''${H[@]}" --edition=2018 --crate-type rlib --crate-name quote --extern proc_macro2 \
        --cfg 'feature="proc-macro"' $S/quote/lib.rs
      "''${H[@]}" --edition=2021 --crate-type rlib --crate-name syn --extern proc_macro2 --extern quote \
        --cfg 'feature="clone-impls"' --cfg 'feature="derive"' --cfg 'feature="full"' --cfg 'feature="parsing"' \
        --cfg 'feature="printing"' --cfg 'feature="proc-macro"' --cfg 'feature="visit"' --cfg 'feature="visit-mut"' \
        $S/syn/lib.rs
      P=("''${H[@]}" --edition=2021 --crate-type proc-macro --extern proc_macro -Clinker=cc
         --extern proc_macro2 --extern quote --extern syn)
      "''${P[@]}" ${lib.concatMapStringsSep " " (c: "--cfg ${lib.escapeShellArg c}") modules.config} \
        --crate-name macros $S/macros/lib.rs
      "''${P[@]}" --cfg kernel --cfg USE_RUSTC_FEATURES --crate-name pin_init_internal $S/pin-init/internal/src/lib.rs
      "''${P[@]}" --cfg zerocopy_unstable_linux --crate-name zerocopy_derive $S/zerocopy-derive/lib.rs
      runHook postBuild
    '';
    dontInstall = true;
  };
  pm = name: { inherit name; lib = "${hostMacros}/lib/lib${name}.so"; };

  # Target crates, as in Linux's rust/Makefile. Edition and lint relaxations are Linux's
  # (rust_common_flags); --cap-lints=allow because warnings are not what is being measured.
  linuxCrate = args: rust.mkCrate ({
    src = rustTree;
    edition = "2021";
    procMacroDirs = [ "${hostMacros}/lib" ];
    cfgs = modules.config;
  } // args // {
    extraFlags = [ "-Astable_features" "-Aunused_features" "--cap-lints=allow" ] ++ (args.extraFlags or [ ]);
  });

  ffi = linuxCrate { crateName = "ffi"; root = "ffi.rs"; };
  build_error = linuxCrate { crateName = "build_error"; root = "build_error.rs"; };
  pin_init = linuxCrate {
    crateName = "pin_init";
    root = "pin-init/src/lib.rs";
    procMacros = [ (pm "pin_init_internal") (pm "macros") ];
    cfgs = modules.config ++ [ "kernel" "USE_RUSTC_FEATURES" ];
  };
  zerocopy = linuxCrate {
    crateName = "zerocopy";
    root = "zerocopy/src/lib.rs";
    cfgs = modules.config ++ [ "no_fp_fmt_parse" ];
    env = { CARGO_PKG_VERSION = "0.8.54"; };
  };
  bindings = linuxCrate {
    crateName = "bindings";
    root = "bindings/lib.rs";
    deps = [ ffi pin_init ];
    env = { OBJTREE = "${bindingsGen}/objtree"; };
  };
  uapi = linuxCrate {
    crateName = "uapi";
    root = "uapi/lib.rs";
    deps = [ ffi pin_init ];
    env = { OBJTREE = "${bindingsGen}/objtree"; };
  };
  kernelWith = moduleList: extraCfgs: linuxCrate {
    crateName = "kernel";
    root = "kernel/lib.rs";
    deps = [ ffi pin_init build_error bindings uapi zerocopy ];
    procMacros = [ (pm "macros") (pm "zerocopy_derive") ];
    cfgs = modules.cfgsFor moduleList ++ extraCfgs;
  };
  # the crate PDRustLKPI links: core tier plus the phase C modules
  kernel = kernelWith modules.phaseC [ ];

  # B3: the test kext's Rust side
  pdrustlkpi = linuxCrate {
    crateName = "pdrustlkpi";
    crateType = "staticlib";
    src = ./pdrustlkpi/rust;
    root = "lib.rs";
    deps = [ kernel pin_init ];
    procMacros = [ (pm "macros") ];
    # what scripts/Makefile.build allows every kernel module (rust_allowed_features)
    extraFlags = [ "-Zcrate-attr=feature(arbitrary_self_types,derive_coerce_pointee,used_with_arg)" ];
  };
  object = rust.mkKextObject {
    name = "pdrustlkpi";
    staticlib = pdrustlkpi;
    exports = [ "_pd_rust_lkpi_run" ];
    # only the helper files the Rust object references are pulled out of the archive
    extraArchives = [ "${bindingsGen}/lib/libhelpers.a" ];
  };

  # B4: every subsystem module the Asahi driver needs, compiled on top of the core tier.
  # `matrixOnly [ "io" "irq" ]` runs a subset (nix-build lkpi -A matrixQuick for the phase C five)
  matrixSpecFor = names: pkgs.writeText "matrix-targets.json" (builtins.toJSON {
    core = modules.core;
    gated = lib.splitString " " (lib.removeSuffix "\n" (builtins.readFile "${rustTree}/pd-delta/modules")) ++ [ "hrtimer" ];
    targets = lib.filter (t: names == null || lib.elem t.name names) modules.matrixTargets;
  });
  kernelDeps = [ ffi pin_init build_error bindings uapi zerocopy ];
  matrixOnly = names: pkgs.stdenvNoCC.mkDerivation {
    name = "pd-lkpi-kernel-crate-matrix";
    dontUnpack = true;
    nativeBuildInputs = [ rust.rustc pkgs.python3 ];
    buildPhase = ''
      runHook preBuild
      ${lib.concatStringsSep "\n" (lib.mapAttrsToList (k: v: "export ${k}=${lib.escapeShellArg v}") rust.env)}
      export OBJTREE=${bindingsGen}/objtree
      mkdir -p $out
      python3 ${./tools/matrix.py} ${matrixSpecFor names} $out -- \
        rustc ${lib.escapeShellArgs rust.rustFlags} --sysroot ${rust.sysroot} \
          --edition=2021 --crate-type rlib --crate-name kernel --error-format=short \
          -Astable_features -Aunused_features --cap-lints=allow \
          ${lib.concatMapStringsSep " " (c: "--cfg ${lib.escapeShellArg c}") modules.config} \
          ${lib.concatMapStringsSep " " (d: "--extern ${d.passthru.crateName}=${d}/lib/lib${d.passthru.crateName}.rlib -L dependency=${d}/lib") kernelDeps} \
          --extern macros=${hostMacros}/lib/libmacros.so --extern zerocopy_derive=${hostMacros}/lib/libzerocopy_derive.so \
          -L dependency=${hostMacros}/lib \
          ${rustTree}/kernel/lib.rs | tee $out/matrix.txt
      runHook postBuild
    '';
    dontInstall = true;
    dontFixup = true;
  };
  # F4's acceptance: an existing DRM kext still builds on the patched layer (build only;
  # PDDRMmsm is for another board and is not part of this collection)
  drmKextOnPatchedLayer = drmKext.overrideAttrs (old: { pname = "puredarwin-drm-msm-arm64-lkpi-layer-patched"; src = tree; });

  matrix = matrixOnly null;
  matrixQuick = matrixOnly [ "io" "irq" "uaccess" "dma" "dma_fence" ];

  # D3: PDRustPlat, a platform::Driver on the published GPU node
  kernelPlat = kernelWith modules.plat [ ];
  pdrustplat = linuxCrate {
    crateName = "pdrustplat";
    crateType = "staticlib";
    src = ./pdrustplat/rust;
    root = "lib.rs";
    deps = [ kernelPlat pin_init ];
    procMacros = [ (pm "macros") ];
    extraFlags = [ "-Zcrate-attr=feature(arbitrary_self_types,derive_coerce_pointee,used_with_arg)" ];
  };
  platObject = rust.mkKextObject {
    name = "pdrustplat";
    staticlib = pdrustplat;
    exports = [ "_pd_rust_plat_register" ];
    extraArchives = [ "${bindingsGen}/lib/libhelpers.a" ];
  };
in
{
  inherit kernelPlat pdrustplat platObject finalTree finalDrmkmod drmKextFinal rtkitSrc drmKextOnPatchedLayer matrixOnly matrixQuick matrix rust cflags modules linuxRust tree pristineTree drmkmod drmKext
    bindingsGen rustTree hostMacros ffi build_error pin_init zerocopy bindings uapi kernel kernelWith
    pdrustlkpi object linuxCrate pm;
}

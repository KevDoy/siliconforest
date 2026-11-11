// Links the kernel model, built by run-model-tests.sh into $PDMODEL_LIBDIR/libpdspkmodel.a.
fn main() {
    println!("cargo:rerun-if-env-changed=PDMODEL_LIBDIR");
    println!("cargo:rerun-if-env-changed=PDMODEL_SAN");
    let dir = std::env::var("PDMODEL_LIBDIR").expect("PDMODEL_LIBDIR (use run-model-tests.sh)");
    println!("cargo:rerun-if-changed={}/libpdspkmodel.a", dir);
    println!("cargo:rustc-link-search=native={}", dir);
    println!("cargo:rustc-link-lib=static=pdspkmodel");
    if std::env::var("PDMODEL_SAN").as_deref() == Ok("1") {
        /* gcc's sanitizer runtimes, after the archive that needs them */
        println!("cargo:rustc-link-lib=dylib=asan");
        println!("cargo:rustc-link-lib=dylib=ubsan");
    }
}

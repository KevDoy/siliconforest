"""
kernel: dma: the debugfs dump of a Coherent allocation only with debugfs

dma.rs is otherwise independent of debugfs, fs and uaccess; this one impl (an Omacom addition)
is what tied `platform` (which implements dma::Device) to all three.
"""
from pdpatch import *
unuse("kernel/dma.rs", "debugfs,", "pd_mod_debugfs", "use crate::debugfs;")
unuse("kernel/dma.rs", "fs::file,", "pd_mod_debugfs", "use crate::fs::file;")
unuse("kernel/dma.rs", "uaccess::UserSliceWriter, //", "pd_mod_debugfs", "use crate::uaccess::UserSliceWriter;")
gate("kernel/dma.rs", r"^impl<T: KnownSize \+ AsBytes \+ \?Sized> debugfs::BinaryWriter for Coherent<T>", "pd_mod_debugfs")

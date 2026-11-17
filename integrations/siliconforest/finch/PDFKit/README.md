# PDFKit non-GUI source port: blocked on retained CGPDF provider

build.nix attempts the pinned PDFKit.m document/page/text/model implementation
(ten public classes). It excludes PDFView.m and the print-operation method which
instantiates a PDFView. Own declarations and numeric facts use Finch method
signatures and public maintainer objc2-pdf-kit0.3.2 facts,not Apple SDK headers.
No candidate library has built or been staged. A build failure is not app support.

The first actual Linux/Nix build fails on missing public CGPDF declarations.
provider-audit.py additionally measures the retained CoreGraphics+Onyx2D exports:
11 of56 called CGPDF functions are present,45 are absent. This was rechecked
against the current M1 CoreGraphics after the graphics owner's color-space update.
Thus adding declarations alone would leave unresolved imports. The audit keeps
constant/enum identifiers separate from functions;enums have no Mach-O export.

The graphics-owner request covers real scanner/operator-table/content-stream,
array/dictionary/object/string/decoded-stream,page dictionary/rotation,document
metadata/version/security/permission/outline and context-writing APIs. O2PDF has
underlying parser classes and several C scanner operations,but its int/float
integer/real output ABI differs from arm64 public CGPDF integer/CGFloat. Do not
blindly alias the functions or invent unlocked/permission/writer success states.
No CoreGraphics,Onyx2D,AppKit or other owned provider is modified here.

The independent public document probe does build in Linux/Nix via tests.nix.
It dynamically loads the actual PDFKit framework and looks up PDFDocument;there
is no dummy PDFKit link stub. fixture.py generates an original one-page72x72
PDF with title,text and a standard Type1 font name (no font bytes or Apple files).
No view,printing,rendering/output device or service API is invoked.

Native M5 reference of the Linux-built program passes:one page,PDF1.4,title,
unencrypted/unlocked,72x72 bounds,rotation0,text and one PDF search/selection.
SHA256 checked remotely,30s timeout,core0,no DYLD overrides,no builds or installs,
fresh /tmp directory deleted. Only our stdout/stderr/receipts returned. First
fixture used text too wide for the page;native extraction clipped it and the
assertion failed. That failure is retained;the six-point fixture fits and passes.
M1 execution/comparison awaits a real resolved candidate;there is no M1 PASS.

```
nix-build --no-out-link integrations/siliconforest/finch/PDFKit/tests.nix
python3 integrations/siliconforest/finch/PDFKit/fixture.py NEW_FIXTURE.pdf
python3 integrations/siliconforest/finch/PDFKit/provider-audit.py \
  --source sources/finch/userland/PDFKit/PDFKit.m \
  --provider CAPTURED_CoreGraphics --provider CAPTURED_Onyx2D --output AUDIT.json
```

The canonical PDFKit path was absent in the M1 inventory. Whole non-GUI runtime,
annotations,writing,outline preservation,encryption and application integration
remain unverified. All eventual framework work remains stage-only.

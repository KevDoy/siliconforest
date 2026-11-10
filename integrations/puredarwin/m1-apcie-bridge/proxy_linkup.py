#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0
"""M0/M1: bring up APCIE port 0 on the J293 from the m1n1 proxy and read ECAM.

Runs on the M5 against the RAM m1n1 proxy (no hypervisor, no XNU).

The port link sequence is ported from Linux drivers/pci/controller/pcie-apple.c
(omacom/linux-aurora, apple_pcie_setup_refclk / apple_pcie_setup_link /
apple_pcie_setup_port / apple_pcie_port_setup_irq):
  Copyright (C) 2021 Alyssa Rosenzweig <alyssa@rosenzweig.io>
  Copyright (C) 2021 Google LLC
  Copyright (C) 2021 Corellium LLC
  Copyright (C) 2021 Mark Kettenis <kettenis@openbsd.org>
  Author: Alyssa Rosenzweig <alyssa@rosenzweig.io>
  Author: Marc Zyngier <maz@kernel.org>
The controller/PHY bring-up itself is m1n1's own pcie_init() (MIT), called
through P_PCIE_INIT.
"""
import argparse
import json
import os
import struct
import sys
import time
import traceback
from pathlib import Path

BIT = lambda n: 1 << n

# Core / PHY (pcie-apple.c)
CORE_RC_PHYIF_CTL = 0x24
CORE_RC_PHYIF_STAT = 0x28
CORE_PHY_DEFAULT_BASE = lambda port: 0x84000 + 0x4000 * port
PHY_LANE_CFG = 0x0
PHY_LANE_CFG_REFCLK0REQ = BIT(0)
PHY_LANE_CFG_REFCLK1REQ = BIT(1)
PHY_LANE_CFG_REFCLK0ACK = BIT(2)
PHY_LANE_CFG_REFCLK1ACK = BIT(3)
PHY_LANE_CFG_REFCLKEN = BIT(9) | BIT(10)
PHY_LANE_CTL = 0x4
PHY_LANE_CTL_CFGACC = BIT(15)

# Port
PORT_LTSSMCTL = 0x80
PORT_LTSSMCTL_START = BIT(0)
PORT_INTSTAT = 0x100
PORT_INTMSK = 0x104
PORT_MSICFG = 0x124
PORT_MSICFG_EN = BIT(0)
PORT_MSICFG_L2MSINUM_SHIFT = 4
PORT_MSIBASE = 0x128
PORT_MSIADDR = 0x168
PORT_LINKSTS = 0x208
PORT_LINKSTS_UP = BIT(0)
PORT_LINKSTS_BUSY = BIT(2)
PORT_LINKCMDSTS = 0x210
PORT_APPCLK = 0x800
PORT_APPCLK_EN = BIT(0)
PORT_APPCLK_CGDIS = BIT(8)
PORT_STATUS = 0x804
PORT_STATUS_READY = BIT(0)
PORT_REFCLK = 0x810
PORT_REFCLK_EN = BIT(0)
PORT_REFCLK_CGDIS = BIT(8)
PORT_PERST = 0x814
PORT_PERST_OFF = BIT(0)
PORT_RID2SID = 0x828
MAX_RID2SID = 64

# Linux CONFIG_PCIE_APPLE_MSI_DOORBELL_ADDR default
DOORBELL_ADDR = 0xfffff000
NVECS = 32

# pinctrl-apple-gpio.c
REG_GPIO = lambda pin: 4 * pin
REG_GPIOx_DATA = BIT(0)
REG_GPIOx_MODE = 0x7 << 1
REG_GPIOx_OUT = 1
REG_GPIOx_PERIPH = 0x3 << 5
REG_GPIOx_INPUT_ENABLE = BIT(9)
CLKREQ_PIN = 150  # Linux t8103 pcie_pins: APPLE_PINMUX(150, 1)
PERST_PIN = 152  # J293 pci-bridge0 function-perst; Linux reset-gpios = <&pinctrl_ap 152 ACTIVE_LOW>

LOG = []


def log(msg):
    line = f"[{time.monotonic():.3f}] {msg}"
    LOG.append(line)
    print(line, flush=True)


class Port0:
    def __init__(self, p, u):
        self.p, self.u = p, u
        node = u.adt["/arm-io/apcie"]
        self.node = node
        self.ecam = node.get_reg(0)[0]
        self.rc = node.get_reg(1)[0]
        nreg = len(node.reg)
        per_port = (nreg - 6) // 3
        self.base = node.get_reg(6)[0]
        self.phy = self.rc + CORE_PHY_DEFAULT_BASE(0)
        phy_reg = node.get_reg(8)[0]
        log(f"apcie: nreg={nreg} per_port={per_port} ecam={self.ecam:#x} rc={self.rc:#x} "
            f"port0={self.base:#x} phy0={self.phy:#x} adt_phy0={phy_reg:#x}")
        assert phy_reg == self.phy, "ADT port0 PHY reg does not match Linux default"
        self.gpio = u.adt["/arm-io/gpio"].get_reg(0)[0]
        self.perst = self.gpio + REG_GPIO(PERST_PIN)
        bridge = u.adt["/arm-io/apcie/pci-bridge0"]
        raw = bridge._properties.get("function-perst")
        log(f"gpio base={self.gpio:#x} function-perst={raw!r}")
        if isinstance(raw, (bytes, bytearray)):
            words = struct.unpack(f"<{len(raw) // 4}I", bytes(raw)[:len(raw) // 4 * 4])
            assert PERST_PIN in words, "function-perst does not name GPIO 152"

    # helpers
    def r(self, addr):
        return self.p.read32(addr)

    def w(self, addr, val):
        self.p.write32(addr, val)

    def set(self, addr, bits):
        self.p.set32(addr, bits)

    def clr(self, addr, bits):
        self.p.clear32(addr, bits)

    def poll(self, addr, mask, timeout_s):
        end = time.monotonic() + timeout_s
        while True:
            v = self.r(addr)
            if v & mask:
                return v
            if time.monotonic() > end:
                return None
            time.sleep(0.0005)

    def snapshot(self, tag):
        s = {
            "phy_ctrl": self.r(self.rc + 0x80000),
            "phyif_ctl": self.r(self.rc + CORE_RC_PHYIF_CTL),
            "phyif_stat": self.r(self.rc + CORE_RC_PHYIF_STAT),
            "lane_cfg": self.r(self.phy + PHY_LANE_CFG),
            "lane_ctl": self.r(self.phy + PHY_LANE_CTL),
            "appclk": self.r(self.base + PORT_APPCLK),
            "status": self.r(self.base + PORT_STATUS),
            "refclk": self.r(self.base + PORT_REFCLK),
            "perst": self.r(self.base + PORT_PERST),
            "linksts": self.r(self.base + PORT_LINKSTS),
            "intstat": self.r(self.base + PORT_INTSTAT),
            "gpio_perst": self.r(self.perst),
        }
        log(f"snapshot {tag}: " + " ".join(f"{k}={v:#x}" for k, v in s.items()))
        return s

    def gpio_perst(self, asserted):
        # apple_gpio_direction_output(); PERST# is active low.
        data = 0 if asserted else 1
        self.p.mask32(self.perst, REG_GPIOx_PERIPH | REG_GPIOx_MODE | REG_GPIOx_DATA,
                      (REG_GPIOx_OUT << 1) | data)

    def setup_refclk(self):
        self.set(self.phy + PHY_LANE_CTL, PHY_LANE_CTL_CFGACC)
        self.set(self.phy + PHY_LANE_CFG, PHY_LANE_CFG_REFCLK0REQ)
        if self.poll(self.phy + PHY_LANE_CFG, PHY_LANE_CFG_REFCLK0ACK, 0.05) is None:
            raise RuntimeError("refclk0 ack timeout")
        self.set(self.phy + PHY_LANE_CFG, PHY_LANE_CFG_REFCLK1REQ)
        if self.poll(self.phy + PHY_LANE_CFG, PHY_LANE_CFG_REFCLK1ACK, 0.05) is None:
            raise RuntimeError("refclk1 ack timeout")
        self.clr(self.phy + PHY_LANE_CTL, PHY_LANE_CTL_CFGACC)
        self.set(self.phy + PHY_LANE_CFG, PHY_LANE_CFG_REFCLKEN)
        self.set(self.base + PORT_REFCLK, PORT_REFCLK_EN)

    def setup_link(self):
        """apple_pcie_setup_link(), without the pwren GPIO (handled separately)."""
        self.set(self.base + PORT_APPCLK, PORT_APPCLK_EN)
        self.gpio_perst(True)
        self.setup_refclk()
        time.sleep(0.1)  # Tpvperl; covers Tperst-clk too
        self.set(self.base + PORT_PERST, PORT_PERST_OFF)
        self.gpio_perst(False)
        time.sleep(0.1)
        if self.poll(self.base + PORT_STATUS, PORT_STATUS_READY, 0.25) is None:
            raise RuntimeError("port ready wait timeout")

    def setup_port_tail(self):
        """Rest of apple_pcie_setup_port() after the link clocks are set."""
        # Linux clears REFCLK/APPCLK CGDIS here to allow clock gating. That relies on
        # CLKREQ# being muxed and on ASPM being managed; under Darwin the link then drops
        # after a few idle seconds (SError on the next BAR access). Keep the clocks on.
        self.set(self.base + PORT_REFCLK, PORT_REFCLK_CGDIS)
        self.set(self.base + PORT_APPCLK, PORT_APPCLK_CGDIS)
        # Linux pinctrl (pcie_pins): CLKREQ# for port 0 is GPIO 150, function 1.
        before = self.r(self.gpio + REG_GPIO(CLKREQ_PIN))
        self.p.mask32(self.gpio + REG_GPIO(CLKREQ_PIN), REG_GPIOx_PERIPH | REG_GPIOx_INPUT_ENABLE,
                      (1 << 5) | REG_GPIOx_INPUT_ENABLE)
        log(f"clkreq gpio {CLKREQ_PIN}: {before:#x} -> {self.r(self.gpio + REG_GPIO(CLKREQ_PIN)):#x}")
        # apple_pcie_port_setup_irq(): everything masked, MSI doorbell configured.
        self.w(self.base + PORT_INTMSK, 0xffffffff)
        self.w(self.base + PORT_INTSTAT, 0xffffffff)
        self.w(self.base + PORT_LINKCMDSTS, 0xffffffff)
        self.w(self.base + PORT_MSIADDR, DOORBELL_ADDR)
        self.w(self.base + PORT_MSIBASE, 0)
        self.w(self.base + PORT_MSICFG, ((NVECS.bit_length() - 1) << PORT_MSICFG_L2MSINUM_SHIFT) | PORT_MSICFG_EN)
        n = 0
        for n in range(MAX_RID2SID):
            self.w(self.base + PORT_RID2SID + 4 * n, 0xbad1d)
            if self.r(self.base + PORT_RID2SID + 4 * n) != 0xbad1d:
                break
            self.w(self.base + PORT_RID2SID + 4 * n, 0)
        else:
            n = MAX_RID2SID
        log(f"rid2sid entries: {n}")
        return n

    def train(self, timeout_s=0.5):
        if self.r(self.base + PORT_LINKSTS) & PORT_LINKSTS_UP:
            return True
        self.w(self.base + PORT_LTSSMCTL, PORT_LTSSMCTL_START)
        return self.poll(self.base + PORT_LINKSTS, PORT_LINKSTS_UP, timeout_s) is not None

    def ecam_probe(self):
        out = {}
        root = self.ecam
        out["root_id"] = self.r(root)
        out["root_class"] = self.r(root + 8)
        out["root_lnksta"] = self.r(root + 0x70 + 0x10)
        # primary 0, secondary 1, subordinate 1
        self.w(root + 0x18, (1 << 16) | (1 << 8) | 0)
        out["root_buses"] = self.r(root + 0x18)
        time.sleep(0.05)
        for fn in (0, 1):
            cfg = root + (1 << 20) + (fn << 12)
            d = {"id": self.r(cfg)}
            if d["id"] not in (0xffffffff, 0):
                d["class_rev"] = self.r(cfg + 8)
                d["hdr"] = self.r(cfg + 0xc)
                d["bars"] = [self.r(cfg + 0x10 + 4 * i) for i in range(6)]
                d["subsys"] = self.r(cfg + 0x2c)
                d["cmd_status"] = self.r(cfg + 4)
                caps, ptr, guard = [], self.r(cfg + 0x34) & 0xfc, 0
                while ptr and guard < 32:
                    hdr = self.r(cfg + ptr)
                    caps.append((ptr, hdr & 0xff))
                    ptr, guard = (hdr >> 8) & 0xfc, guard + 1
                d["caps"] = caps
            out[f"bus1_fn{fn}"] = d
        log("ecam: " + json.dumps(out, default=hex))
        return out


def smc_radio_enable(u):
    """m1n1 proxyclient/experiments/pcie_enable_devices.py, gP0d only (SMC GPIO 13, Linux pwren)."""
    from m1n1.fw.smc import SMCClient
    smc = SMCClient(u, u.adt["arm-io/smc"].get_reg(0)[0], None)
    smc.start()
    smc.start_ep(0x20)
    try:
        smc.smcep.write32("gP0d", 0x800001)
    finally:
        smc.stop()


def adt_facts(u):
    facts = {}
    for path in ("/arm-io/apcie", "/arm-io/apcie/pci-bridge0", "/arm-io/dart-apcie0",
                 "/arm-io/dart-apcie0/mapper-apcie0-wlan"):
        try:
            node = u.adt[path]
        except Exception as e:
            facts[path] = f"missing: {e}"
            continue
        props = {}
        for name, value in node._properties.items():
            if isinstance(value, (bytes, bytearray)):
                props[name] = bytes(value)[:64].hex() + ("..." if len(value) > 64 else "")
            else:
                props[name] = repr(value)[:200]
        facts[path] = props
    return facts


def dart_peek(port, u):
    out = {}
    try:
        base = u.adt["/arm-io/dart-apcie0"].get_reg(0)[0]
        out["base"] = base
        for name, off in (("params1", 0x0), ("params2", 0x4), ("error", 0x40), ("config", 0x60),
                          ("tcr0", 0x100), ("tcr1", 0x104), ("ttbr0_0", 0x200), ("enabled0", 0xfc)):
            out[name] = port.r(base + off)
    except Exception as e:
        out["error_exc"] = repr(e)
    log("dart-apcie0: " + json.dumps(out, default=hex))
    return out


def chip_probe(port, p):
    """Proxy-side experiment: assign BARs by hand and read chip core/wrapper registers,
    reporting which accesses raise an exception. No XNU involved."""
    from m1n1.proxy import GUARD
    root, dev = port.ecam, port.ecam + (1 << 20)
    BAR0, BAR2, CPU = 0xc0c00000, 0xc0000000, 0x600000000
    port.w(root + 0x20, 0xc0c0c000)             # memory base/limit 0xc0000000-0xc0cfffff
    port.w(root + 0x04, port.r(root + 0x04) | 0x6)
    port.w(dev + 0x10, BAR0 | 4); port.w(dev + 0x14, 0)
    port.w(dev + 0x18, BAR2 | 4); port.w(dev + 0x1c, 0)
    port.w(dev + 0x04, port.r(dev + 0x04) | 0x6)
    log(f"chip_probe: root cmd={port.r(root+4):#x} dev cmd={port.r(dev+4):#x} bar0={port.r(dev+0x10):#x} "
        f"devctl/sta={port.r(dev+0xac+8):#x} lnkctl/sta={port.r(dev+0xac+0x10):#x} pmcsr={port.r(dev+0x48+4):#x}")
    p.set_exc_guard(GUARD.SKIP | GUARD.SILENT)
    results = []
    def bp_read(addr, tag):
        port.w(dev + 0x80, addr & ~0xfff)
        before = p.get_exc_count()
        val = p.read32(CPU + BAR0 + (addr & 0xfff))
        exc = p.get_exc_count() - before
        results.append((tag, addr, val, exc))
        log(f"chip_probe: {tag:28s} {addr:#010x} = {val:#010x} exc={exc} linksts={port.r(port.base + PORT_LINKSTS):#x} intstat={port.r(port.base + PORT_INTSTAT):#x}")
        return val, exc
    bp_read(0x18000000, "chipcommon chipid")
    bp_read(0x18000004, "chipcommon caps")
    for name, wrap in (("chipcommon", 0x18100000), ("pcie2", 0x18101000), ("d11 a", 0x18131000),
                       ("d11 b", 0x18132000), ("arm cr4", 0x18130000)):
        for reg, rn in ((0x408, "ioctl"), (0x500, "iostatus"), (0x800, "resetctl"), (0x804, "resetstatus")):
            bp_read(wrap + reg, f"{name} wrap {rn}")
    bp_read(0x18030000, "arm cr4 core +0")
    bp_read(0x18012000, "pmu core +0")
    bp_read(0x18012600, "pmu control")
    bp_read(0x18012618, "pmu min_res_mask")
    bp_read(0x1801261c, "pmu max_res_mask")
    bp_read(0x1801262c, "pmu res_state")
    bp_read(0x18000000, "chipcommon chipid again")
    # brcmf_chip_disable_arm() / bwfm_chip_cr4_set_passive() on the CR4, step by step.
    def bp_write(addr, val, tag):
        port.w(dev + 0x80, addr & ~0xfff)
        before = p.get_exc_count()
        p.write32(CPU + BAR0 + (addr & 0xfff), val)
        exc = p.get_exc_count() - before
        log(f"chip_probe: WRITE {tag:22s} {addr:#010x} <- {val:#010x} exc={exc} linksts={port.r(port.base + PORT_LINKSTS):#x} intstat={port.r(port.base + PORT_INTSTAT):#x}")
    W = 0x18130000
    HALT, FGC, CLK = 0x20, 0x2, 0x1
    v, _ = bp_read(W + 0x408, "cr4 ioctl (start)")
    pre = v & HALT
    bp_write(W + 0x408, pre | FGC | CLK, "cr4 ioctl prereset")
    bp_read(W + 0x408, "cr4 ioctl readback")
    bp_write(W + 0x800, 1, "cr4 resetctl=1")
    time.sleep(0.001)
    bp_read(W + 0x800, "cr4 resetctl after set")
    bp_write(W + 0x408, HALT | FGC | CLK, "cr4 ioctl in-reset")
    bp_read(W + 0x408, "cr4 ioctl readback")
    bp_write(W + 0x800, 0, "cr4 resetctl=0")
    time.sleep(0.001)
    bp_read(W + 0x800, "cr4 resetctl after clear")
    bp_write(W + 0x408, HALT | CLK, "cr4 ioctl postreset")
    bp_read(W + 0x408, "cr4 ioctl final")
    bp_read(0x18000000, "chipcommon chipid end")
    p.set_exc_guard(GUARD.OFF)
    return [dict(tag=t, addr=a, val=v, exc=e) for t, a, v, e in results]


def bring_up(p, u, out, no_smc=False):
    """pcie_init, then the port-0 link sequence. Returns the result dict; link_up says whether ECAM is usable."""
    result = {"stage": "connected", "ok": False}
    out = Path(out)
    (out / "pcie-adt-facts.json").write_text(json.dumps(adt_facts(u), indent=1))
    port = Port0(p, u)

    # M0: m1n1 controller + PHY + port bring-up.
    ret = p.pcie_init()
    log(f"P_PCIE_INIT returned {ret}")
    result["pcie_init"] = ret
    result["stage"] = "pcie_init"
    result["after_init"] = port.snapshot("after pcie_init")
    result["dart_after_init"] = dart_peek(port, u)
    result["m0_port_ready"] = bool(result["after_init"]["status"] & PORT_STATUS_READY)

    # M1: Linux port link sequence.
    link = bool(result["after_init"]["linksts"] & PORT_LINKSTS_UP)
    attempts = []
    for attempt in ("no-smc", "smc"):
        if link:
            break
        if attempt == "smc":
            if no_smc:
                break
            log("link did not come up; enabling radio power through SMC gP0d and retrying")
            try:
                smc_radio_enable(u)
                time.sleep(0.1)
            except Exception:
                log("SMC radio enable failed:\n" + traceback.format_exc())
                attempts.append({"attempt": attempt, "smc_error": True})
                break
        rec = {"attempt": attempt}
        try:
            port.setup_link()
            rec["rid2sid"] = port.setup_port_tail()
            link = port.train()
        except Exception as e:
            rec["error"] = repr(e)
            log(f"attempt {attempt} error: {e!r}")
        rec["snapshot"] = port.snapshot(f"after attempt {attempt}")
        rec["link_up"] = link
        attempts.append(rec)
    result["attempts"] = attempts
    result["link_up"] = link
    result["stage"] = "link"
    if link:
        result["ecam"] = port.ecam_probe()
        result["dart_after_link"] = dart_peek(port, u)
        result["ok"] = result["ecam"].get("bus1_fn0", {}).get("id") == 0x442514e4
        if os.environ.get("PCIE_CHIP_PROBE") == "1":
            try:
                result["chip_probe"] = chip_probe(port, p)
            except Exception:
                log("chip_probe failed:\n" + traceback.format_exc())
    result["stage"] = "done"
    result["log"] = list(LOG)
    (out / "pcie-result.json").write_text(json.dumps(result, indent=1, default=hex))
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--device", required=True)
    ap.add_argument("--host-source", required=True, type=Path, help="m1n1 checkout (proxyclient inside)")
    ap.add_argument("--out", required=True, type=Path)
    ap.add_argument("--no-smc", action="store_true", help="never try the SMC gP0d radio enable")
    args = ap.parse_args()
    args.out.mkdir(mode=0o700, parents=True, exist_ok=True)
    result = {"stage": "start", "ok": False}
    try:
        sys.path.insert(0, str(args.host_source / "proxyclient"))
        from m1n1.proxy import UartInterface, M1N1Proxy
        from m1n1.proxyutils import ProxyUtils
        iface = UartInterface(device=args.device)
        iface.nop()
        p = M1N1Proxy(iface)
        u = ProxyUtils(p)
        result = bring_up(p, u, args.out, args.no_smc)
    except Exception:
        result["exception"] = traceback.format_exc()
        log(result["exception"])
    finally:
        result["log"] = LOG
        (args.out / "result.json").write_text(json.dumps(result, indent=1, default=hex))
        print("RESULT " + json.dumps({k: result.get(k) for k in ("stage", "ok", "pcie_init", "m0_port_ready", "link_up")}))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    os.umask(0o077)
    raise SystemExit(main())

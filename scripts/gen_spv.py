#!/usr/bin/env python3
"""Generate apps/viewer/spv/*.spv from GLSL.

Prefers Qt's qsb (ships with Qt 6 under <Qt>/<ver>/<kit>/bin/qsb.exe).
Falls back to a minimal pure-Python emitter only if qsb is unavailable.
"""

from __future__ import annotations

import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHADERS = ROOT / "apps" / "viewer" / "shaders"
OUT = ROOT / "apps" / "viewer" / "spv"
SHADER_NAMES = (
    "mesh.vert",
    "mesh.frag",
    "line.vert",
    "line.frag",
    "axis.vert",
    "axis.frag",
)


def find_qsb() -> Path | None:
    env = os.environ.get("QSB") or os.environ.get("QT_QSB")
    if env and Path(env).is_file():
        return Path(env)

    prefix = os.environ.get("CMAKE_PREFIX_PATH", "")
    for part in prefix.replace("\\", "/").split(os.pathsep):
        cand = Path(part) / "bin" / "qsb.exe"
        if cand.is_file():
            return cand
        cand = Path(part) / "bin" / "qsb"
        if cand.is_file():
            return cand

    qt_root = Path(os.environ.get("BREP_QT_ROOT", r"C:\Qt6"))
    if qt_root.is_dir():
        for kit in ("mingw_64", "msvc2022_64", "msvc2019_64", "llvm-mingw_64"):
            for ver_dir in sorted(qt_root.glob("*"), reverse=True):
                cand = ver_dir / kit / "bin" / "qsb.exe"
                if cand.is_file():
                    return cand
                cand = ver_dir / kit / "bin" / "qsb"
                if cand.is_file():
                    return cand

    which = shutil.which("qsb") or shutil.which("qsb.exe")
    return Path(which) if which else None


def compile_with_qsb(qsb: Path) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="brep_qsb_") as tmp:
        tmp_path = Path(tmp)
        for name in SHADER_NAMES:
            src = SHADERS / name
            pack = tmp_path / f"{name}.qsb"
            spv = OUT / f"{name}.spv"
            subprocess.run([str(qsb), "-o", str(pack), str(src)], check=True)
            subprocess.run(
                [str(qsb), "-x", "spirv,100", "-o", str(spv), str(pack)],
                check=True,
            )
            print(f"wrote {spv} ({spv.stat().st_size} bytes) via qsb")


# ---------------------------------------------------------------------------
# Minimal fallback emitter (may lack MatrixStride; prefer qsb)
# ---------------------------------------------------------------------------
OpCapability = 17
OpExtInstImport = 11
OpMemoryModel = 14
OpEntryPoint = 15
OpExecutionMode = 16
OpName = 5
OpMemberName = 6
OpDecorate = 71
OpMemberDecorate = 72
OpTypeVoid = 19
OpTypeFunction = 33
OpTypeFloat = 22
OpTypeInt = 21
OpTypeVector = 23
OpTypeMatrix = 24
OpTypeStruct = 30
OpTypePointer = 32
OpConstant = 43
OpConstantComposite = 46
OpVariable = 59
OpFunction = 54
OpFunctionEnd = 56
OpLabel = 248
OpAccessChain = 65
OpLoad = 61
OpStore = 62
OpCompositeConstruct = 44
OpCompositeExtract = 81
OpFMul = 133
OpFAdd = 129
OpDot = 148
OpExtInst = 12
OpReturn = 253
OpMatrixTimesVector = 145
OpFNegate = 127


def fbits(v: float) -> int:
    return struct.unpack("<I", struct.pack("<f", v))[0]


def enc_str(s: str) -> list[int]:
    raw = s.encode("utf-8") + b"\x00"
    while len(raw) % 4:
        raw += b"\x00"
    return list(struct.unpack("<" + "I" * (len(raw) // 4), raw))


class Module:
    def __init__(self) -> None:
        self.words = [0x07230203, 0x00010000, 0x0008000B, 0, 0]
        self.next_id = 1

    def id(self) -> int:
        i = self.next_id
        self.next_id += 1
        return i

    def emit(self, op: int, *ops: int) -> None:
        self.words.append(((1 + len(ops)) << 16) | op)
        self.words.extend(ops)

    def emit_str(self, op: int, *pre: int, string: str) -> None:
        extra = enc_str(string)
        self.words.append(((1 + len(pre) + len(extra)) << 16) | op)
        self.words.extend(pre)
        self.words.extend(extra)

    def entry(self, model: int, func: int, name: str, interfaces: list[int]) -> None:
        nw = enc_str(name)
        ops = [model, func, *nw, *interfaces]
        self.words.append(((1 + len(ops)) << 16) | OpEntryPoint)
        self.words.extend(ops)

    def finish(self) -> bytes:
        self.words[3] = self.next_id
        return struct.pack("<" + "I" * len(self.words), *self.words)


def _decorate_ubo_matrix(m: Module, tubo_ty: int, member: int, offset: int) -> None:
    m.emit(OpMemberDecorate, tubo_ty, member, 5)  # ColMajor
    m.emit(OpMemberDecorate, tubo_ty, member, 7, 16)  # MatrixStride
    m.emit(OpMemberDecorate, tubo_ty, member, 35, offset)  # Offset


def line_frag() -> bytes:
    m = Module()
    m.emit(OpCapability, 1)
    ext = m.id()
    m.emit_str(OpExtInstImport, ext, string="GLSL.std.450")
    m.emit(OpMemoryModel, 0, 1)
    main = m.id()
    outc = m.id()
    m.entry(4, main, "main", [outc])
    m.emit(OpExecutionMode, main, 7)
    m.emit(OpDecorate, outc, 30, 0)

    tvoid = m.id()
    m.emit(OpTypeVoid, tvoid)
    tfn = m.id()
    m.emit(OpTypeFunction, tfn, tvoid)
    tf = m.id()
    m.emit(OpTypeFloat, tf, 32)
    tv4 = m.id()
    m.emit(OpTypeVector, tv4, tf, 4)
    tout = m.id()
    m.emit(OpTypePointer, tout, 3, tv4)
    m.emit(OpVariable, tout, outc, 3)
    c0 = m.id()
    m.emit(OpConstant, tf, c0, fbits(0.08))
    c1 = m.id()
    m.emit(OpConstant, tf, c1, fbits(0.09))
    c2 = m.id()
    m.emit(OpConstant, tf, c2, fbits(0.12))
    c3 = m.id()
    m.emit(OpConstant, tf, c3, fbits(1.0))
    col = m.id()
    m.emit(OpConstantComposite, tv4, col, c0, c1, c2, c3)

    m.emit(OpFunction, tvoid, main, 0, tfn)
    lab = m.id()
    m.emit(OpLabel, lab)
    m.emit(OpStore, outc, col)
    m.emit(OpReturn)
    m.emit(OpFunctionEnd)
    return m.finish()


def _vert_common(with_normal: bool) -> bytes:
    m = Module()
    m.emit(OpCapability, 1)
    ext = m.id()
    m.emit_str(OpExtInstImport, ext, string="GLSL.std.450")
    m.emit(OpMemoryModel, 0, 1)

    main = m.id()
    in_pos = m.id()
    in_nrm = m.id() if with_normal else None
    v_nrm = m.id() if with_normal else None
    gl_pos = m.id()
    ubo_var = m.id()
    tubo_ty = m.id()

    interfaces = [in_pos, gl_pos]
    if with_normal:
        interfaces = [in_pos, in_nrm, v_nrm, gl_pos]
    m.entry(0, main, "main", interfaces)

    m.emit(OpDecorate, in_pos, 30, 0)
    if with_normal:
        m.emit(OpDecorate, in_nrm, 30, 1)
        m.emit(OpDecorate, v_nrm, 30, 0)
    m.emit(OpDecorate, gl_pos, 11, 0)
    m.emit(OpDecorate, tubo_ty, 2)  # Block
    _decorate_ubo_matrix(m, tubo_ty, 0, 0)
    _decorate_ubo_matrix(m, tubo_ty, 1, 64)
    m.emit(OpMemberDecorate, tubo_ty, 2, 35, 128)
    m.emit(OpDecorate, ubo_var, 33, 0)
    m.emit(OpDecorate, ubo_var, 34, 0)

    tvoid = m.id()
    m.emit(OpTypeVoid, tvoid)
    tfn = m.id()
    m.emit(OpTypeFunction, tfn, tvoid)
    tf = m.id()
    m.emit(OpTypeFloat, tf, 32)
    tint = m.id()
    m.emit(OpTypeInt, tint, 32, 1)
    tv3 = m.id()
    m.emit(OpTypeVector, tv3, tf, 3)
    tv4 = m.id()
    m.emit(OpTypeVector, tv4, tf, 4)
    tm4 = m.id()
    m.emit(OpTypeMatrix, tm4, tv4, 4)
    m.emit(OpTypeStruct, tubo_ty, tm4, tm4, tv4)

    t_in3 = m.id()
    m.emit(OpTypePointer, t_in3, 1, tv3)
    t_out3 = m.id()
    m.emit(OpTypePointer, t_out3, 3, tv3)
    t_out4 = m.id()
    m.emit(OpTypePointer, t_out4, 3, tv4)
    t_ubo_p = m.id()
    m.emit(OpTypePointer, t_ubo_p, 2, tubo_ty)
    t_uni_m4 = m.id()
    m.emit(OpTypePointer, t_uni_m4, 2, tm4)

    m.emit(OpVariable, t_in3, in_pos, 1)
    if with_normal:
        m.emit(OpVariable, t_in3, in_nrm, 1)
        m.emit(OpVariable, t_out3, v_nrm, 3)
    m.emit(OpVariable, t_out4, gl_pos, 3)
    m.emit(OpVariable, t_ubo_p, ubo_var, 2)

    i0 = m.id()
    m.emit(OpConstant, tint, i0, 0)
    i1 = m.id()
    m.emit(OpConstant, tint, i1, 1)
    one_f = m.id()
    m.emit(OpConstant, tf, one_f, fbits(1.0))

    m.emit(OpFunction, tvoid, main, 0, tfn)
    lab = m.id()
    m.emit(OpLabel, lab)

    if with_normal:
        n = m.id()
        m.emit(OpLoad, tv3, n, in_nrm)
        model_ptr = m.id()
        m.emit(OpAccessChain, t_uni_m4, model_ptr, ubo_var, i1)
        model = m.id()
        m.emit(OpLoad, tm4, model, model_ptr)
        c0 = m.id()
        m.emit(OpCompositeExtract, tv4, c0, model, 0)
        c1 = m.id()
        m.emit(OpCompositeExtract, tv4, c1, model, 1)
        c2 = m.id()
        m.emit(OpCompositeExtract, tv4, c2, model, 2)
        r0x = m.id()
        m.emit(OpCompositeExtract, tf, r0x, c0, 0)
        r0y = m.id()
        m.emit(OpCompositeExtract, tf, r0y, c1, 0)
        r0z = m.id()
        m.emit(OpCompositeExtract, tf, r0z, c2, 0)
        row0 = m.id()
        m.emit(OpCompositeConstruct, tv3, row0, r0x, r0y, r0z)
        r1x = m.id()
        m.emit(OpCompositeExtract, tf, r1x, c0, 1)
        r1y = m.id()
        m.emit(OpCompositeExtract, tf, r1y, c1, 1)
        r1z = m.id()
        m.emit(OpCompositeExtract, tf, r1z, c2, 1)
        row1 = m.id()
        m.emit(OpCompositeConstruct, tv3, row1, r1x, r1y, r1z)
        r2x = m.id()
        m.emit(OpCompositeExtract, tf, r2x, c0, 2)
        r2y = m.id()
        m.emit(OpCompositeExtract, tf, r2y, c1, 2)
        r2z = m.id()
        m.emit(OpCompositeExtract, tf, r2z, c2, 2)
        row2 = m.id()
        m.emit(OpCompositeConstruct, tv3, row2, r2x, r2y, r2z)
        nx = m.id()
        m.emit(OpDot, tf, nx, row0, n)
        ny = m.id()
        m.emit(OpDot, tf, ny, row1, n)
        nz = m.id()
        m.emit(OpDot, tf, nz, row2, n)
        nout = m.id()
        m.emit(OpCompositeConstruct, tv3, nout, nx, ny, nz)
        m.emit(OpStore, v_nrm, nout)

    p3 = m.id()
    m.emit(OpLoad, tv3, p3, in_pos)
    px = m.id()
    m.emit(OpCompositeExtract, tf, px, p3, 0)
    py = m.id()
    m.emit(OpCompositeExtract, tf, py, p3, 1)
    pz = m.id()
    m.emit(OpCompositeExtract, tf, pz, p3, 2)
    p4 = m.id()
    m.emit(OpCompositeConstruct, tv4, p4, px, py, pz, one_f)
    mvp_ptr = m.id()
    m.emit(OpAccessChain, t_uni_m4, mvp_ptr, ubo_var, i0)
    mvp = m.id()
    m.emit(OpLoad, tm4, mvp, mvp_ptr)
    clip = m.id()
    m.emit(OpMatrixTimesVector, tv4, clip, mvp, p4)
    m.emit(OpStore, gl_pos, clip)
    m.emit(OpReturn)
    m.emit(OpFunctionEnd)
    return m.finish()


def mesh_frag() -> bytes:
    m = Module()
    m.emit(OpCapability, 1)
    ext = m.id()
    m.emit_str(OpExtInstImport, ext, string="GLSL.std.450")
    m.emit(OpMemoryModel, 0, 1)
    main = m.id()
    v_nrm = m.id()
    outc = m.id()
    ubo_var = m.id()
    tubo_ty = m.id()
    m.entry(4, main, "main", [v_nrm, outc])
    m.emit(OpExecutionMode, main, 7)
    m.emit(OpDecorate, v_nrm, 30, 0)
    m.emit(OpDecorate, outc, 30, 0)
    m.emit(OpDecorate, tubo_ty, 2)
    _decorate_ubo_matrix(m, tubo_ty, 0, 0)
    _decorate_ubo_matrix(m, tubo_ty, 1, 64)
    m.emit(OpMemberDecorate, tubo_ty, 2, 35, 128)
    m.emit(OpDecorate, ubo_var, 33, 0)
    m.emit(OpDecorate, ubo_var, 34, 0)

    tvoid = m.id()
    m.emit(OpTypeVoid, tvoid)
    tfn = m.id()
    m.emit(OpTypeFunction, tfn, tvoid)
    tf = m.id()
    m.emit(OpTypeFloat, tf, 32)
    tint = m.id()
    m.emit(OpTypeInt, tint, 32, 1)
    tv3 = m.id()
    m.emit(OpTypeVector, tv3, tf, 3)
    tv4 = m.id()
    m.emit(OpTypeVector, tv4, tf, 4)
    tm4 = m.id()
    m.emit(OpTypeMatrix, tm4, tv4, 4)
    m.emit(OpTypeStruct, tubo_ty, tm4, tm4, tv4)

    t_in3 = m.id()
    m.emit(OpTypePointer, t_in3, 1, tv3)
    t_out4 = m.id()
    m.emit(OpTypePointer, t_out4, 3, tv4)
    t_ubo_p = m.id()
    m.emit(OpTypePointer, t_ubo_p, 2, tubo_ty)
    t_uni_v4 = m.id()
    m.emit(OpTypePointer, t_uni_v4, 2, tv4)

    m.emit(OpVariable, t_in3, v_nrm, 1)
    m.emit(OpVariable, t_out4, outc, 3)
    m.emit(OpVariable, t_ubo_p, ubo_var, 2)

    i2 = m.id()
    m.emit(OpConstant, tint, i2, 2)
    c025 = m.id()
    m.emit(OpConstant, tf, c025, fbits(0.25))
    c075 = m.id()
    m.emit(OpConstant, tf, c075, fbits(0.75))
    c0 = m.id()
    m.emit(OpConstant, tf, c0, fbits(0.0))
    c1 = m.id()
    m.emit(OpConstant, tf, c1, fbits(1.0))
    br = m.id()
    m.emit(OpConstant, tf, br, fbits(0.45))
    bg = m.id()
    m.emit(OpConstant, tf, bg, fbits(0.62))
    bb = m.id()
    m.emit(OpConstant, tf, bb, fbits(0.85))
    base = m.id()
    m.emit(OpConstantComposite, tv3, base, br, bg, bb)

    m.emit(OpFunction, tvoid, main, 0, tfn)
    lab = m.id()
    m.emit(OpLabel, lab)
    n = m.id()
    m.emit(OpLoad, tv3, n, v_nrm)
    nrm = m.id()
    m.emit(OpExtInst, tv3, nrm, ext, 69, n)
    lptr = m.id()
    m.emit(OpAccessChain, t_uni_v4, lptr, ubo_var, i2)
    l4 = m.id()
    m.emit(OpLoad, tv4, l4, lptr)
    lx = m.id()
    m.emit(OpCompositeExtract, tf, lx, l4, 0)
    ly = m.id()
    m.emit(OpCompositeExtract, tf, ly, l4, 1)
    lz = m.id()
    m.emit(OpCompositeExtract, tf, lz, l4, 2)
    nlx = m.id()
    m.emit(OpFNegate, tf, nlx, lx)
    nly = m.id()
    m.emit(OpFNegate, tf, nly, ly)
    nlz = m.id()
    m.emit(OpFNegate, tf, nlz, lz)
    ld = m.id()
    m.emit(OpCompositeConstruct, tv3, ld, nlx, nly, nlz)
    ldn = m.id()
    m.emit(OpExtInst, tv3, ldn, ext, 69, ld)
    ndotl = m.id()
    m.emit(OpDot, tf, ndotl, nrm, ldn)
    nd = m.id()
    m.emit(OpExtInst, tf, nd, ext, 40, ndotl, c0)
    t = m.id()
    m.emit(OpFMul, tf, t, c075, nd)
    s = m.id()
    m.emit(OpFAdd, tf, s, c025, t)
    cr = m.id()
    m.emit(OpCompositeExtract, tf, cr, base, 0)
    cg = m.id()
    m.emit(OpCompositeExtract, tf, cg, base, 1)
    cb = m.id()
    m.emit(OpCompositeExtract, tf, cb, base, 2)
    rr = m.id()
    m.emit(OpFMul, tf, rr, cr, s)
    gg = m.id()
    m.emit(OpFMul, tf, gg, cg, s)
    bbv = m.id()
    m.emit(OpFMul, tf, bbv, cb, s)
    rgba = m.id()
    m.emit(OpCompositeConstruct, tv4, rgba, rr, gg, bbv, c1)
    m.emit(OpStore, outc, rgba)
    m.emit(OpReturn)
    m.emit(OpFunctionEnd)
    return m.finish()


def compile_fallback() -> None:
    print("WARNING: qsb not found; using fallback SPIR-V emitter", file=sys.stderr)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, fn in {
        "line.frag.spv": line_frag,
        "line.vert.spv": lambda: _vert_common(False),
        "mesh.vert.spv": lambda: _vert_common(True),
        "mesh.frag.spv": mesh_frag,
    }.items():
        path = OUT / name
        data = fn()
        path.write_bytes(data)
        print(f"wrote {path} ({len(data)} bytes)")


def main() -> None:
    qsb = find_qsb()
    if qsb is not None:
        print(f"using qsb: {qsb}")
        compile_with_qsb(qsb)
    else:
        compile_fallback()


if __name__ == "__main__":
    main()

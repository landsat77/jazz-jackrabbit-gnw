#!/usr/bin/env python3
"""Build the game data pack of the Jazz Jackrabbit homebrew.

    python3 jj_pack.py Jazz.jj /path/to/JAZZ

The folder holds the files of Jazz Jackrabbit 1 for DOS (full game,
shareware episode or Holiday Hare): BLOCKS.000, LEVEL0.000, SPRITES.000,
PANEL.000, *.PSM... They are stored as they are (names in upper case), with:

  - SURFACES.BAK: every picture of the game, pre-decoded (the console uses
    them in place from its flash instead of decoding them into its RAM);
  - <NAME>.MUS: the music modules (*.PSM) rendered to samples (the console
    is too small to play the modules).

Both are made by running the game itself on this computer: jj_render
(jj_render/ next to this script, or the harness of the repository).
Pack format: src/port/jj_pack.c.

Only Python 3 is needed (no extra module).
"""
import argparse
import os
import platform
import shutil
import struct
import subprocess
import sys
import tempfile

MAGIC = b"GWJJ01\0\0"
ENTRY_NAME_LEN = 24
REQUIRED = ["PANEL.000", "FONTMN1.0FN", "MENU.000", "SOUNDS.000"]
SKIP_EXT = (".EXE", ".COM", ".BAT", ".DOC", ".TXT", ".ICO", ".PIF", ".DIZ", ".CFG", ".BAK")


def collect(folder):
    files = {}
    for entry in sorted(os.listdir(folder)):
        path = os.path.join(folder, entry)
        if not os.path.isfile(path):
            continue
        name = entry.upper()
        if name.endswith(SKIP_EXT) or len(name) >= ENTRY_NAME_LEN:
            continue
        with open(path, "rb") as fh:
            files[name] = fh.read()
    return files


def write_pack(out_path, files, verbose=True):
    header_size = 16 + len(files) * (ENTRY_NAME_LEN + 8)
    offset = (header_size + 3) & ~3
    entries = []
    for name, blob in files:
        entries.append((name, offset, len(blob)))
        offset = (offset + len(blob) + 3) & ~3
    total = offset
    out = bytearray(total)
    out[0:8] = MAGIC
    struct.pack_into("<II", out, 8, len(files), total)
    for i, (name, off, size) in enumerate(entries):
        nb = name.encode("ascii")[:ENTRY_NAME_LEN - 1]
        struct.pack_into("<%dsII" % ENTRY_NAME_LEN, out, 16 + i * (ENTRY_NAME_LEN + 8), nb, off, size)
    for (name, blob), (_, off, size) in zip(files, entries):
        out[off:off + size] = blob
    with open(out_path, "wb") as fh:
        fh.write(out)
    if verbose:
        print("wrote %s: %d files, %d bytes" % (out_path, len(files), total))
    return total


def default_renderer():
    """jj_render-<os>-<arch> in jj_render/ next to this script (release zip)
    or in tools/bin/ (tools/build_renderers.sh), else the harness built with
    'make -C harness'."""
    here = os.path.dirname(os.path.abspath(__file__))
    system = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}.get(platform.system())
    machine = platform.machine().lower()
    arch = "arm64" if machine in ("arm64", "aarch64") else "x86_64"
    candidates = []
    if system:
        names = ["jj_render-%s-%s%s" % (system, arch, ".exe" if system == "windows" else "")]
        if system == "windows" and arch == "arm64":  # x86_64 emulation
            names.append("jj_render-windows-x86_64.exe")
        for n in names:
            candidates += [os.path.join(here, "jj_render", n), os.path.join(here, "bin", n),
                           os.path.join(here, n)]
    candidates += [os.path.join(here, "..", "harness", "build", "jj_host")]
    for p in candidates:
        if os.path.isfile(p):
            prepare_executable(p)
            return p
    return None


def prepare_executable(path):
    """Make a renderer from a zip file runnable (exec bit, macOS quarantine)."""
    if os.name != "nt" and not os.access(path, os.X_OK):
        try:
            os.chmod(path, os.stat(path).st_mode | 0o111)
        except OSError:
            pass
    if platform.system() == "Darwin":
        subprocess.run(["xattr", "-d", "com.apple.quarantine", path],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def run(renderer, args, what):
    r = subprocess.run([renderer] + args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       universal_newlines=True)
    if r.returncode != 0:
        sys.stderr.write(r.stdout[-4000:])
        sys.exit("error: %s failed (%s)" % (what, renderer))
    return r.stdout


def prepare(items, renderer, music, codec, rate, verbose=True):
    """Run the game on this computer: pre-decoded pictures and music."""
    tmp = tempfile.mkdtemp(prefix="jj_pack_")
    try:
        base = os.path.join(tmp, "base.jj")
        write_pack(base, items, verbose=False)
        result = []
        bake = os.path.join(tmp, "SURFACES.BAK")
        out = run(renderer, [base, "-H", "16000", "-B", bake, "-o", tmp], "making the pictures")
        failed = [l.split()[1] for l in out.splitlines() if l.startswith("bake: ") and l.endswith("failed")]
        if failed and verbose:
            print("note: could not load %s (unused by the game?)" % ", ".join(failed))
        with open(bake, "rb") as fh:
            blob = fh.read()
        result.append(("SURFACES.BAK", blob))
        if verbose:
            print("pictures: %d, %d bytes" % (struct.unpack_from("<I", blob, 4)[0], len(blob)))
        if music:
            mdir = os.path.join(tmp, "music")
            os.mkdir(mdir)
            args = [base, "-R", mdir, "-M", str(rate)] + (["-A"] if codec == "adpcm" else [])
            run(renderer, args, "rendering the music")
            size = 0
            for name in sorted(os.listdir(mdir)):
                if name.endswith(".MUS"):
                    with open(os.path.join(mdir, name), "rb") as fh:
                        data = fh.read()
                    result.append((name, data))
                    size += len(data)
            if verbose:
                print("music: %d pieces, %s %d Hz, %d bytes" % (len(result) - 1, codec.upper(), rate, size))
        return result
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("output", help="pack to write (same name as the .bin, extension .jj)")
    ap.add_argument("folder", help="folder of the DOS game")
    ap.add_argument("--music-rate", type=int, choices=(11025, 22050), default=11025,
                    help="rate of the pre-rendered music (default 11025)")
    ap.add_argument("--music-codec", choices=("pcm", "adpcm"), default="adpcm",
                    help="adpcm (default, 4 times smaller) or pcm (16 bit, no hiss)")
    ap.add_argument("--no-music", action="store_true", help="no music (smaller pack)")
    ap.add_argument("--renderer", help="jj_render program (jj_render/ next to this script by default)")
    ap.add_argument("-q", "--quiet", action="store_true")
    args = ap.parse_args()
    if not os.path.isdir(args.folder):
        sys.exit("error: %s is not a folder" % args.folder)
    files = collect(args.folder)
    missing = [n for n in REQUIRED if n not in files]
    if missing:
        sys.exit("error: missing game data: %s (is this the folder of Jazz Jackrabbit for DOS?)"
                 % ", ".join(missing))
    renderer = args.renderer or default_renderer()
    if not renderer or not os.path.isfile(renderer):
        sys.exit("error: jj_render not found (jj_render/ next to this script, 'make -C harness' or --renderer)")
    items = sorted(files.items())
    extra = prepare(items, renderer, not args.no_music, args.music_codec, args.music_rate,
                    verbose=not args.quiet)
    # the console plays the rendered music, not the modules
    items = [(n, b) for n, b in items if not n.endswith(".PSM")] + extra
    write_pack(args.output, sorted(items), verbose=not args.quiet)


if __name__ == "__main__":
    main()

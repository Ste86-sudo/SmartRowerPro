# Script SCons (extra_scripts = pre:) — genera include/WebUI_HTML.h
# gzippando web/index.html a build time. Sostituisce il vecchio pack_html.ps1:
# un'unica fonte (web/index.html), nessun header da 280KB da rigenerare a mano.
Import("env")

import gzip
import os

PROJECT_DIR = env["PROJECT_DIR"]
SRC_HTML = os.path.normpath(os.path.join(PROJECT_DIR, "..", "web", "index.html"))
OUT_DIR = os.path.join(PROJECT_DIR, "include")
OUT_H = os.path.join(OUT_DIR, "WebUI_HTML.h")


def generate():
    if not os.path.exists(SRC_HTML):
        raise SystemExit("[gen_webui] Sorgente non trovato: %s" % SRC_HTML)

    if os.path.exists(OUT_H) and os.path.getmtime(OUT_H) >= os.path.getmtime(SRC_HTML):
        print("[gen_webui] WebUI_HTML.h aggiornato, nessuna rigenerazione")
        return

    with open(SRC_HTML, "rb") as f:
        raw = f.read()

    # mtime=0 -> output deterministico a parità di input
    data = gzip.compress(raw, compresslevel=9, mtime=0)

    os.makedirs(OUT_DIR, exist_ok=True)
    with open(OUT_H, "w", newline="\n") as f:
        f.write("// FILE GENERATO da tools/gen_webui.py — non editare, modificare web/index.html\n")
        f.write("#pragma once\n#include <pgmspace.h>\n\n")
        f.write("const uint8_t index_html_gz[] PROGMEM = {\n")
        for i in range(0, len(data), 16):
            chunk = data[i:i + 16]
            f.write("    " + ", ".join("0x%02X" % b for b in chunk) + ",\n")
        f.write("};\n")

    print("[gen_webui] Generato %s (%d byte html -> %d byte gz)" % (OUT_H, len(raw), len(data)))


generate()

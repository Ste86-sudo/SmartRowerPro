#!/usr/bin/env python3
"""
SmartRower 2.0 — Simulatore del telaio per test della web UI senza hardware.

Serve la web UI vera (web/index.html) e simula il firmware del telaio:
- WebSocket /ws: CFG, METRICS (2 Hz), buffer binario forza/cavo/sedile (100 Hz),
  gestione comandi (GET_CFG, mech:, encmode:, ghost:, tare, calib:, SCAN, ...)
- vogata realistica a ~21-24 SPM con difetti tecnici casuali, così il Coach
  mostra cue diversi (picco tardivo, curva a spillo, doppio picco,
  shooting the slide, ritorno veloce)
- HR che deriva verso l'alto (per decoupling/zone), watt da lavoro reale
- con --fault simula un guasto encoder a 120 s (rientro dopo 30 s)

Solo libreria standard Python: nessuna dipendenza, funziona offline.

Uso:
    python simulate.py [--port 8080] [--fault]
    poi apri http://localhost:8080
"""

import argparse
import base64
import hashlib
import math
import random
import socket
import struct
import threading
import time
from pathlib import Path

WEB_INDEX = Path(__file__).resolve().parents[2] / "web" / "index.html"
WS_GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

clients = []          # socket WS attivi
clients_lock = threading.Lock()


# ------------------------------- stato simulato -------------------------------

class State:
    def __init__(self, fault_demo=False):
        self.tara = 84213
        self.scala = 10412.5
        self.u_height = 181.0
        self.u_weight = 75.0
        self.pull = 3.0
        self.rel = 1.5
        self.ppr = 600.0
        self.circ = 100.0
        self.loff = 0.0
        self.ftp = 200.0
        self.ghost = [110.0, 0.0, 1.25, 0.33]   # b2, meff, ldrive, dr
        self.enc_enabled = True
        self.fault_demo = fault_demo

        self.watts = 0
        self.spm = 0
        self.pace = 0
        self.dist = 0.0
        self.kcal = 0.0
        self.strokes = 0
        self.hr = 118.0
        self.enc_ticks = 0
        self.dropped = 0
        self.sync = 0
        self.fault = False
        self.pulling = False
        self.force = 0.0
        self.cable = 0.0
        self.seat = 0.35


S = State()


def cfg_msg():
    g = S.ghost
    return ("CFG:%d,%.4f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%s,%s,%.1f|%s|%.2f,%.2f,%.2f,%.2f|%d" % (
        S.tara, S.scala, S.u_height, S.u_weight, S.pull, S.rel, S.ppr, S.circ, S.loff,
        "", "", S.ftp, "AA:BB:CC:DD:EE:FF", g[0], g[1], g[2], g[3], 1 if S.enc_enabled else 0))


def ghost_json():
    n = 64
    b2, meff, ldrive, dr = S.ghost
    F, VS, VA = [], [], []
    fpk = 0.0
    for i in range(n):
        u = i / (n - 1)
        phi = (u ** 1.0) * ((1 - u) ** 2.0)     # Beta(2,3) non normalizzata
        f = b2 * (phi / 0.148) ** 2 * 0.35
        F.append(round(f, 1))
        fpk = max(fpk, f)
        VS.append(round(1.9 * math.sin(min(1.0, u / 0.6) * math.pi) if u < 0.6 else 0.0, 2))
        VA.append(round(1.6 * math.sin(max(0.0, (u - 0.55) / 0.45) * math.pi) if u > 0.55 else 0.0, 2))
    p_teo = (20 / 60.0) * b2 * ldrive ** 3 * 0.35 / ((dr * 3.0) ** 2)
    return ('{"P_teorica":%.1f,"F_peak":%.1f,"F_ghost":%s,"vs_ghost":%s,"va_ghost":%s}' %
            (p_teo, fpk, F, VS, VA)).replace("'", "")


# ------------------------------- websocket framing -------------------------------

def ws_send(sock, payload, binary=False):
    op = 0x2 if binary else 0x1
    data = payload if binary else payload.encode("utf-8")
    ln = len(data)
    if ln < 126:
        hdr = struct.pack("!BB", 0x80 | op, ln)
    elif ln < 65536:
        hdr = struct.pack("!BBH", 0x80 | op, 126, ln)
    else:
        hdr = struct.pack("!BBQ", 0x80 | op, 127, ln)
    sock.sendall(hdr + data)


def broadcast(payload, binary=False):
    dead = []
    with clients_lock:
        for c in clients:
            try:
                ws_send(c, payload, binary)
            except OSError:
                dead.append(c)
        for c in dead:
            clients.remove(c)


def ws_recv(sock):
    """Ritorna (opcode, payload) o None a connessione chiusa."""
    def rd(n):
        buf = b""
        while len(buf) < n:
            chunk = sock.recv(n - len(buf))
            if not chunk:
                return None
            buf += chunk
        return buf

    h = rd(2)
    if h is None:
        return None
    op = h[0] & 0x0F
    masked = h[1] & 0x80
    ln = h[1] & 0x7F
    if ln == 126:
        ext = rd(2)
        if ext is None: return None
        ln = struct.unpack("!H", ext)[0]
    elif ln == 127:
        ext = rd(8)
        if ext is None: return None
        ln = struct.unpack("!Q", ext)[0]
    mask = rd(4) if masked else b"\x00" * 4
    if mask is None:
        return None
    data = rd(ln) if ln else b""
    if data is None:
        return None
    if masked:
        data = bytes(b ^ mask[i % 4] for i, b in enumerate(data))
    return op, data


# ------------------------------- comandi dalla UI -------------------------------

def handle_command(sock, txt):
    txt = txt.strip()
    if txt == "GET_CFG":
        ws_send(sock, cfg_msg())
    elif txt == "GET_PBS":
        ws_send(sock, "PBS:100=24,200=52,500=118,1000=245,2000=512,5000=1350,6000=0,10000=2820,21097=0,42195=0")
    elif txt == "SCAN":
        time.sleep(0.3)
        ws_send(sock, "WIFI_LIST:CasaMia,FASTWEB-7A2F,Vodafone-A1B2")
    elif txt == "tare" or txt.startswith("calib:"):
        print("[SIM] comando calibrazione:", txt)
        broadcast(cfg_msg())
    elif txt.startswith("mech:"):
        try:
            p, r = txt[5:].split("|")
            S.pull, S.rel = float(p), float(r)
            print(f"[SIM] soglie aggiornate: pull={S.pull} rel={S.rel}")
        except ValueError:
            pass
        broadcast(cfg_msg())
    elif txt.startswith("encmode:"):
        S.enc_enabled = txt[8:].strip() != "0"
        print("[SIM] encoder", "ATTIVO" if S.enc_enabled else "DISATTIVATO (stima da picco)")
        broadcast(cfg_msg())
    elif txt.startswith("ghost:"):
        try:
            S.ghost = [float(x) for x in txt[6:].split("|")]
        except ValueError:
            pass
        broadcast(ghost_json())
    elif txt.startswith("CFG:"):
        body = txt[4:].split(",")
        try:
            S.tara = int(float(body[0])); S.scala = float(body[1])
            S.u_height = float(body[2]); S.u_weight = float(body[3])
            S.pull = float(body[4]); S.rel = float(body[5])
            S.ppr = float(body[6]); S.circ = float(body[7]); S.loff = float(body[8])
            if len(body) > 11: S.ftp = float(body[11])
            print("[SIM] CFG salvata")
        except (ValueError, IndexError):
            pass
        broadcast(cfg_msg())
    elif txt.startswith("SAVE_PB:"):
        print("[SIM] PB salvato:", txt[8:])
    elif txt == "REBOOT":
        print("[SIM] REBOOT richiesto (ignorato)")


# ------------------------------- motore di vogata -------------------------------

DEFECTS = ["ok", "ok", "ok", "ok", "late_peak", "spiky", "double", "shoot", "slow_rec"]

def sim_thread():
    dt = 0.01
    t0 = time.time()
    triples = []
    last_metrics = 0.0

    # parametri del ciclo corrente
    def new_cycle():
        spm = random.uniform(20.5, 24.0)
        defect = random.choice(DEFECTS)
        T = 60.0 / spm
        d = 0.36 if defect != "slow_rec" else 0.50
        return {
            "T": T, "drive": d * T, "defect": defect,
            "peak": random.uniform(30, 38),
            "ld": random.uniform(1.28, 1.40),
            "work": 0.0,
        }

    cyc = new_cycle()
    phase = 0.0
    prev_cable = 0.0

    # Scheduling assoluto: su Windows time.sleep(0.01) può dormire ~15.6 ms
    # (Python <3.11), che rallenterebbe la timeline al ~64% del tempo reale.
    # Con il clock assoluto + recupero il rate medio è esattamente 100 Hz.
    next_t = time.perf_counter()

    while True:
        now = time.time() - t0
        phase += dt
        if phase >= cyc["T"]:
            phase -= cyc["T"]
            # fine ciclo: metriche del colpo
            S.strokes += 1
            S.spm = round(60.0 / cyc["T"])
            S.watts = int(min(1500, cyc["work"] / cyc["T"]))
            v = (S.watts / 2.8) ** (1 / 3) if S.watts > 0 else 0
            S.pace = int(500 / v) if v > 0 else 0
            S.kcal += cyc["work"] / 4184.0 * 4.0
            cyc = new_cycle()

        # demo guasto encoder: 120..150 s
        S.fault = S.fault_demo and 120 < now < 150

        drv = cyc["drive"]
        defect = cyc["defect"]
        if phase < drv:
            u = phase / drv
            if defect == "late_peak":
                shape = (u ** 2.2) * ((1 - u) ** 1.0) / 0.105
            elif defect == "spiky":
                shape = ((u ** 1.0) * ((1 - u) ** 2.0) / 0.148) ** 2.4
            else:
                shape = (u ** 1.0) * ((1 - u) ** 2.0) / 0.148     # Beta(2,3)
            f = cyc["peak"] * shape
            if defect == "double":
                f *= 1.0 - 0.38 * math.exp(-((u - 0.55) / 0.09) ** 2)
            f += random.gauss(0, 0.4)
            f = max(0.0, f)
            cable = cyc["ld"] * (1 - math.cos(u * math.pi)) / 2
            if defect == "shoot":
                us = min(1.0, u / 0.25)      # il sedile scappa subito
            else:
                us = min(1.0, u / 0.75)
            seat = 0.30 + 0.62 * cyc["ld"] * 0.65 * (1 - math.cos(us * math.pi)) / 2
        else:
            u = (phase - drv) / (cyc["T"] - drv)
            f = max(0.0, 1.2 * (1 - u) + random.gauss(0, 0.15))
            cable = cyc["ld"] * (1 + math.cos(u * math.pi)) / 2
            us = max(0.0, (u - 0.25) / 0.75)
            seat = 0.30 + 0.62 * cyc["ld"] * 0.65 * (1 + math.cos(us * math.pi)) / 2

        if phase < drv:
            dc = cable - prev_cable
            if dc > 0:
                cyc["work"] += f * 9.81 * dc
                S.dist += ((S.watts / 2.8) ** (1 / 3) if S.watts else 2.0) * dt
        prev_cable = cable

        # guasto: il cavo resta fermo
        if S.fault or not S.enc_enabled:
            cable = 0.0
        S.force = f
        S.cable = cable
        S.seat = seat
        S.pulling = phase < drv
        S.enc_ticks = int(cable / ((S.circ * math.pi / 1000) / S.ppr)) if cable > 0 else 0

        # HR: deriva lenta + risposta ai watt
        target_hr = 112 + S.watts * 0.22 + now * 0.05
        S.hr += (min(178, target_hr) - S.hr) * 0.002 + random.gauss(0, 0.15)

        triples.extend([f, cable, seat])
        if len(triples) >= 39:
            broadcast(struct.pack("<39f", *triples[:39]), binary=True)
            S.sync += 1
            triples = []

        if now - last_metrics >= 0.5:
            last_metrics = now
            if random.random() < 0.02:
                S.dropped += random.randint(1, 3)
            m = ("METRICS:%d|%d|%d|%d|%.1f|1|%d|%d|%d|%d|%.2f|%.2f|%d|%.3f|%d|%d|%d" % (
                S.watts, int(S.dist), S.spm, S.pace, S.kcal,
                S.tara + int(S.force * S.scala / 100), S.sync, int(S.hr),
                S.enc_ticks, S.seat, S.force, 1 if S.pulling else 0,
                S.cable, S.dropped, 1 if S.fault else 0, S.strokes))
            broadcast(m)

        next_t += dt
        delay = next_t - time.perf_counter()
        if delay > 0:
            time.sleep(delay)
        elif delay < -0.5:
            next_t = time.perf_counter()   # troppo indietro: riallinea senza raffica


# ------------------------------- server HTTP + WS -------------------------------

def handle_conn(sock, addr):
    try:
        sock.settimeout(5)
        req = b""
        while b"\r\n\r\n" not in req:
            chunk = sock.recv(4096)
            if not chunk:
                return
            req += chunk
        head = req.decode("utf-8", "replace")
        line0 = head.split("\r\n", 1)[0]
        path = line0.split(" ")[1] if len(line0.split(" ")) > 1 else "/"
        headers = {}
        for ln in head.split("\r\n")[1:]:
            if ":" in ln:
                k, v = ln.split(":", 1)
                headers[k.strip().lower()] = v.strip()

        if headers.get("upgrade", "").lower() == "websocket":
            key = headers.get("sec-websocket-key", "")
            accept = base64.b64encode(hashlib.sha1((key + WS_GUID).encode()).digest()).decode()
            sock.sendall((
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                f"Sec-WebSocket-Accept: {accept}\r\n\r\n").encode())
            sock.settimeout(None)
            with clients_lock:
                clients.append(sock)
            print(f"[SIM] client WS connesso da {addr[0]} (totale {len(clients)})")
            while True:
                fr = ws_recv(sock)
                if fr is None:
                    break
                op, data = fr
                if op == 0x8:      # close
                    break
                if op == 0x9:      # ping → pong
                    ws_send(sock, data.decode("utf-8", "replace"))
                elif op == 0x1:
                    handle_command(sock, data.decode("utf-8", "replace"))
            with clients_lock:
                if sock in clients:
                    clients.remove(sock)
            print("[SIM] client WS disconnesso")
            return

        # HTTP semplice
        if path == "/" or path.startswith("/index"):
            body = WEB_INDEX.read_bytes()
            sock.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                         b"Cache-Control: no-store\r\nContent-Length: " + str(len(body)).encode()
                         + b"\r\n\r\n" + body)
        elif path == "/ghost":
            body = ghost_json().encode()
            sock.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                         + str(len(body)).encode() + b"\r\n\r\n" + body)
        elif path == "/update":
            body = b"<html><body style='background:#020617;color:#fff'>Pagina OTA (simulata)</body></html>"
            sock.sendall(b"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: "
                         + str(len(body)).encode() + b"\r\n\r\n" + body)
        else:
            sock.sendall(b"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n")
    except OSError:
        pass
    finally:
        with clients_lock:
            if sock in clients:
                clients.remove(sock)
        try:
            sock.close()
        except OSError:
            pass


def main():
    ap = argparse.ArgumentParser(description="Simulatore telaio SmartRower")
    ap.add_argument("--port", type=int, default=8080)
    ap.add_argument("--fault", action="store_true",
                    help="simula guasto encoder da 120 a 150 s")
    args = ap.parse_args()
    S.fault_demo = args.fault

    if not WEB_INDEX.exists():
        raise SystemExit(f"web/index.html non trovato: {WEB_INDEX}")

    threading.Thread(target=sim_thread, daemon=True).start()

    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("0.0.0.0", args.port))
    srv.listen(8)
    print(f"[SIM] Telaio simulato su http://localhost:{args.port}"
          + ("  (demo guasto encoder a 120 s)" if args.fault else ""))
    print("[SIM] Ctrl+C per uscire")
    try:
        while True:
            c, a = srv.accept()
            threading.Thread(target=handle_conn, args=(c, a), daemon=True).start()
    except KeyboardInterrupt:
        print("\n[SIM] chiusura")


if __name__ == "__main__":
    main()

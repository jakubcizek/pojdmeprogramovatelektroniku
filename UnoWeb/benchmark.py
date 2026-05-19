#!/usr/bin/env python3
"""
benchmark.py - zatezovy / brute-force HTTP test pro UnoWeb (ATMEGA328P + W5500).

Otevre N TCP spojeni (volitelne ve vice vlaknech), na kazdem posle
"GET / HTTP/1.0" a meri, jak dlouho trvalo nez prisla cela stranka
(connect + send + prijem tela az do uzavreni spojeni serverem).

Nakonec vypise: nejkratsi / prumernou / nejdelsi transakci, medianu a
percentily, pocet uspechu/selhani a celkovy cas vsech spojeni.

Pozn.: W5500 ma jediny TCP socket v LISTEN -> server obsluhuje vzdy jen
jedno spojeni. Vyssi soubeznost tedy hlavne meri front/odmitnuta spojeni;
to je zamer (brute-force). Selhani se pocitaji zvlast, aby nezkreslila
statistiku uspesnych transakci.

Pouziti:
    python benchmark.py HOST [-n POCET] [-c VLAKEN] [--timeout S] [--port P]

HOST je povinny - musi ho vzdy zadat uzivatel. Lze zadat jako holou
IP/hostname (napr. HOST), jako HOST:PORT nebo jako URL vcetne portu
(scheme://HOST:PORT). Port z URL se pouzije, pokud neni explicitne
prepsan prepinacem --port; jinak je vychozi 80.

Priklad:
    python benchmark.py HOST -n 1000 -c 8
    python benchmark.py scheme://HOST:PORT -n 500

Bez zavislosti - pouze standardni knihovna.
"""

import argparse
import socket
import statistics
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from urllib.parse import urlparse

DEFAULT_PORT = 80


def parse_target(raw_host: str, port_override):
    """Rozlozi zadany cil na (host, port).

    Prijima holou IP/hostname, tvar HOST:PORT i URL vcetne portu
    (scheme://HOST:PORT). Priorita portu:
    explicitni --port  >  port z URL  >  DEFAULT_PORT.
    """
    raw_host = raw_host.strip()
    url_port = None

    if "://" in raw_host:
        parsed = urlparse(raw_host)
        host = parsed.hostname or ""
        url_port = parsed.port
    elif "/" in raw_host:
        # napr. "HOST:PORT/cesta" bez schematu
        parsed = urlparse("//" + raw_host)
        host = parsed.hostname or ""
        url_port = parsed.port
    elif raw_host.count(":") == 1:
        # "host:port" bez schematu (ne IPv6)
        parsed = urlparse("//" + raw_host)
        host = parsed.hostname or ""
        url_port = parsed.port
    else:
        host = raw_host

    if not host:
        raise ValueError(f"nelze rozpoznat hostname z {raw_host!r}")

    if port_override is not None:
        port = port_override
    elif url_port is not None:
        port = url_port
    else:
        port = DEFAULT_PORT
    return host, port


def one_request(host: str, port: int, timeout: float):
    """Jedna kompletni HTTP transakce.

    Vraci (ok: bool, elapsed_s: float, nbytes: int, info: str).
    elapsed se meri od tesne pred connect az po prijem celeho tela.
    """
    req = (
        "GET / HTTP/1.0\r\n"
        f"Host: {host}\r\n"
        "Connection: close\r\n"
        "\r\n"
    ).encode("ascii")

    t0 = time.perf_counter()
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(timeout)
    try:
        sock.connect((host, port))
        sock.sendall(req)

        chunks = []
        while True:
            buf = sock.recv(1024)
            if not buf:            # server uzavrel spojeni -> konec tela
                break
            chunks.append(buf)

        elapsed = time.perf_counter() - t0
        data = b"".join(chunks)
        if not data:
            return False, elapsed, 0, "prazdna odpoved"

        first_line = data.split(b"\r\n", 1)[0].decode("latin1", "replace")
        ok = b" 200 " in data[:64]
        return ok, elapsed, len(data), first_line
    except (socket.timeout, OSError) as exc:
        return False, time.perf_counter() - t0, 0, f"{type(exc).__name__}: {exc}"
    finally:
        try:
            sock.close()
        except OSError:
            pass


def fmt_ms(seconds: float) -> str:
    return f"{seconds * 1000.0:.2f} ms"


def main() -> int:
    ap = argparse.ArgumentParser(description="HTTP zatezovy test pro UnoWeb")
    ap.add_argument("host",
                    help="POVINNE: IP/hostname, HOST:PORT nebo URL desky "
                         "(scheme://HOST:PORT)")
    ap.add_argument("--port", type=int, default=None,
                    help=f"TCP port; prebije port z URL (vychozi {DEFAULT_PORT})")
    ap.add_argument("-n", "--requests", type=int, default=1000,
                    help="celkovy pocet spojeni (vychozi 1000)")
    ap.add_argument("-c", "--concurrency", type=int, default=8,
                    help="pocet soubeznych vlaken (vychozi 8)")
    ap.add_argument("--timeout", type=float, default=5.0,
                    help="timeout na spojeni v sekundach (vychozi 5)")
    args = ap.parse_args()

    try:
        host, port = parse_target(args.host, args.port)
    except ValueError as exc:
        print(f"Chyba: {exc}", file=sys.stderr)
        return 2

    print(f"Cil:        http://{host}:{port}/")
    print(f"Spojeni:    {args.requests}   vlaken: {args.concurrency}   "
          f"timeout: {args.timeout:g}s")
    print("Bezi ...", flush=True)

    durations = []          # jen uspesne transakce
    ok_count = 0
    fail_count = 0
    total_bytes = 0
    err_samples = {}

    wall_start = time.perf_counter()
    with ThreadPoolExecutor(max_workers=args.concurrency) as pool:
        futures = [
            pool.submit(one_request, host, port, args.timeout)
            for _ in range(args.requests)
        ]
        done = 0
        for fut in as_completed(futures):
            ok, elapsed, nbytes, info = fut.result()
            done += 1
            if ok:
                ok_count += 1
                durations.append(elapsed)
                total_bytes += nbytes
            else:
                fail_count += 1
                err_samples[info] = err_samples.get(info, 0) + 1
            if done % 100 == 0:
                print(f"  ... {done}/{args.requests}", flush=True)
    wall = time.perf_counter() - wall_start

    print("\n================ VYSLEDKY ================")
    print(f"Celkovy cas vsech spojeni : {wall:.3f} s")
    print(f"Uspesnych transakci       : {ok_count}/{args.requests}")
    print(f"Selhani                   : {fail_count}")
    if ok_count:
        print(f"Propustnost (uspesne)     : {ok_count / wall:.1f} req/s")
        print(f"Prenesenych bajtu         : {total_bytes} "
              f"(prum. {total_bytes // ok_count} B/odpoved)")
        print("---- doba transakce (uspesne) ----")
        print(f"  nejkratsi : {fmt_ms(min(durations))}")
        print(f"  prumer    : {fmt_ms(statistics.fmean(durations))}")
        print(f"  median    : {fmt_ms(statistics.median(durations))}")
        order = sorted(durations)
        p95 = order[min(len(order) - 1, int(len(order) * 0.95))]
        p99 = order[min(len(order) - 1, int(len(order) * 0.99))]
        print(f"  p95       : {fmt_ms(p95)}")
        print(f"  p99       : {fmt_ms(p99)}")
        print(f"  nejdelsi  : {fmt_ms(max(durations))}")
        if len(durations) > 1:
            print(f"  smer.odch.: {fmt_ms(statistics.pstdev(durations))}")
    if err_samples:
        print("---- nejcastejsi chyby ----")
        for msg, cnt in sorted(err_samples.items(),
                               key=lambda kv: kv[1], reverse=True)[:5]:
            print(f"  {cnt:5d}x  {msg}")
    print("==========================================")
    return 0 if ok_count else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print("\nPreruseno.")
        sys.exit(130)

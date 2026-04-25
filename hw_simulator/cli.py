"""
Biometric Vault — CLI Test Tool

Hardware Simulator ile iletişim kurmak için komut satırı aracı.
Geliştirme ve test sırasında kullanılır.

Kullanım:
    python cli.py status                   # Cihaz durumu
    python cli.py enroll 1 "Sağ İşaret"    # Parmak izi kaydet
    python cli.py verify 1                 # Parmak izi doğrula
    python cli.py list                     # Kayıtlı parmak izlerini listele
    python cli.py delete 1                 # Parmak izi sil
    python cli.py disconnect               # Cihaz çekilmesini simüle et
    python cli.py reconnect                # Cihaz bağlanmasını simüle et
"""

import asyncio
import argparse
import json
import sys

from protocol import SOCKET_PATH, Request, Response


# ─── ANSI Colors ─────────────────────────────────────────────────────────────

class Color:
    RESET  = "\033[0m"
    BOLD   = "\033[1m"
    RED    = "\033[91m"
    GREEN  = "\033[92m"
    YELLOW = "\033[93m"
    BLUE   = "\033[94m"
    CYAN   = "\033[96m"
    DIM    = "\033[2m"


def success(msg: str) -> str:
    return f"{Color.GREEN}✓{Color.RESET} {msg}"


def error(msg: str) -> str:
    return f"{Color.RED}✗{Color.RESET} {msg}"


def info(msg: str) -> str:
    return f"{Color.BLUE}ℹ{Color.RESET} {msg}"


def header(msg: str) -> str:
    return f"{Color.BOLD}{Color.CYAN}{msg}{Color.RESET}"


# ─── Socket Client ──────────────────────────────────────────────────────────

async def send_request(request: Request) -> Response:
    """Simülatöre istek gönder ve yanıt al."""
    try:
        reader, writer = await asyncio.open_unix_connection(SOCKET_PATH)
    except (ConnectionRefusedError, FileNotFoundError):
        print(error("Cannot connect to simulator!"))
        print(info(f"Is the simulator running? Start it with:"))
        print(f"  {Color.DIM}python simulator.py{Color.RESET}")
        sys.exit(1)

    try:
        # İstek gönder
        writer.write(request.to_json().encode("utf-8") + b"\n")
        await writer.drain()

        # Yanıt al
        data = await asyncio.wait_for(reader.readline(), timeout=5.0)
        return Response.from_json(data.decode("utf-8").strip())
    finally:
        writer.close()
        await writer.wait_closed()


# ─── Command Handlers ───────────────────────────────────────────────────────

async def cmd_status(_args: argparse.Namespace) -> None:
    """Cihaz durumunu göster."""
    resp = await send_request(Request(cmd="status"))

    print(header("═══ Device Status ═══"))

    if resp.status == "ok":
        state = resp.device_state or "unknown"
        state_color = Color.GREEN if state == "connected" else Color.RED
        print(f"  State : {state_color}{state}{Color.RESET}")
    else:
        print(error(resp.reason or "Unknown error"))


async def cmd_enroll(args: argparse.Namespace) -> None:
    """Parmak izi kaydet."""
    name = args.name if args.name else None
    resp = await send_request(
        Request(cmd="enroll", finger_id=args.finger_id, name=name)
    )

    if resp.status == "ok":
        print(success(f"Enrolled finger #{resp.finger_id}"))
    else:
        print(error(f"Enroll failed: {resp.reason}"))


async def cmd_verify(args: argparse.Namespace) -> None:
    """Parmak izi doğrula."""
    resp = await send_request(Request(cmd="verify", finger_id=args.finger_id))

    if resp.status == "ok":
        print(success("Fingerprint verified!"))
        print(f"  {Color.DIM}Key: {resp.key[:16]}...{Color.RESET}")
    else:
        print(error(f"Verification failed: {resp.reason}"))


async def cmd_delete(args: argparse.Namespace) -> None:
    """Parmak izi sil."""
    resp = await send_request(Request(cmd="delete", finger_id=args.finger_id))

    if resp.status == "ok":
        print(success(f"Deleted finger #{resp.finger_id}"))
    else:
        print(error(f"Delete failed: {resp.reason}"))


async def cmd_list(_args: argparse.Namespace) -> None:
    """Kayıtlı parmak izlerini listele."""
    resp = await send_request(Request(cmd="list"))

    if resp.status == "ok":
        fingers = resp.fingers or []
        if not fingers:
            print(info("No fingerprints enrolled"))
            return

        print(header("═══ Enrolled Fingerprints ═══"))
        print(f"  {'ID':>4}  {'Name':<20}  {'Enrolled':>20}  {'Verifications':>14}")
        print(f"  {'─'*4}  {'─'*20}  {'─'*20}  {'─'*14}")
        for fp in fingers:
            print(
                f"  {fp['finger_id']:>4}  "
                f"{fp['name']:<20}  "
                f"{fp['enrolled_at']:>20}  "
                f"{fp['verify_count']:>14}"
            )
        print(f"\n  {Color.DIM}Total: {len(fingers)} fingerprint(s){Color.RESET}")
    else:
        print(error(f"List failed: {resp.reason}"))


async def cmd_disconnect(_args: argparse.Namespace) -> None:
    """Cihaz çekilmesini simüle et."""
    resp = await send_request(Request(cmd="disconnect"))

    if resp.status == "ok":
        print(f"{Color.YELLOW}⚡{Color.RESET} Device disconnected — key wiped")
    else:
        print(error(f"Disconnect failed: {resp.reason}"))


async def cmd_reconnect(_args: argparse.Namespace) -> None:
    """Cihaz bağlanmasını simüle et."""
    resp = await send_request(Request(cmd="reconnect"))

    if resp.status == "ok":
        print(f"{Color.GREEN}🔌{Color.RESET} Device reconnected — key reloaded")
    else:
        print(error(f"Reconnect failed: {resp.reason}"))


# ─── Argument Parsing ───────────────────────────────────────────────────────

def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Biometric Vault — Hardware Simulator CLI",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=f"""
{Color.BOLD}Examples:{Color.RESET}
  python cli.py status                    Check device status
  python cli.py enroll 1 "Right Index"    Enroll a fingerprint
  python cli.py verify 1                  Verify a fingerprint
  python cli.py list                      List enrolled fingerprints
  python cli.py delete 1                  Delete a fingerprint
  python cli.py disconnect                Simulate device removal
  python cli.py reconnect                 Simulate device reconnection
        """,
    )

    subparsers = parser.add_subparsers(dest="command", help="Command to execute")
    subparsers.required = True

    # status
    subparsers.add_parser("status", help="Show device status")

    # enroll
    p_enroll = subparsers.add_parser("enroll", help="Enroll a new fingerprint")
    p_enroll.add_argument("finger_id", type=int, help="Finger ID (1-127)")
    p_enroll.add_argument("name", nargs="?", default=None, help="Finger name (optional)")

    # verify
    p_verify = subparsers.add_parser("verify", help="Verify a fingerprint")
    p_verify.add_argument("finger_id", type=int, help="Finger ID to verify")

    # delete
    p_delete = subparsers.add_parser("delete", help="Delete an enrolled fingerprint")
    p_delete.add_argument("finger_id", type=int, help="Finger ID to delete")

    # list
    subparsers.add_parser("list", help="List enrolled fingerprints")

    # disconnect
    subparsers.add_parser("disconnect", help="Simulate device disconnection")

    # reconnect
    subparsers.add_parser("reconnect", help="Simulate device reconnection")

    return parser


# ─── Main ────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = build_parser()
    args = parser.parse_args()

    handlers = {
        "status": cmd_status,
        "enroll": cmd_enroll,
        "verify": cmd_verify,
        "delete": cmd_delete,
        "list": cmd_list,
        "disconnect": cmd_disconnect,
        "reconnect": cmd_reconnect,
    }

    handler = handlers[args.command]
    asyncio.run(handler(args))


if __name__ == "__main__":
    main()

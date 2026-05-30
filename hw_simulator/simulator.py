"""
Biometric Vault — Hardware Simulator Daemon

STM32F103C8T6 + AS608 donanım davranışını simüle eden daemon.
Unix Domain Socket üzerinden JSON mesajlarla haberleşir.

Kullanım:
    python simulator.py                 # Daemon'ı başlat
    python simulator.py --no-persist    # Her başlatmada yeni key üret
    python simulator.py --verbose       # Detaylı loglama

Donanım geldiğinde:
    Bu daemon devre dışı kalır, yerine gerçek UART bağlantısı kullanılır.
    HAL katmanı (hal_simulator.c → hal_serial.c) bu geçişi sağlar.
"""

import asyncio
import argparse
import logging
import os
import signal
import sys
import time
from pathlib import Path

from protocol import (
    SOCKET_PATH,
    Command,
    DeviceState,
    Request,
    Response,
    make_ok_response,
    make_error_response,
)
from crypto_key import KeyManager
from fingerprint_db import FingerprintDB
from binary_protocol import (
    CMD_ENROLL_START,
    CMD_VERIFY_START,
    CMD_LIST_ENROLLED,
    CMD_DELETE,
    CMD_CANCEL,
    CMD_GET_KEY,
    EVT_WAITING_FINGER,
    EVT_FINGER_DETECTED,
    EVT_REMOVE_FINGER,
    EVT_PLACE_AGAIN,
    EVT_PROCESSING,
    RES_SUCCESS,
    RES_FAIL_NO_MATCH,
    RES_FAIL_ALREADY_EXISTS,
    RES_FAIL_SLOT_FULL,
    RES_FAIL_INVALID_ID,
    RES_KEY_DATA,
    RES_CANCELLED,
    build_packet,
    PacketReader,
)

# ─── Logging ─────────────────────────────────────────────────────────────────

LOG_FORMAT = "%(asctime)s [%(levelname)s] %(message)s"
LOG_DATE_FORMAT = "%H:%M:%S"

logger = logging.getLogger("hw_simulator")


# ─── Simulator ───────────────────────────────────────────────────────────────

class HardwareSimulator:
    """
    STM32 + AS608 donanımını simüle eden ana sınıf.

    Durum makinesi:
        CONNECTED ←→ DISCONNECTED

    CONNECTED durumunda:
        - Tüm komutlar işlenir
        - verify başarılıysa AES key döndürülür

    DISCONNECTED durumunda:
        - Sadece reconnect ve status komutları kabul edilir
        - Diğer komutlar "device_disconnected" hatası döner
    """

    def __init__(self, persist_key: bool = True):
        self._state = DeviceState.CONNECTED
        self._key_manager = KeyManager(persist=persist_key)
        self._fingerprint_db = FingerprintDB()

        # İstatistikler
        self._start_time = time.time()
        self._request_count = 0
        self._verify_success_count = 0
        self._verify_fail_count = 0
        self._key_window_until: float = 0.0
        self._cancel_requested = False

    def initialize(self) -> None:
        """Simülatörü başlat, key ve DB'yi yükle."""
        self._key_manager.initialize()
        self._fingerprint_db.load()

        logger.info("=" * 60)
        logger.info("  Biometric Vault — Hardware Simulator")
        logger.info("=" * 60)
        logger.info(f"  Device State  : {self._state.value}")
        logger.info(f"  Key ID        : {self._key_manager.key_id}")
        logger.info(f"  Fingerprints  : {self._fingerprint_db.count} enrolled")
        logger.info(f"  Socket        : {SOCKET_PATH}")
        logger.info("=" * 60)

    def handle_request(self, request: Request) -> Response:
        """
        Gelen isteği işle ve yanıt döndür.
        STM32 firmware'inin komut işleme döngüsünü simüle eder.
        """
        self._request_count += 1

        # Bağlantı durumu kontrolü
        if self._state == DeviceState.DISCONNECTED:
            if request.cmd not in (Command.RECONNECT, Command.STATUS):
                logger.warning(f"Command '{request.cmd}' rejected — device disconnected")
                return make_error_response("device_disconnected")

        try:
            match request.cmd:
                case Command.ENROLL:
                    return self._handle_enroll(request)
                case Command.VERIFY:
                    return self._handle_verify(request)
                case Command.VERIFY_START:
                    return self._handle_verify_start()
                case Command.GET_KEY:
                    return self._handle_get_key()
                case Command.CANCEL:
                    self._cancel_requested = True
                    return make_ok_response()
                case Command.DELETE:
                    return self._handle_delete(request)
                case Command.LIST:
                    return self._handle_list()
                case Command.STATUS:
                    return self._handle_status()
                case Command.DISCONNECT:
                    return self._handle_disconnect()
                case Command.RECONNECT:
                    return self._handle_reconnect()
                case _:
                    logger.error(f"Unknown command: {request.cmd}")
                    return make_error_response(f"unknown_command: {request.cmd}")
        except Exception as e:
            logger.exception(f"Error handling command '{request.cmd}'")
            return make_error_response(str(e))

    # ─── Command Handlers ────────────────────────────────────────────────

    def _handle_enroll(self, request: Request) -> Response:
        """
        Parmak izi kayıt.
        Gerçek AS608'de bu işlem ~3 saniye sürer (2x parmak tarama).
        """
        if request.finger_id is None:
            return make_error_response("finger_id required")

        try:
            fp = self._fingerprint_db.enroll(request.finger_id, request.name)
            logger.info(
                f"✓ Enrolled finger #{fp.finger_id} "
                f"('{fp.name}') — total: {self._fingerprint_db.count}"
            )
            return make_ok_response(finger_id=fp.finger_id)
        except ValueError as e:
            logger.warning(f"✗ Enroll failed: {e}")
            return make_error_response(str(e))

    def _handle_verify(self, request: Request) -> Response:
        """Legacy: verify specific finger_id (no key — use get_key)."""
        if request.finger_id is None:
            return make_error_response("finger_id required")
        if self._fingerprint_db.verify(request.finger_id):
            self._verify_success_count += 1
            self._key_window_until = time.time() + 10.0
            return make_ok_response(finger_id=request.finger_id)
        self._verify_fail_count += 1
        return make_error_response("no_match")

    def _handle_verify_start(self) -> Response:
        """1:N verify — opens 10s GET_KEY window on success."""
        if not self._key_manager.is_available():
            return make_error_response("device_disconnected")
        matched = self._fingerprint_db.verify_any()
        if matched is None:
            self._verify_fail_count += 1
            return make_error_response("no_match")
        self._verify_success_count += 1
        self._key_window_until = time.time() + 10.0
        logger.info(f"✓ verify_start matched finger #{matched}")
        return make_ok_response(finger_id=matched)

    def _handle_get_key(self) -> Response:
        if time.time() > self._key_window_until:
            return make_error_response("key_window_expired")
        if not self._key_manager.is_available():
            return make_error_response("device_disconnected")
        self._key_window_until = 0.0
        return make_ok_response(key=self._key_manager.key_b64)

    async def _emit_scan_events(self, writer, need_remove: bool = False) -> None:
        """UART event sequence for enroll/verify."""
        for code in (EVT_WAITING_FINGER, EVT_FINGER_DETECTED):
            writer.write(build_packet(code))
            await writer.drain()
            await asyncio.sleep(0.05)
        if need_remove:
            writer.write(build_packet(EVT_REMOVE_FINGER))
            await writer.drain()
            await asyncio.sleep(0.05)
            writer.write(build_packet(EVT_PLACE_AGAIN))
            await writer.drain()
            await asyncio.sleep(0.05)
        writer.write(build_packet(EVT_PROCESSING))
        await writer.drain()
        await asyncio.sleep(0.1)

    async def handle_uart_packets(self, code: int, payload: bytes, writer) -> None:
        """Process one binary command; write response packets to writer."""
        self._cancel_requested = False
        if self._state == DeviceState.DISCONNECTED:
            writer.write(build_packet(RES_FAIL_NO_MATCH))
            await writer.drain()
            return

        if code == CMD_CANCEL:
            self._cancel_requested = True
            writer.write(build_packet(RES_CANCELLED))
            await writer.drain()
            return

        if code == CMD_VERIFY_START:
            await self._emit_scan_events(writer)
            matched = self._fingerprint_db.verify_any()
            if matched is None:
                writer.write(build_packet(RES_FAIL_NO_MATCH))
            else:
                self._key_window_until = time.time() + 10.0
                writer.write(build_packet(RES_SUCCESS, bytes([matched])))
            await writer.drain()
            return

        if code == CMD_GET_KEY:
            if time.time() > self._key_window_until or not self._key_manager.is_available():
                writer.write(build_packet(RES_FAIL_NO_MATCH))
            else:
                self._key_window_until = 0.0
                writer.write(build_packet(RES_KEY_DATA, self._key_manager.key))
            await writer.drain()
            return

        if code == CMD_ENROLL_START:
            if len(payload) < 1:
                writer.write(build_packet(RES_FAIL_INVALID_ID))
                await writer.drain()
                return
            finger_id = payload[0]
            await self._emit_scan_events(writer, need_remove=True)
            try:
                fp = self._fingerprint_db.enroll(finger_id, None)
                writer.write(build_packet(RES_SUCCESS, bytes([fp.finger_id])))
            except ValueError as e:
                msg = str(e)
                if "already" in msg:
                    writer.write(build_packet(RES_FAIL_ALREADY_EXISTS, bytes([finger_id])))
                elif "full" in msg:
                    writer.write(build_packet(RES_FAIL_SLOT_FULL))
                else:
                    writer.write(build_packet(RES_FAIL_INVALID_ID))
            await writer.drain()
            return

        if code == CMD_DELETE:
            if len(payload) < 1:
                writer.write(build_packet(RES_FAIL_INVALID_ID))
                await writer.drain()
                return
            fid = payload[0]
            if self._fingerprint_db.delete(fid):
                writer.write(build_packet(RES_SUCCESS, bytes([fid])))
            else:
                writer.write(build_packet(RES_FAIL_NO_MATCH))
            await writer.drain()
            return

        if code == CMD_LIST_ENROLLED:
            ids = self._fingerprint_db.list_enrolled_ids()
            body = bytes([len(ids)]) + bytes(ids)
            writer.write(build_packet(RES_SUCCESS, body))
            await writer.drain()
            return

    def _handle_delete(self, request: Request) -> Response:
        """Kayıtlı parmak izini sil."""
        if request.finger_id is None:
            return make_error_response("finger_id required")

        deleted = self._fingerprint_db.delete(request.finger_id)
        if deleted:
            logger.info(
                f"✓ Deleted finger #{request.finger_id} "
                f"— remaining: {self._fingerprint_db.count}"
            )
            return make_ok_response(finger_id=request.finger_id)
        else:
            logger.warning(f"✗ Finger #{request.finger_id} not found")
            return make_error_response("not_found")

    def _handle_list(self) -> Response:
        """Kayıtlı parmak izlerini listele."""
        fingers = self._fingerprint_db.list_all()
        logger.info(f"Listed {len(fingers)} enrolled fingerprints")
        return make_ok_response(fingers=fingers)

    def _handle_status(self) -> Response:
        """Cihaz durumunu döndür."""
        uptime = int(time.time() - self._start_time)
        logger.debug(f"Status query — state={self._state.value}, uptime={uptime}s")
        return make_ok_response(
            device_state=self._state.value,
        )

    def _handle_disconnect(self) -> Response:
        """
        Cihaz çekilmesini simüle et.
        Gerçek donanımda: USB kablo çekilir → udev event tetiklenir.
        Simülasyonda: state değişir, key bellekten silinir.
        """
        if self._state == DeviceState.DISCONNECTED:
            return make_error_response("already_disconnected")

        self._state = DeviceState.DISCONNECTED
        self._key_manager.wipe()
        logger.warning("⚡ DEVICE DISCONNECTED — key wiped from memory")
        return make_ok_response(device_state=self._state.value)

    def _handle_reconnect(self) -> Response:
        """
        Cihaz tekrar bağlanmasını simüle et.
        Key tekrar yüklenir (STM32 flash'ından okuma simülasyonu).
        """
        if self._state == DeviceState.CONNECTED:
            return make_error_response("already_connected")

        self._state = DeviceState.CONNECTED
        self._key_manager.initialize()  # Key'i tekrar yükle
        logger.info("🔌 DEVICE RECONNECTED — key reloaded")
        return make_ok_response(device_state=self._state.value)


# ─── Async Socket Server ────────────────────────────────────────────────────

class SimulatorServer:
    """
    Unix Domain Socket üzerinden bağlantı kabul eden async server.
    Her bağlantı tek bir request/response döngüsü çalıştırır.
    """

    def __init__(self, simulator: HardwareSimulator):
        self._simulator = simulator
        self._server: asyncio.AbstractServer | None = None

    async def start(self) -> None:
        """Sunucuyu başlat."""
        # Eski socket dosyasını temizle
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)

        self._server = await asyncio.start_unix_server(
            self._handle_client,
            path=SOCKET_PATH,
        )

        # Socket dosyasına herkesin erişebilmesini sağla
        os.chmod(SOCKET_PATH, 0o777)

        logger.info(f"Listening on {SOCKET_PATH}")

    async def _handle_client(
        self,
        reader: asyncio.StreamReader,
        writer: asyncio.StreamWriter,
    ) -> None:
        """Tek bir client bağlantısını işle."""
        try:
            # İstek oku (newline ile sonlandırılmış JSON)
            data = await asyncio.wait_for(reader.readline(), timeout=5.0)
            if not data:
                return

            raw = data.decode("utf-8").strip()
            logger.debug(f"← Received: {raw}")

            # İsteği parse et ve işle
            request = Request.from_json(raw)
            response = self._simulator.handle_request(request)

            # Yanıtı gönder
            resp_json = response.to_json()
            logger.debug(f"→ Sending: {resp_json}")

            writer.write(resp_json.encode("utf-8") + b"\n")
            await writer.drain()

        except asyncio.TimeoutError:
            logger.warning("Client connection timed out")
        except Exception as e:
            logger.error(f"Error handling client: {e}")
            try:
                error_resp = make_error_response(str(e))
                writer.write(error_resp.to_json().encode("utf-8") + b"\n")
                await writer.drain()
            except Exception:
                pass
        finally:
            writer.close()
            try:
                await writer.wait_closed()
            except Exception:
                pass

    async def stop(self) -> None:
        """Sunucuyu durdur."""
        if self._server:
            self._server.close()
            await self._server.wait_closed()
        if os.path.exists(SOCKET_PATH):
            os.unlink(SOCKET_PATH)
        logger.info("Server stopped")


# ─── UART (binary protocol) ──────────────────────────────────────────────────

class _UartWriter:
    """Async write helper for binary UART responses."""

    def __init__(self, fd: int):
        self._fd = fd

    def write(self, data: bytes) -> None:
        os.write(self._fd, data)

    async def drain(self) -> None:
        await asyncio.sleep(0)


class UartServer:
    """Binary protocol on a PTY or serial device (stdlib only)."""

    def __init__(self, simulator: HardwareSimulator, port: str):
        self._simulator = simulator
        self._port = port
        self._pkt_reader = PacketReader()
        self._stop = asyncio.Event()
        self._fd: int | None = None

    def _configure_tty(self, fd: int) -> None:
        """Raw 8N1-ish mode. PTY ignores baud; skip cfsetispeed (BSD-only in Python)."""
        import termios

        attrs = termios.tcgetattr(fd)
        attrs[0] = attrs[0] & ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK
                              | termios.ISTRIP | termios.INLCR | termios.IGNCR
                              | termios.ICRNL | termios.IXON)
        attrs[1] = attrs[1] & ~termios.OPOST
        attrs[2] = attrs[2] | (termios.CS8 | termios.CLOCAL | termios.CREAD)
        attrs[2] = attrs[2] & ~(termios.PARENB | termios.CSTOPB)
        attrs[3] = attrs[3] & ~(termios.ECHO | termios.ECHONL | termios.ICANON
                              | termios.ISIG | termios.IEXTEN)
        attrs[6][termios.VMIN] = 0
        attrs[6][termios.VTIME] = 1
        termios.tcsetattr(fd, termios.TCSANOW, attrs)

    def _on_readable(self) -> None:
        if self._fd is None:
            return
        try:
            data = os.read(self._fd, 512)
        except OSError:
            self._stop.set()
            return
        if not data:
            return
        for code, payload in self._pkt_reader.feed(data):
            asyncio.create_task(
                self._simulator.handle_uart_packets(code, payload, _UartWriter(self._fd))
            )

    async def run(self) -> None:
        import fcntl

        logger.info(f"UART binary protocol on {self._port}")
        fd = os.open(self._port, os.O_RDWR | os.O_NOCTTY | os.O_SYNC)
        self._fd = fd
        flags = fcntl.fcntl(fd, fcntl.F_GETFL)
        fcntl.fcntl(fd, fcntl.F_SETFL, flags | os.O_NONBLOCK)
        self._configure_tty(fd)

        loop = asyncio.get_running_loop()
        loop.add_reader(fd, self._on_readable)
        try:
            await self._stop.wait()
        finally:
            loop.remove_reader(fd)
            os.close(fd)
            self._fd = None


# ─── Main ────────────────────────────────────────────────────────────────────

async def main(args: argparse.Namespace) -> None:
    """Ana çalıştırma döngüsü."""
    simulator = HardwareSimulator(persist_key=not args.no_persist)
    simulator.initialize()

    uart_port = args.uart or os.environ.get("BIOMETRIC_UART")

    server = SimulatorServer(simulator)
    await server.start()

    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()
    uart_server: UartServer | None = None

    if uart_port:
        uart_server = UartServer(simulator, uart_port)
        asyncio.create_task(uart_server.run())
        logger.info(f"UART binary protocol enabled on {uart_port}")

    def signal_handler():
        logger.info("\nShutdown signal received...")
        stop_event.set()
        if uart_server:
            uart_server._stop.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, signal_handler)

    logger.info("Ready — waiting for connections (Ctrl+C to stop)")
    await stop_event.wait()

    await server.stop()
    logger.info("Goodbye!")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Biometric Vault — Hardware Simulator Daemon",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python simulator.py                  Start with persistent key
  python simulator.py --no-persist     New key on every start
  python simulator.py -v               Verbose logging
        """,
    )
    parser.add_argument(
        "--no-persist",
        action="store_true",
        help="Generate new encryption key on every start (don't save to disk)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Enable verbose (DEBUG) logging",
    )
    parser.add_argument(
        "--uart",
        metavar="DEVICE",
        help="Binary UART protocol on serial device (e.g. /dev/pts/3)",
    )
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()

    # Logging ayarla
    level = logging.DEBUG if args.verbose else logging.INFO
    logging.basicConfig(level=level, format=LOG_FORMAT, datefmt=LOG_DATE_FORMAT)

    try:
        asyncio.run(main(args))
    except KeyboardInterrupt:
        pass

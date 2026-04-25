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
        """
        Parmak izi doğrula.
        Başarılıysa AES-256 key'i döndürür.

        Gerçek donanımda:
        - AS608 parmağı tarar (~0.5s)
        - Template ile karşılaştırır (~0.3s)
        - STM32 sonucu değerlendirir
        - Başarılıysa key'i UART'tan gönderir
        """
        if request.finger_id is None:
            return make_error_response("finger_id required")

        matched = self._fingerprint_db.verify(request.finger_id)

        if matched:
            self._verify_success_count += 1
            logger.info(f"✓ Finger #{request.finger_id} verified — key released")
            return make_ok_response(key=self._key_manager.key_b64)
        else:
            self._verify_fail_count += 1
            logger.warning(f"✗ Finger #{request.finger_id} — no match")
            return make_error_response("no_match")

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


# ─── Main ────────────────────────────────────────────────────────────────────

async def main(args: argparse.Namespace) -> None:
    """Ana çalıştırma döngüsü."""
    # Simülatörü oluştur ve başlat
    simulator = HardwareSimulator(persist_key=not args.no_persist)
    simulator.initialize()

    # Sunucuyu oluştur ve başlat
    server = SimulatorServer(simulator)
    await server.start()

    # Graceful shutdown handler
    loop = asyncio.get_running_loop()
    stop_event = asyncio.Event()

    def signal_handler():
        logger.info("\nShutdown signal received...")
        stop_event.set()

    for sig in (signal.SIGINT, signal.SIGTERM):
        loop.add_signal_handler(sig, signal_handler)

    logger.info("Ready — waiting for connections (Ctrl+C to stop)")

    # Çalışmaya devam et
    await stop_event.wait()

    # Temiz kapanış
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

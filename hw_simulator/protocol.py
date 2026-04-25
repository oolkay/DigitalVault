"""
Biometric Vault — Hardware Simulator Protocol Definitions

STM32 + AS608 simülatörü ile host arasındaki mesaj protokolünü tanımlar.
Gerçek donanımda UART üzerinden binary protokol kullanılacak;
simülasyonda Unix Domain Socket + JSON kullanıyoruz.
"""

import json
from enum import Enum
from dataclasses import dataclass, asdict
from typing import Optional


# ─── Socket Configuration ────────────────────────────────────────────────────

SOCKET_PATH = "/tmp/biometric_vault.sock"


# ─── Command Types ───────────────────────────────────────────────────────────

class Command(str, Enum):
    """STM32'ye gönderilebilecek komutlar."""
    ENROLL      = "enroll"       # Yeni parmak izi kaydet
    VERIFY      = "verify"       # Parmak izi doğrula → key döndür
    DELETE      = "delete"       # Kayıtlı parmak izini sil
    LIST        = "list"         # Kayıtlı parmak izlerini listele
    STATUS      = "status"       # Cihaz durumunu sorgula
    DISCONNECT  = "disconnect"   # Cihaz çekilmesini simüle et
    RECONNECT   = "reconnect"    # Cihaz tekrar bağlanmasını simüle et


# ─── Status Codes ────────────────────────────────────────────────────────────

class Status(str, Enum):
    """Yanıt durumları."""
    OK    = "ok"
    ERROR = "error"


# ─── Device States ───────────────────────────────────────────────────────────

class DeviceState(str, Enum):
    """Simüle edilen cihaz durumları."""
    CONNECTED    = "connected"
    DISCONNECTED = "disconnected"


# ─── Request / Response ─────────────────────────────────────────────────────

@dataclass
class Request:
    """Host → STM32 istek mesajı."""
    cmd: str
    finger_id: Optional[int] = None
    name: Optional[str] = None  # Enroll sırasında parmak izi adı

    def to_json(self) -> str:
        d = {k: v for k, v in asdict(self).items() if v is not None}
        return json.dumps(d)

    @classmethod
    def from_json(cls, data: str) -> "Request":
        d = json.loads(data)
        return cls(
            cmd=d.get("cmd", ""),
            finger_id=d.get("finger_id"),
            name=d.get("name"),
        )


@dataclass
class Response:
    """STM32 → Host yanıt mesajı."""
    status: str
    key: Optional[str] = None           # Base64 encoded AES key (verify başarılıysa)
    reason: Optional[str] = None        # Hata nedeni
    device_state: Optional[str] = None  # Cihaz durumu
    fingers: Optional[list] = None      # Kayıtlı parmak izi listesi
    finger_id: Optional[int] = None     # İşlem yapılan parmak izi ID'si

    def to_json(self) -> str:
        d = {k: v for k, v in asdict(self).items() if v is not None}
        return json.dumps(d)

    @classmethod
    def from_json(cls, data: str) -> "Response":
        d = json.loads(data)
        return cls(
            status=d.get("status", ""),
            key=d.get("key"),
            reason=d.get("reason"),
            device_state=d.get("device_state"),
            fingers=d.get("fingers"),
            finger_id=d.get("finger_id"),
        )


# ─── Helper Functions ────────────────────────────────────────────────────────

def make_ok_response(**kwargs) -> Response:
    """Başarılı yanıt oluştur."""
    return Response(status=Status.OK, **kwargs)


def make_error_response(reason: str) -> Response:
    """Hata yanıtı oluştur."""
    return Response(status=Status.ERROR, reason=reason)

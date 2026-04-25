"""
Biometric Vault — Encryption Key Management

STM32 üzerinde saklanan şifreleme anahtarını simüle eder.
Gerçek donanımda key STM32'nin flash belleğinde saklanır ve
asla kalıcı olarak host'a aktarılmaz.

Simülasyonda:
- İlk çalıştırmada rastgele 256-bit AES key üretilir
- Key, simülatör çalıştığı sürece bellekte tutulur
- Opsiyonel olarak diske kaydedilebilir (geliştirme kolaylığı için)
"""

import os
import base64
import hashlib
import json
from pathlib import Path

# Key dosyasının saklanacağı yer (simülasyon için)
KEY_STORE_PATH = Path(__file__).parent / ".keystore.json"

# AES-256 key boyutu (32 byte = 256 bit)
AES_KEY_SIZE = 32


class KeyManager:
    """
    STM32 üzerindeki şifreleme anahtarı yönetimini simüle eder.

    Gerçek donanımda:
    - Key, STM32 flash'ında saklanır
    - Parmak izi doğrulandığında UART üzerinden host'a gönderilir
    - Host sadece RAM'de tutar, diske yazmaz

    Simülasyonda:
    - Key rastgele üretilir veya dosyadan yüklenir
    - Persist modu açıksa restart'lar arasında korunur
    """

    def __init__(self, persist: bool = True):
        """
        Args:
            persist: True ise key diske kaydedilir (dev mode).
                     False ise her başlatmada yeni key üretilir.
        """
        self._persist = persist
        self._key: bytes | None = None
        self._key_id: str | None = None

    def initialize(self) -> None:
        """Key'i yükle veya yeni üret."""
        if self._persist and KEY_STORE_PATH.exists():
            self._load_from_disk()
        else:
            self._generate_new_key()
            if self._persist:
                self._save_to_disk()

    def _generate_new_key(self) -> None:
        """Kriptografik olarak güvenli rastgele AES-256 key üret."""
        self._key = os.urandom(AES_KEY_SIZE)
        self._key_id = hashlib.sha256(self._key).hexdigest()[:16]

    def _save_to_disk(self) -> None:
        """Key'i diske kaydet (sadece simülasyon için!)."""
        data = {
            "key_b64": base64.b64encode(self._key).decode("ascii"),
            "key_id": self._key_id,
            "warning": "SIMULATION ONLY — Real key is stored on STM32 flash"
        }
        KEY_STORE_PATH.write_text(json.dumps(data, indent=2))

    def _load_from_disk(self) -> None:
        """Key'i diskten yükle."""
        data = json.loads(KEY_STORE_PATH.read_text())
        self._key = base64.b64decode(data["key_b64"])
        self._key_id = data["key_id"]

    @property
    def key(self) -> bytes:
        """Raw AES-256 key (32 bytes)."""
        if self._key is None:
            raise RuntimeError("KeyManager not initialized. Call initialize() first.")
        return self._key

    @property
    def key_b64(self) -> str:
        """Base64 encoded key (UART üzerinden gönderilecek format)."""
        return base64.b64encode(self.key).decode("ascii")

    @property
    def key_id(self) -> str:
        """Key'in kısa hash'i (loglama için, güvenlik açısından sakıncasız)."""
        if self._key_id is None:
            raise RuntimeError("KeyManager not initialized. Call initialize() first.")
        return self._key_id

    def wipe(self) -> None:
        """
        Key'i bellekten ve diskten sil.
        STM32 çekildiğinde çağrılır — host'taki key yok edilir.
        """
        self._key = None
        self._key_id = None
        # Disk dosyasını silmiyoruz, çünkü STM32 tekrar takıldığında
        # aynı key'i vermesi gerekiyor (simülasyonda)

    def is_available(self) -> bool:
        """Key bellekte mevcut mu?"""
        return self._key is not None

    def reset(self) -> None:
        """
        Tamamen yeni bir key üret ve eskisini yok et.
        Gerçek donanımda bu, STM32'nin factory reset'ini simüle eder.
        """
        self._generate_new_key()
        if self._persist:
            self._save_to_disk()

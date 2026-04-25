"""
Biometric Vault — Simulated Fingerprint Database

AS608 sensörünün parmak izi veritabanını simüle eder.
Gerçek donanımda parmak izi template'leri AS608 modülünün
dahili flash belleğinde saklanır.

Simülasyonda:
- Her parmak izi bir ID ve isimle temsil edilir
- Doğrulama her zaman başarılı kabul edilir (veya CLI ile kontrol edilir)
- Veritabanı JSON dosyasında saklanır
"""

import json
import time
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import Optional

# Veritabanı dosya yolu
DB_PATH = Path(__file__).parent / ".fingerprint_db.json"

# AS608 modülünün maksimum parmak izi kapasitesi
MAX_FINGERPRINTS = 127


@dataclass
class Fingerprint:
    """Kayıtlı bir parmak izi kaydı."""
    finger_id: int
    name: str
    enrolled_at: str  # ISO format timestamp
    verify_count: int = 0
    last_verified_at: Optional[str] = None


class FingerprintDB:
    """
    AS608 parmak izi modülünün veritabanını simüle eder.

    Gerçek donanımda:
    - AS608 kendi dahili flash'ında template saklar
    - Template'ler cihaz dışına çıkmaz
    - Doğrulama AS608 üzerinde yapılır

    Simülasyonda:
    - Basit ID-based matching
    - Kayıtlı ID varsa "match", yoksa "no match"
    """

    def __init__(self):
        self._db: dict[int, Fingerprint] = {}

    def load(self) -> None:
        """Veritabanını diskten yükle."""
        if DB_PATH.exists():
            data = json.loads(DB_PATH.read_text())
            self._db = {
                int(k): Fingerprint(**v)
                for k, v in data.items()
            }

    def save(self) -> None:
        """Veritabanını diske kaydet."""
        data = {
            str(k): asdict(v)
            for k, v in self._db.items()
        }
        DB_PATH.write_text(json.dumps(data, indent=2))

    def enroll(self, finger_id: int, name: Optional[str] = None) -> Fingerprint:
        """
        Yeni parmak izi kaydet.

        Gerçek donanımda AS608, parmağı 2 kez tarar ve template oluşturur.
        Simülasyonda sadece ID kaydediyoruz.

        Args:
            finger_id: Parmak izi ID'si (1-127)
            name: Parmak izi adı (opsiyonel, ör. "Sağ İşaret Parmağı")

        Returns:
            Oluşturulan Fingerprint kaydı

        Raises:
            ValueError: Geçersiz ID veya kapasite doluysa
        """
        if not 1 <= finger_id <= MAX_FINGERPRINTS:
            raise ValueError(f"finger_id must be between 1 and {MAX_FINGERPRINTS}")

        if finger_id in self._db:
            raise ValueError(f"Finger ID {finger_id} already enrolled")

        if len(self._db) >= MAX_FINGERPRINTS:
            raise ValueError(f"Database full (max {MAX_FINGERPRINTS})")

        fp = Fingerprint(
            finger_id=finger_id,
            name=name or f"Finger_{finger_id}",
            enrolled_at=time.strftime("%Y-%m-%dT%H:%M:%S"),
        )
        self._db[finger_id] = fp
        self.save()
        return fp

    def verify(self, finger_id: int) -> bool:
        """
        Parmak izi doğrula.

        Gerçek donanımda AS608 sensörden okunan parmağı,
        kayıtlı template'lerle karşılaştırır ve skor döndürür.

        Simülasyonda: Kayıtlı ID varsa True, yoksa False.

        Args:
            finger_id: Doğrulanacak parmak izi ID'si

        Returns:
            True = match, False = no match
        """
        if finger_id not in self._db:
            return False

        # Doğrulama istatistiklerini güncelle
        fp = self._db[finger_id]
        fp.verify_count += 1
        fp.last_verified_at = time.strftime("%Y-%m-%dT%H:%M:%S")
        self.save()
        return True

    def delete(self, finger_id: int) -> bool:
        """
        Kayıtlı parmak izini sil.

        Args:
            finger_id: Silinecek parmak izi ID'si

        Returns:
            True = silindi, False = bulunamadı
        """
        if finger_id not in self._db:
            return False

        del self._db[finger_id]
        self.save()
        return True

    def list_all(self) -> list[dict]:
        """Tüm kayıtlı parmak izlerini listele."""
        return [
            {
                "finger_id": fp.finger_id,
                "name": fp.name,
                "enrolled_at": fp.enrolled_at,
                "verify_count": fp.verify_count,
            }
            for fp in sorted(self._db.values(), key=lambda x: x.finger_id)
        ]

    @property
    def count(self) -> int:
        """Kayıtlı parmak izi sayısı."""
        return len(self._db)

    def exists(self, finger_id: int) -> bool:
        """Belirtilen ID kayıtlı mı?"""
        return finger_id in self._db

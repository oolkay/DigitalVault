# PROJECT_CONTEXT.md
# Biometric Encryption-based Vault — Agent Reference Document

> **Bu dosya bir agent için hazırlanmıştır.**  
> Projeyi sıfırdan anlamak için tek kaynak budur. Tüm mimari, bileşenler, gereksinimler ve zaman çizelgesi burada mevcuttur.

---

## 1. Projenin Özeti (One-liner)

Standart bir USB flash sürücü üzerindeki verileri, donanım tabanlı parmak izi doğrulaması ile şifreleyen / şifre çözen güvenli bir vault (kasa) sistemi. Parmak izi sensörü doğrudan bir STM32 mikrodenetleyiciye bağlıdır; şifreleme anahtarı hiçbir zaman host bilgisayarda saklanmaz.

---

## 2. Proje Bilgileri

| Alan | Değer |
|---|---|
| Proje Adı | Biometric Encryption-based Vault Implementation |
| Ders | CSE 496 (Bitirme Projesi) |
| Öğrenci | Ömer Faruk Olkay |
| Danışman | Prof. Dr. Erkan Zergeroğlu |
| Üniversite | Gebze Technical University |
| Sunum Tarihi | Mart 2026 |

---

## 3. Temel Konsept

```
Kullanıcı parmağını sensöre koyar
        ↓
STM32 mikrodenetleyici parmak izini doğrular
        ↓
Şifreleme anahtarı UART üzerinden host'a gönderilir
        ↓
Linux device driver (C) anahtarı alır
        ↓
OpenSSL ile USB partition gerçek zamanlı şifrelenir/çözülür
        ↓
Kullanıcı verilere erişebilir (mount edilmiş filesystem)
        ↓
STM32 çıkarılırsa → otomatik unmount + kilitleme
```

---

## 4. Sistem Mimarisi (3 Katman)

### Katman 1: Application Layer
- **Python Desktop App (GUI)**
- Kullanıcıya vault işlemleri için arayüz sağlar
- IPC (Status/Control) protokolü ile OS katmanıyla iletişim kurar

### Katman 2: OS & Filesystem Layer
- **Device Driver (C)** — Linux kernel driver
- Üç sorumluluğu var:
  1. **UART (Key Request):** STM32'den şifreleme anahtarını alır
  2. **Encrypt/Decrypt:** OpenSSL aracılığıyla şifreleme/çözme işlemi yapar
  3. **Sector Read/Write:** `/dev/sdX` üzerindeki raw USB partition'a erişir
- Alt bileşenler:
  - **OpenSSL** — kriptografik işlemler
  - **Raw USB Partition (`/dev/sdX`)** — şifreli veri depolama alanı

### Katman 3: Hardware Security Layer
- **STM32F103C8T6 Mikrodenetleyici** (Bare-Metal firmware)
  - UART üzerinden host ile iletişim kurar
  - Şifreleme anahtarını güvenli şekilde barındırır
  - Fiziksel olarak çıkarıldığında vault otomatik kilitlenir
- **AS608 Optical Fingerprint Module**
  - STM32'ye UART ile bağlıdır
  - Parmak izi kaydetme ve doğrulama işlemlerini gerçekleştirir

---

## 5. Kullanım Senaryoları (Use Cases)

Aktör (kullanıcı) sisteme dört ana işlem üzerinden erişir:

| Use Case | Açıklama |
|---|---|
| **Vault Operations** | Vault'u aç (mount), kapat (unmount) |
| **Fingerprint Management** | Parmak izi kayıt etme, silme, listeleme |
| **Monitoring** | Vault durumunu ve erişim loglarını izleme |
| **File Operations** | Mount edilmiş vault içinde dosya okuma/yazma |

---

## 6. Yazılım ve Donanım Gereksinimleri

### Yazılım
- **Host OS:** Linux (zorunlu — device driver Linux kernel için)
- **OpenSSL** — şifreleme kütüphanesi
- **GUI Framework** — Python tabanlı masaüstü arayüz (PyQt veya benzeri)

### Donanım
- **Mikrodenetleyici:** STM32F103C8T6 ("Blue Pill")
- **Biyometrik Sensör:** AS608 Optical Fingerprint Module
- **Depolama:** Standart USB Flash Sürücü

---

## 7. Başarı Kriterleri (Kesin Hedefler)

Agent bu kriterleri implementasyon sırasında her zaman gözetmelidir:

| Kriter | Hedef |
|---|---|
| **Veri Gizliliği** | Vault unmount edildiğinde USB partition'daki ham veri tamamen okunamaz olmalı |
| **Biyometrik Doğrulama Hızı** | Kayıtlı parmak izi **1.5 saniye altında** doğrulanmalı |
| **False Acceptance Rate (FAR)** | **%0.001'den az** (yanlış kişiyi kabul etmemeli) |
| **Kriptografik I/O Hızı** | Gerçek zamanlı şifreleme/çözme **min. 15 MB/s** okuma ve yazma |
| **Hardware-Disconnect Lockdown** | STM32 fiziksel olarak çekildiğinde sistem **otomatik** unmount + kilitleme yapmalı |

---

## 8. Proje Zaman Çizelgesi

| Ay | Görev |
|---|---|
| **Mart 2026** | Literature Research & System Design |
| **Nisan 2026** | Hardware Integration & Driver Implementation |
| **Mayıs 2026** | Driver Implementation & Cryptography |
| **Haziran 2026** | GUI Implementation |

> **Mevcut durum (Nisan 2026 sonu):** Nisan milestone'u içinde — hardware entegrasyonu ve driver implementasyonu aşamasında.

---

## 9. Teknoloji Stack Özeti

```
┌─────────────────────────────────────────────┐
│            Python GUI (Desktop App)         │  ← Kullanıcı arayüzü
├─────────────────────────────────────────────┤
│         Linux Device Driver (C)             │  ← Kernel-level
│   OpenSSL            Raw USB (/dev/sdX)     │
├─────────────────────────────────────────────┤
│  STM32F103C8T6   ←UART→  AS608 Sensor       │  ← Donanım
└─────────────────────────────────────────────┘
```

---

## 10. Kritik Güvenlik Tasarım Kararları

1. **Anahtar host'ta saklanmaz** — Şifreleme anahtarı yalnızca STM32 üzerinde durur, host RAM'ine yalnızca aktif kullanım sırasında ve geçici olarak gelir.
2. **Hardware-bound security** — STM32 token fiziksel güvenlik anahtarı işlevi görür; çıkarılırsa veri anında erişilemez hale gelir.
3. **Bare-metal STM32 firmware** — İşletim sistemi olmadan çalışır, saldırı yüzeyi minimumdur.
4. **Raw partition şifreleme** — Dosya sistemi seviyesinde değil, sektör seviyesinde şifreleme yapılır.

---

## 11. Referanslar

- NIST FIPS 197 — Advanced Encryption Standard (AES)
- Linux Device Drivers, 3rd Edition (Corbet, Rubini, Kroah-Hartman)
- RFC 8439 — ChaCha20 and Poly1305 for IETF Protocols

---

*Bu dosya projenin CSE 496 First Presentation sunumundan (Mart 2026) türetilmiştir.*
"""
Binary UART packet codec — PC ↔ STM32 (same as hal_protocol.c)
"""

from __future__ import annotations

SYNC = 0xAA
MAX_PAYLOAD = 64

CMD_ENROLL_START = 0x01
CMD_VERIFY_START = 0x02
CMD_DELETE = 0x03
CMD_CANCEL = 0x04
CMD_GET_KEY = 0x05
CMD_LIST_ENROLLED = 0x06

EVT_WAITING_FINGER = 0x10
EVT_FINGER_DETECTED = 0x11
EVT_REMOVE_FINGER = 0x12
EVT_PLACE_AGAIN = 0x13
EVT_PROCESSING = 0x14

RES_SUCCESS = 0x20
RES_FAIL_NO_FINGER = 0x21
RES_FAIL_BAD_IMAGE = 0x22
RES_FAIL_NO_MATCH = 0x23
RES_FAIL_ALREADY_EXISTS = 0x24
RES_FAIL_SLOT_FULL = 0x25
RES_FAIL_INVALID_ID = 0x26
RES_CANCELLED = 0x27
RES_TIMEOUT = 0x28
RES_KEY_DATA = 0x29
RES_BUSY = 0x2A
RES_SENSOR_ERROR = 0x2B


def checksum(data: bytes) -> int:
    x = 0
    for b in data:
        x ^= b
    return x


def build_packet(code: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_PAYLOAD:
        raise ValueError("payload too large")
    body = bytes([SYNC, code, len(payload)]) + payload
    return body + bytes([checksum(body)])


class PacketReader:
    """Incremental packet parser."""

    def __init__(self) -> None:
        self._buf = bytearray()

    def feed(self, data: bytes) -> list[tuple[int, bytes]]:
        self._buf.extend(data)
        out: list[tuple[int, bytes]] = []
        while True:
            pkt = self._try_one()
            if pkt is None:
                break
            out.append(pkt)
        return out

    def _try_one(self) -> tuple[int, bytes] | None:
        b = self._buf
        if len(b) < 4:
            return None
        # find sync
        while b and b[0] != SYNC:
            del b[0]
        if len(b) < 4:
            return None
        code = b[1]
        plen = b[2]
        if plen > MAX_PAYLOAD:
            del b[0]
            return None
        total = 4 + plen
        if len(b) < total:
            return None
        frame = bytes(b[:total])
        if checksum(frame[:-1]) != frame[-1]:
            del b[0]
            return None
        del b[:total]
        return code, frame[3:3 + plen]

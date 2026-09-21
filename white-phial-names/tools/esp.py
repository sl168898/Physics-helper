"""Minimal checked Skyrim ESP record/subrecord helpers."""
import struct
import zlib

def records(blob):
    pos = 0
    while pos < len(blob):
        assert pos + 24 <= len(blob), "Truncated record header"
        head = blob[pos:pos + 24]
        size, = struct.unpack_from("<I", head, 4)
        if head[:4] == b"GRUP":
            assert size >= 24 and pos + size <= len(blob)
            yield from records(blob[pos + 24:pos + size])
            pos += size
        else:
            assert pos + 24 + size <= len(blob)
            data = blob[pos + 24:pos + 24 + size]
            if struct.unpack_from("<I", head, 8)[0] & 0x40000:
                expected, = struct.unpack_from("<I", data)
                data = zlib.decompress(data[4:])
                assert len(data) == expected
            yield head, data
            pos += 24 + size
    assert pos == len(blob)


def fields(data):
    pos = 0
    while pos < len(data):
        assert pos + 6 <= len(data)
        tag, size = struct.unpack_from("<4sH", data, pos)
        assert tag != b"XXXX", "Unexpected extended subrecord in known input"
        assert pos + 6 + size <= len(data)
        yield tag, data[pos + 6:pos + 6 + size]
        pos += 6 + size


def field(tag, data):
    return struct.pack("<4sH", tag, len(data)) + data


def record(tag, formid, data, flags=0, head=None):
    if head is None:
        head = struct.pack("<4sIIIIHH", tag, len(data), flags, formid, 0, 44, 0)
    else:
        head = bytearray(head)
        struct.pack_into("<I", head, 4, len(data))
        struct.pack_into("<I", head, 8, struct.unpack_from("<I", head, 8)[0] & ~0x40000)
    return bytes(head) + data


def group(tag, data):
    return struct.pack("<4sI4sIHHHH", b"GRUP", 24 + len(data), tag, 0, 0, 0, 0, 0) + data



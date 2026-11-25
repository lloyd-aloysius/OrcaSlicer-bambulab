"""Feed generated JPEG frames to the native Mac player over a loopback TLS socket."""
import argparse
import asyncio
from pathlib import Path
import ssl
import struct
import subprocess
import tempfile


async def run(probe, plugin, directory):
    cert, key, picture = (directory / name for name in ("cert.pem", "key.pem", "frame.jpg"))
    subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes", "-days", "1",
                    "-subj", "/CN=localhost", "-keyout", key, "-out", cert], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    subprocess.run(["ffmpeg", "-nostdin", "-loglevel", "error", "-f", "lavfi", "-i",
                    "testsrc2=size=320x240", "-frames:v", "1", str(picture)], check=True)
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    context.load_cert_chain(cert, key)
    frame = picture.read_bytes()
    packet = struct.pack("<IIII", len(frame), 0, 1, 0) + frame

    connections = 0

    async def stream(reader, writer):
        nonlocal connections
        connections += 1
        first = connections == 1

        try:
            header = await asyncio.wait_for(reader.readexactly(80), timeout=5)
            if header[16:48].rstrip(b"\0") != b"bblp" or header[48:].rstrip(b"\0") != b"fixture":
                return
            count = 0
            while True:
                if first and count == 8:
                    return
                count += 1
                writer.write(packet)
                await writer.drain()
                await asyncio.sleep(0.1)
        except (ConnectionError, asyncio.IncompleteReadError):
            pass
        finally:
            writer.close()

    async with await asyncio.start_server(stream, "127.0.0.1", 0, ssl=context) as server:
        port = server.sockets[0].getsockname()[1]
        url = f"bambu:///local/127.0.0.1.?port={port}&user=bblp&passwd=fixture&device=fixture"
        proc = await asyncio.create_subprocess_exec(probe, plugin, url, "--drop", cwd=directory)
        try:
            return await asyncio.wait_for(proc.wait(), 30)
        finally:
            if proc.returncode is None:
                proc.kill()
                await proc.wait()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("probe", type=Path)
    parser.add_argument("plugin", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        raise SystemExit(asyncio.run(run(args.probe.resolve(), args.plugin.resolve(), Path(tmp))))

#!/usr/bin/env python3
"""Print Talko 2 Drums serial output, sending '?' for a status report every few seconds."""
import argparse
import time

import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=float, default=10)
parser.add_argument('--every', type=float, default=2, help='seconds between status requests')
parser.add_argument('--until', help='stop early once this text arrives')
parser.add_argument('--send', help="text to send once at the start, e.g. '!' for the self-test")
args = parser.parse_args()

ports = [port for port in list_ports.comports()
         if (port.product or '').startswith('Talko 2 Drums')]
assert len(ports) == 1, 'Expected one Talko 2 Drums on USB'
with serial.Serial(ports[0].device, 115200, timeout=0.05) as link:
    end = time.monotonic() + args.seconds
    asked = 0.0
    seen = ''
    if args.send:
        link.write(args.send.encode())
    while time.monotonic() < end:
        if args.every > 0 and time.monotonic() - asked >= args.every:
            link.write(b'?')
            asked = time.monotonic()
        data = link.read(512)
        if data:
            text = data.decode(errors='replace')
            print(text, end='', flush=True)
            seen = (seen + text)[-4096:]
            if args.until and args.until in seen:
                break

#!/usr/bin/env python3
"""Print Talko 2 Drums serial output, sending '?' for a status report every few seconds."""
import argparse
import time

import serial
from serial.tools import list_ports

parser = argparse.ArgumentParser()
parser.add_argument('--seconds', type=float, default=10)
parser.add_argument('--every', type=float, default=2, help='seconds between status requests')
args = parser.parse_args()

ports = [port for port in list_ports.comports()
         if (port.product or '').startswith('Talko 2 Drums')]
assert len(ports) == 1, 'Expected one Talko 2 Drums on USB'
with serial.Serial(ports[0].device, 115200, timeout=0.05) as link:
    end = time.monotonic() + args.seconds
    asked = 0.0
    while time.monotonic() < end:
        if time.monotonic() - asked >= args.every:
            link.write(b'?')
            asked = time.monotonic()
        data = link.read(512)
        if data:
            print(data.decode(errors='replace'), end='', flush=True)

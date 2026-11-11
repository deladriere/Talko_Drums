#!/usr/bin/env python3
"""Flash Talko 2 Drums after saving the device's complete flash to rollback/."""
import argparse
import datetime
import subprocess
import time
from pathlib import Path

import serial
from serial.tools import list_ports

ROOT = Path(__file__).resolve().parents[1]
PICOTOOL = Path.home() / '.platformio/packages/tool-picotool-rp2040-earlephilhower/picotool'
parser = argparse.ArgumentParser()
parser.add_argument('--serial', required=True, help='USB serial number of the Talko')
parser.add_argument('--image', default=str(ROOT / 'Talko_Drums-0.1.uf2'))
parser.add_argument('--product', default='Talko 2 Drums 0.1', help='USB name the image reports')
parser.add_argument('--no-backup', action='store_true', help='only when the device already runs Drums')
args = parser.parse_args()
PRODUCT = args.product


def talko(product_prefix='Talko 2 '):
    return [port for port in list_ports.comports()
            if port.serial_number == args.serial and port.vid == 0x2E8A
            and (port.product or '').startswith(product_prefix)]


def pico(*command, check=True):
    result = subprocess.run([str(PICOTOOL), *command, '--ser', args.serial],
                            text=True, capture_output=True)
    if check and result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return result


ports = talko()
assert len(ports) == 1, 'Expected one Talko 2 with this serial number; no device touched'
print(f'Found {ports[0].product} on {ports[0].device}', flush=True)

with serial.Serial(ports[0].device, 1200, timeout=0.1) as link:
    link.dtr = False
for _ in range(40):
    time.sleep(0.25)
    if not pico('info', '-a', check=False).returncode:
        break
else:
    raise RuntimeError('Talko did not enter BOOTSEL')

if not args.no_backup:
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    backup = ROOT / 'rollback' / f'before-Drums-{stamp}.uf2'
    backup.parent.mkdir(exist_ok=True)
    print('Saving the complete flash to ' + str(backup.relative_to(ROOT)), flush=True)
    pico('save', '-a', str(backup))
    payload = (backup.stat().st_size // 512) * 256
    assert payload == 2 * 1024 * 1024, f'Backup holds {payload} bytes, expected 2 MB'

print(pico('load', '-v', '-x', args.image).stdout, flush=True)
for _ in range(60):
    time.sleep(0.25)
    ports = talko(PRODUCT)
    if len(ports) == 1:
        break
else:
    raise RuntimeError(PRODUCT + ' did not appear on USB')
print(f'Running {ports[0].product} on {ports[0].device}', flush=True)

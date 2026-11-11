#!/usr/bin/env python3
"""Compare bench logs: the worst and mean microseconds per 128-sample block of each voice."""
import re
import sys

PATTERN = re.compile(r'(\S+)\s+(\S+)\s+worst_us=\s*(\d+)\s+mean_us=\s*(\d+)')


def load(path):
    rows = {}
    for line in open(path):
        m = PATTERN.match(line)
        if m:
            rows[(m[1], m[2])] = (int(m[3]), int(m[4]))
    return rows


logs = [load(path) for path in sys.argv[1:]]
print('%-6s %-10s' % ('ENGINE', 'SOUND') + ''.join(' %13s' % path.split('/')[-1][:13] for path in sys.argv[1:]))
for key in logs[0]:
    print('%-6s %-10s' % key + ''.join(' %6d/%-6d' % log.get(key, (0, 0)) for log in logs))

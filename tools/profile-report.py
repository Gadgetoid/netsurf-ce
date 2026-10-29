import argparse
import bisect
import collections
import glob
import os
import struct
import subprocess

SAMPLE = struct.Struct('<12I')


def defined_symbols(nm, path):
    output = subprocess.run([nm, '-n', '--defined-only', path], capture_output=True, text=True).stdout
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[1] in 'Tt':
            yield int(fields[0], 16), fields[2]


def main():
    parser = argparse.ArgumentParser(description='Summarise a NetSurf profile.bin, weighted by UI thread CPU time')
    parser.add_argument('profile')
    parser.add_argument('--build', required=True, help='CMake build directory configured with NETSURF_CE_PROFILE=ON')
    parser.add_argument('--nm', default='llvm-nm')
    parser.add_argument('--top', type=int, default=25)
    arguments = parser.parse_args()

    symbols = list(defined_symbols(arguments.nm, os.path.join(arguments.build, 'netsurf.elf')))
    addresses = [address for address, _ in symbols]
    text_end = addresses[-1] + 0x1000
    owners = {}
    for archive in sorted(glob.glob(os.path.join(arguments.build, '*.a'))):
        library = os.path.basename(archive)[3:-2]
        for _, name in defined_symbols(arguments.nm, archive):
            owners.setdefault(name, library)

    def lookup(address):
        if address < addresses[0] or address > text_end:
            return None
        return symbols[bisect.bisect_right(addresses, address) - 1][1]

    data = open(arguments.profile, 'rb').read()
    samples = [SAMPLE.unpack_from(data, offset) for offset in range(0, len(data) - SAMPLE.size + 1, SAMPLE.size)]
    busy = [sample for sample in samples if not sample[2]]

    self_time = collections.Counter()
    library_time = collections.Counter()
    inclusive_time = collections.Counter()
    blocked = collections.Counter()
    total = 0
    for pc, lr, _, cpu, *callers in busy:
        function = lookup(pc)
        frames = [name for name in (lookup(address) for address in [lr, *callers] if address) if name]
        total += cpu
        if function is None:
            caller = frames[0] if frames else None
            self_time['[system] %s' % (caller or '?')] += cpu
            library_time['system, from %s' % owners.get(caller, '?')] += cpu
        else:
            self_time[function] += cpu
            library_time[owners.get(function, '?')] += cpu
        for name in set(([function] if function else []) + frames):
            inclusive_time[name] += cpu
        if cpu == 0:
            blocked[' < '.join([function or '[system]'] + frames[:3])] += 1

    def show(title, counter, limit):
        print('\n' + title)
        for name, value in counter.most_common(limit):
            print('%5.1f%%  %-45s %s' % (100 * value / total, name, owners.get(name, '')))

    print('%d samples, %d not waiting for input, %.0f ms UI thread CPU' % (len(samples), len(busy), total / 1e4))
    show('CPU by library', library_time, arguments.top)
    show('CPU by function (self)', self_time, arguments.top)
    show('CPU by function (inclusive, approximate)', inclusive_time, arguments.top)
    print('\nSamples with no CPU used (blocked or preempted): %d' % sum(blocked.values()))
    for chain, count in blocked.most_common(arguments.top // 2):
        print('%6d  %s' % (count, chain))


main()

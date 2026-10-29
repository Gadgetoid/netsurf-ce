import argparse
import os
import shutil
import zipfile

TIMESTAMP = (2026, 10, 29, 0, 0, 0)
FOLDER = 'アプリ/NetSurf'

RESOURCES = [
    'adblock.css', 'credits.html', 'default.css', 'internal.css', 'licence.html',
    'netsurf.png', 'quirks.css', 'welcome.html', 'favicon.png',
]

FONTS = [
    'DejaVuSans.ttf', 'DejaVuSans-Bold.ttf', 'DejaVuSans-Oblique.ttf', 'DejaVuSans-BoldOblique.ttf',
    'DejaVuSerif.ttf', 'DejaVuSerif-Bold.ttf', 'DejaVuSerif-Italic.ttf',
    'DejaVuSansMono.ttf', 'DejaVuSansMono-Bold.ttf',
]

LICENCES = {
    'netsurf': 'COPYING',
    'libcss': 'COPYING',
    'libdom': 'COPYING',
    'libhubbub': 'COPYING',
    'libnsbmp': 'COPYING',
    'libnsfb': 'COPYING',
    'libnsgif': 'COPYING',
    'libnsutils': 'COPYING',
    'libparserutils': 'COPYING',
    'libwapcaplet': 'COPYING',
    'bearssl': 'LICENSE.txt',
    'printf': 'LICENSE',
    'stb': 'LICENSE',
    'zlib': 'LICENSE',
    'freetype': 'LICENSE.TXT',
    'dejavu': 'LICENSE',
}


def contents(repository, build, sources):
    framebuffer = os.path.join(sources, 'netsurf/frontends/framebuffer/res')
    files = [
        ('ReadMe.txt', os.path.join(repository, 'card/ReadMe.txt')),
        (f'{FOLDER}/AppMain.exe', os.path.join(build, 'NetSurf.exe')),
        (f'{FOLDER}/index.din', None),
        (f'{FOLDER}/Choices', os.path.join(repository, 'card/Choices')),
        (f'{FOLDER}/Messages', os.path.join(build, 'generated/netsurf/Messages')),
        (f'{FOLDER}/licences/netsurf-ce.txt', os.path.join(repository, 'LICENSE')),
    ]
    files += [(f'{FOLDER}/{name}', os.path.join(framebuffer, name)) for name in RESOURCES]
    files += [(f'{FOLDER}/{name}', os.path.join(sources, 'dejavu/ttf', name)) for name in FONTS]
    files += [(f'{FOLDER}/licences/{name}.txt', os.path.join(sources, name, licence)) for name, licence in LICENCES.items()]
    return files


def read(path):
    if path is None:
        return b''
    with open(os.path.realpath(path), 'rb') as source:
        return source.read()


def main():
    parser = argparse.ArgumentParser(description='Package NetSurf for the SHARP Brain')
    parser.add_argument('--build', required=True)
    parser.add_argument('--sources', required=True)
    parser.add_argument('--folder', help='write the package as a folder here')
    parser.add_argument('--zip', help='write the package as a zip here')
    arguments = parser.parse_args()
    repository = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    files = [(name, read(path)) for name, path in contents(repository, arguments.build, arguments.sources)]

    if arguments.folder:
        shutil.rmtree(arguments.folder, ignore_errors=True)
        for name, data in files:
            path = os.path.join(arguments.folder, name)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            with open(path, 'wb') as output:
                output.write(data)

    if arguments.zip:
        with zipfile.ZipFile(arguments.zip, 'w') as archive:
            for name, data in files:
                entry = zipfile.ZipInfo(name, TIMESTAMP)
                entry.compress_type = zipfile.ZIP_DEFLATED
                entry.external_attr = 0o644 << 16
                archive.writestr(entry, data)


main()

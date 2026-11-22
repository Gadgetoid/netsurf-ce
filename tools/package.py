import argparse
import io
import os
import shutil
import zipfile

TIMESTAMP = (2026, 10, 29, 0, 0, 0)
FOLDER = 'NetSurf'
SHORTCUT = 'pwsh2-boot/Programs/NetSurf.lnk'
SHORTCUT_TARGET = '35#"\\Storage Card\\NetSurf\\AppMain.exe"'

RESOURCES = [
    'adblock.css', 'credits.html', 'default.css', 'internal.css', 'licence.html',
    'netsurf.png', 'quirks.css', 'welcome.html', 'favicon.png',
]

FONTS = [
    'DejaVuSans.ttf', 'DejaVuSans-Bold.ttf', 'DejaVuSans-Oblique.ttf', 'DejaVuSans-BoldOblique.ttf',
    'DejaVuSerif.ttf', 'DejaVuSerif-Bold.ttf', 'DejaVuSerif-Italic.ttf',
    'DejaVuSansMono.ttf', 'DejaVuSansMono-Bold.ttf',
]

FALLBACK_FONT = 'NotoSansJP-Regular.ttf'
FALLBACK_SOURCE = 'notosansjp/NotoSansJP[wght].ttf'
FALLBACK_WEIGHT = 400

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
    'notosansjp-ofl': 'OFL.txt',
}


def contents(repository, build, sources):
    framebuffer = os.path.join(sources, 'netsurf/frontends/framebuffer/res')
    files = [
        (f'{FOLDER}/ReadMe.txt', os.path.join(repository, 'card/ReadMe.txt')),
        (f'{FOLDER}/AppMain.exe', os.path.join(build, 'NetSurf.exe')),
        (SHORTCUT, SHORTCUT_TARGET.encode('ascii')),
        (f'{FOLDER}/Choices', os.path.join(repository, 'card/Choices')),
        (f'{FOLDER}/Messages', os.path.join(build, 'generated/netsurf/Messages')),
        (f'{FOLDER}/licences/netsurf-ce.txt', os.path.join(repository, 'LICENSE')),
    ]
    files += [(f'{FOLDER}/{name}', os.path.join(framebuffer, name)) for name in RESOURCES]
    files += [(f'{FOLDER}/{name}', os.path.join(sources, 'dejavu/ttf', name)) for name in FONTS]
    files.append((f'{FOLDER}/{FALLBACK_FONT}', static_instance(os.path.join(sources, FALLBACK_SOURCE), FALLBACK_WEIGHT)))
    files += [(f'{FOLDER}/licences/{name}.txt', os.path.join(sources, name, licence)) for name, licence in LICENCES.items()]
    return files


def static_instance(path, weight):
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    font = instantiateVariableFont(TTFont(path), {'wght': weight})
    output = io.BytesIO()
    font.save(output)
    return output.getvalue()


def read(path):
    if isinstance(path, bytes):
        return path
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

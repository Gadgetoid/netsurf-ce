import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { basename, dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const repository = join(dirname(fileURLToPath(import.meta.url)), '..');
const destination = process.argv[2] ?? join(repository, 'sources');
const sources = JSON.parse(readFileSync(join(repository, 'sources.json'), 'utf8'));

function git(directory, ...args) {
    return execFileSync('git', ['-C', directory, ...args], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'inherit'] }).trim();
}

function hasCommit(directory, commit) {
    try {
        execFileSync('git', ['-C', directory, 'cat-file', '-e', `${commit}^{commit}`], { stdio: 'ignore' });
        return true;
    } catch {
        return false;
    }
}

function fetchRepository(source, directory) {
    if (!existsSync(join(directory, '.git'))) {
        mkdirSync(directory, { recursive: true });
        git(directory, 'init', '--quiet');
    }
    if (!hasCommit(directory, source.commit)) {
        git(directory, 'fetch', '--quiet', '--depth', '1', source.url, source.commit);
    }
    git(directory, 'checkout', '--quiet', '--force', '--detach', source.commit);
    git(directory, 'clean', '--quiet', '-fdx');
    const patchDirectory = join(repository, 'patches', source.name);
    const patches = existsSync(patchDirectory) ? readdirSync(patchDirectory).filter(name => name.endsWith('.patch')).sort() : [];
    for (const patch of patches) {
        git(directory, 'apply', '--whitespace=nowarn', join(patchDirectory, patch));
    }
    return `${source.commit}${patches.length ? ` + ${patches.length} patch${patches.length > 1 ? 'es' : ''}` : ''}`;
}

async function download(source, url) {
    const response = await fetch(url);
    if (!response.ok) {
        throw new Error(`${source.name}: ${response.status} from ${url}`);
    }
    const data = Buffer.from(await response.arrayBuffer());
    const digest = createHash('sha256').update(data).digest('hex');
    if (digest !== source.sha256) {
        throw new Error(`${source.name}: SHA-256 ${digest}, expected ${source.sha256}`);
    }
    return data;
}

async function fetchDownload(source, directory) {
    const marker = join(directory, '.sha256');
    if (existsSync(marker) && readFileSync(marker, 'utf8') === source.sha256) {
        return source.sha256;
    }
    const data = await download(source, source.archive ?? source.file);
    rmSync(directory, { recursive: true, force: true });
    mkdirSync(directory, { recursive: true });
    if (source.archive) {
        execFileSync('tar', ['-x', '-f', '-', '-C', directory, '--strip-components=1'], { input: data });
    } else {
        writeFileSync(join(directory, basename(new URL(source.file).pathname)), data);
    }
    writeFileSync(marker, source.sha256);
    return source.sha256;
}

mkdirSync(destination, { recursive: true });
for (const source of sources) {
    const directory = join(destination, source.name);
    const version = source.commit ? fetchRepository(source, directory) : await fetchDownload(source, directory);
    console.log(`${source.name} ${version}`);
}

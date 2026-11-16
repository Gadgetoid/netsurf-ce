import { execFileSync } from 'node:child_process';
import { readdirSync, statSync } from 'node:fs';
import { join } from 'node:path';

const [debugTool, device, localFolder, remoteFolder] = process.argv.slice(2);
if (!remoteFolder) {
    throw new Error('Usage: install.mjs CE_DEBUG DEVICE LOCAL_FOLDER REMOTE_FOLDER');
}

function debug(...args) {
    return execFileSync(debugTool, ['--device', device, ...args], { encoding: 'utf8', stdio: ['ignore', 'pipe', 'inherit'] });
}

function makeFolder(path) {
    try {
        debug('mkdir', path);
    } catch {
    }
}

function copyFolder(local, remote) {
    makeFolder(remote);
    for (const name of readdirSync(local).sort()) {
        const localPath = join(local, name);
        const remotePath = `${remote}/${name}`;
        if (statSync(localPath).isDirectory()) {
            copyFolder(localPath, remotePath);
        } else {
            debug('put', localPath, remotePath);
            console.log(remotePath);
        }
    }
}

const parent = remoteFolder.slice(0, remoteFolder.lastIndexOf('/'));
makeFolder(parent);
copyFolder(localFolder, remoteFolder);

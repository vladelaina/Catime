const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const rootDir = path.resolve(__dirname, '..');
const downloadsDir = path.join(rootDir, 'downloads');
const manifestPath = path.join(downloadsDir, 'releases.json');
const installerPattern = /^catime_(\d+(?:\.\d+){1,3})\.exe$/i;

function compareVersions(left, right) {
    const leftParts = left.split('.').map(Number);
    const rightParts = right.split('.').map(Number);
    const length = Math.max(leftParts.length, rightParts.length);

    for (let i = 0; i < length; i += 1) {
        const diff = (leftParts[i] || 0) - (rightParts[i] || 0);
        if (diff !== 0) return diff;
    }

    return 0;
}

function sha256(filePath) {
    return crypto.createHash('sha256').update(fs.readFileSync(filePath)).digest('hex');
}

if (!fs.existsSync(downloadsDir)) {
    fs.mkdirSync(downloadsDir, { recursive: true });
}

const files = fs.readdirSync(downloadsDir)
    .map((file) => {
        const match = file.match(installerPattern);
        if (!match) return null;

        const filePath = path.join(downloadsDir, file);
        const stat = fs.statSync(filePath);
        if (!stat.isFile()) return null;

        return {
            version: match[1],
            file,
            url: file,
            size: stat.size,
            sha256: sha256(filePath),
        };
    })
    .filter(Boolean)
    .sort((left, right) => compareVersions(right.version, left.version));

const manifest = {
    latest: files[0] || null,
    files,
};

fs.writeFileSync(manifestPath, `${JSON.stringify(manifest, null, 2)}\n`);

if (manifest.latest) {
    console.log(`Download manifest updated: ${manifest.latest.file}`);
} else {
    console.log('Download manifest updated: no installers found');
}

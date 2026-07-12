#!/usr/bin/env node
/*
 * Build the deterministic CTAR container used for embedded language and font
 * resources. Only Node.js built-ins are used so cross-compilation stays a
 * host-side build step.
 */

const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');

const [, , manifestArg, resourceHeaderArg, outputBinArg, outputRcArg, auditArg] =
  process.argv;

if (!manifestArg || !resourceHeaderArg || !outputBinArg || !outputRcArg || !auditArg) {
  console.error(
    'Usage: prepare_embedded_resources.js <manifest.json> <resource.h> ' +
    '<output.bin> <output.rc> <audit.json>'
  );
  process.exit(1);
}

const CONTAINER_MAGIC = 'CTAR';
const LANGUAGE_MAGIC = 'CTLG';
const FONT_MAGIC = 'CTFT';
const FORMAT_VERSION = 1;
const CONTAINER_HEADER_SIZE = 32;
const STREAM_HEADER_SIZE = 8;
const STREAM_ENTRY_SIZE = 8;
const UINT16_MAX = 0xFFFF;
const UINT32_MAX = 0xFFFFFFFF;

const nodeMajorVersion = Number(process.versions.node.split('.')[0]);
if (!Number.isInteger(nodeMajorVersion) || nodeMajorVersion < 18) {
  fail(`Node.js 18 or newer is required; found ${process.versions.node}`);
}

const manifestPath = path.resolve(manifestArg);
const resourceHeaderPath = path.resolve(resourceHeaderArg);
const outputBinPath = path.resolve(outputBinArg);
const outputRcPath = path.resolve(outputRcArg);
const auditPath = path.resolve(auditArg);
const manifestDir = path.dirname(manifestPath);
const projectRoot = path.resolve(manifestDir, '..');

function fail(message) {
  throw new Error(message);
}

function sha256(data) {
  return crypto.createHash('sha256').update(data).digest('hex');
}

function checkedUInt16(value, label) {
  if (!Number.isInteger(value) || value < 0 || value > UINT16_MAX) {
    fail(`${label} is outside uint16 range: ${value}`);
  }
  return value;
}

function checkedUInt32(value, label) {
  if (!Number.isSafeInteger(value) || value < 0 || value > UINT32_MAX) {
    fail(`${label} is outside uint32 range: ${value}`);
  }
  return value;
}

function addUInt32(left, right, label) {
  return checkedUInt32(left + right, label);
}

function isInsideProject(filePath) {
  const relative = path.relative(projectRoot, filePath);
  return relative !== '' &&
    relative !== '..' &&
    !relative.startsWith(`..${path.sep}`) &&
    !path.isAbsolute(relative);
}

function resolveSource(source, label) {
  if (typeof source !== 'string' || source.length === 0 || path.isAbsolute(source)) {
    fail(`${label}.source must be a non-empty relative path`);
  }
  if (source.includes('\\')) {
    fail(`${label}.source must use forward slashes: ${source}`);
  }

  const filePath = path.resolve(manifestDir, source);
  if (!isInsideProject(filePath)) {
    fail(`${label}.source escapes the project root: ${source}`);
  }

  const stat = fs.statSync(filePath, { throwIfNoEntry: false });
  if (!stat || !stat.isFile()) {
    fail(`${label}.source is not a file: ${source}`);
  }

  return filePath;
}

function readManifest() {
  let manifest;
  try {
    manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  } catch (error) {
    fail(`Unable to parse ${manifestPath}: ${error.message}`);
  }

  if (!manifest || typeof manifest !== 'object' || Array.isArray(manifest)) {
    fail(`${manifestPath}: root must be an object`);
  }
  if (manifest.formatVersion !== FORMAT_VERSION) {
    fail(`${manifestPath}: unsupported formatVersion ${manifest.formatVersion}`);
  }
  if (!/^[A-Z][A-Z0-9_]*$/.test(manifest.containerResource || '')) {
    fail(`${manifestPath}: invalid containerResource`);
  }
  if (!Array.isArray(manifest.languages) || manifest.languages.length === 0) {
    fail(`${manifestPath}: languages must be a non-empty array`);
  }
  if (!Array.isArray(manifest.fonts) || manifest.fonts.length === 0) {
    fail(`${manifestPath}: fonts must be a non-empty array`);
  }

  return manifest;
}

function readResourceIds() {
  const definitions = new Map();
  const text = fs.readFileSync(resourceHeaderPath, 'utf8');
  for (const line of text.split(/\r?\n/)) {
    const match = line.match(/^\s*#define\s+([A-Z][A-Z0-9_]*)\s+([0-9]+)\b/);
    if (!match) {
      continue;
    }

    const name = match[1];
    const value = checkedUInt16(Number(match[2]), `${resourceHeaderPath}:${name}`);
    if (definitions.has(name) && definitions.get(name) !== value) {
      fail(`${resourceHeaderPath}: conflicting definitions for ${name}`);
    }
    definitions.set(name, value);
  }
  return definitions;
}

function resolveResourceId(definitions, name, label) {
  if (!/^[A-Z][A-Z0-9_]*$/.test(name || '')) {
    fail(`${label}.resource is invalid`);
  }
  if (!definitions.has(name)) {
    fail(`${label}.resource is not defined in resource.h: ${name}`);
  }
  return definitions.get(name);
}

function parseLanguage(sourcePath) {
  const sourceBuffer = fs.readFileSync(sourcePath);
  let text;
  try {
    text = new TextDecoder('utf-8', { fatal: true }).decode(sourceBuffer)
      .replace(/^\uFEFF/, '');
  } catch (error) {
    fail(`${sourcePath}: invalid UTF-8: ${error.message}`);
  }
  const entries = [];

  text.split(/\r?\n/).forEach((line, index) => {
    if (/^\s*$/.test(line) || /^\s*[#;]/.test(line) || /^\s*\[/.test(line)) {
      return;
    }

    const match = line.match(/^\s*"([^"]*)"\s*=\s*"([^"]*)"\s*$/);
    if (!match) {
      fail(`${sourcePath}:${index + 1}: expected "English Key"="Translated Value"`);
    }
    entries.push({ key: match[1], value: match[2], lineNumber: index + 1 });
  });

  if (entries.length === 0) {
    fail(`${sourcePath}: no translation entries found`);
  }

  const firstValueByKey = new Map();
  const firstLineByKey = new Map();
  for (const entry of entries) {
    if (!firstValueByKey.has(entry.key)) {
      firstValueByKey.set(entry.key, entry.value);
      firstLineByKey.set(entry.key, entry.lineNumber);
    } else if (firstValueByKey.get(entry.key) !== entry.value) {
      fail(
        `${sourcePath}:${entry.lineNumber}: duplicate key "${entry.key}" differs from ` +
        `line ${firstLineByKey.get(entry.key)}`
      );
    }
  }

  return { sourceBuffer, entries };
}

function makeAuditEntry(spec, resourceId, sourceBuffer, embeddedBuffer, extra = {}) {
  return {
    resource: spec.resource,
    resourceId,
    source: spec.source,
    ...extra,
    sourceLength: sourceBuffer.length,
    sourceSha256: sha256(sourceBuffer),
    embeddedLength: embeddedBuffer.length,
    embeddedSha256: sha256(embeddedBuffer)
  };
}

function prepareLanguages(manifest, resourceIds) {
  const prepared = manifest.languages.map((spec, index) => {
    const label = `languages[${index}]`;
    if (!spec || typeof spec !== 'object' || Array.isArray(spec)) {
      fail(`${label} must be an object`);
    }
    if (spec.mode !== 'english' && spec.mode !== 'translation') {
      fail(`${label}.mode must be "english" or "translation"`);
    }

    const sourcePath = resolveSource(spec.source, label);
    const parsed = parseLanguage(sourcePath);
    return {
      spec,
      resourceId: resolveResourceId(resourceIds, spec.resource, label),
      ...parsed
    };
  });

  const english = prepared.filter((item) => item.spec.mode === 'english');
  if (english.length !== 1) {
    fail(`${manifestPath}: exactly one language must use mode "english"`);
  }
  const englishEntries = english[0].entries;

  return prepared.map((item) => {
    if (item.entries.length !== englishEntries.length) {
      fail(
        `${item.spec.source}: ${item.entries.length} entries, ` +
        `expected ${englishEntries.length}`
      );
    }
    item.entries.forEach((entry, index) => {
      if (entry.key !== englishEntries[index].key) {
        fail(
          `${item.spec.source}:${entry.lineNumber}: key mismatch at entry ${index + 1}; ` +
          `expected "${englishEntries[index].key}", got "${entry.key}"`
        );
      }
    });

    const lines = item.spec.mode === 'english'
      ? item.entries.map((entry) => `"${entry.key}"="${entry.value}"`)
      : item.entries.map((entry) => `"${entry.value}"`);
    const embeddedBuffer = Buffer.from(`${lines.join('\n')}\n`, 'utf8');

    return {
      resourceId: item.resourceId,
      data: embeddedBuffer,
      audit: makeAuditEntry(
        item.spec,
        item.resourceId,
        item.sourceBuffer,
        embeddedBuffer,
        { mode: item.spec.mode, translationCount: item.entries.length }
      )
    };
  });
}

function prepareFonts(manifest, resourceIds) {
  return manifest.fonts.map((spec, index) => {
    const label = `fonts[${index}]`;
    if (!spec || typeof spec !== 'object' || Array.isArray(spec)) {
      fail(`${label} must be an object`);
    }

    const sourcePath = resolveSource(spec.source, label);
    const sourceBuffer = fs.readFileSync(sourcePath);
    if (sourceBuffer.length === 0) {
      fail(`${spec.source}: font file is empty`);
    }
    const resourceId = resolveResourceId(resourceIds, spec.resource, label);
    return {
      resourceId,
      data: sourceBuffer,
      audit: makeAuditEntry(spec, resourceId, sourceBuffer, sourceBuffer)
    };
  });
}

function validateUniqueIds(entries, label) {
  const seen = new Set();
  for (const entry of entries) {
    if (seen.has(entry.resourceId)) {
      fail(`${label}: duplicate resource ID ${entry.resourceId}`);
    }
    seen.add(entry.resourceId);
  }
}

function buildUncompressedStream(magic, entries) {
  if (entries.length > UINT16_MAX) {
    fail(`${magic}: too many entries`);
  }

  const directorySize = addUInt32(
    STREAM_HEADER_SIZE,
    entries.length * STREAM_ENTRY_SIZE,
    `${magic} directory size`
  );
  let totalSize = directorySize;
  for (const entry of entries) {
    checkedUInt32(entry.data.length, `${magic} entry length`);
    totalSize = addUInt32(totalSize, entry.data.length, `${magic} stream size`);
  }

  const directory = Buffer.alloc(directorySize);
  directory.write(magic, 0, 4, 'ascii');
  directory.writeUInt16LE(FORMAT_VERSION, 4);
  directory.writeUInt16LE(entries.length, 6);
  entries.forEach((entry, index) => {
    const offset = STREAM_HEADER_SIZE + index * STREAM_ENTRY_SIZE;
    directory.writeUInt16LE(entry.resourceId, offset);
    directory.writeUInt16LE(0, offset + 2);
    directory.writeUInt32LE(entry.data.length, offset + 4);
  });

  return Buffer.concat([directory, ...entries.map((entry) => entry.data)], totalSize);
}

function verifyUncompressedStream(buffer, magic, entries) {
  if (buffer.length < STREAM_HEADER_SIZE || buffer.toString('ascii', 0, 4) !== magic) {
    fail(`${magic}: invalid stream magic after inflate`);
  }
  if (buffer.readUInt16LE(4) !== FORMAT_VERSION) {
    fail(`${magic}: invalid stream version after inflate`);
  }
  if (buffer.readUInt16LE(6) !== entries.length) {
    fail(`${magic}: entry count mismatch after inflate`);
  }

  const dataOffset = STREAM_HEADER_SIZE + entries.length * STREAM_ENTRY_SIZE;
  if (dataOffset > buffer.length) {
    fail(`${magic}: directory exceeds inflated stream`);
  }

  let cursor = dataOffset;
  entries.forEach((entry, index) => {
    const offset = STREAM_HEADER_SIZE + index * STREAM_ENTRY_SIZE;
    const resourceId = buffer.readUInt16LE(offset);
    const flags = buffer.readUInt16LE(offset + 2);
    const length = buffer.readUInt32LE(offset + 4);
    if (resourceId !== entry.resourceId || flags !== 0 || length !== entry.data.length) {
      fail(`${magic}: directory entry ${index} changed during round-trip`);
    }
    if (length > buffer.length - cursor) {
      fail(`${magic}: entry ${index} exceeds inflated stream`);
    }

    const decoded = buffer.subarray(cursor, cursor + length);
    if (sha256(decoded) !== entry.audit.embeddedSha256) {
      fail(`${magic}: SHA-256 mismatch for resource ${resourceId}`);
    }
    cursor += length;
  });

  if (cursor !== buffer.length) {
    fail(`${magic}: trailing bytes after final entry`);
  }
}

/*
 * These profiles were exhaustively selected for the two stable input classes
 * with the Node/zlib versions pinned by the release workflow. The manifest
 * entry order is compression-tuned as well. Both still emit ordinary zlib
 * streams; only input ordering and encoder search/memory settings differ.
 */
const compressionProfiles = new Map([
  [LANGUAGE_MAGIC, Object.freeze({
    level: 7,
    windowBits: 15,
    memLevel: 5,
    strategy: zlib.constants.Z_DEFAULT_STRATEGY
  })],
  [FONT_MAGIC, Object.freeze({
    level: 9,
    windowBits: 15,
    memLevel: 6,
    strategy: zlib.constants.Z_DEFAULT_STRATEGY
  })]
]);

function compressionAuditOptions(options) {
  return {
    level: options.level,
    windowBits: options.windowBits,
    memLevel: options.memLevel,
    strategy: 'Z_DEFAULT_STRATEGY'
  };
}

function compressAndVerify(magic, entries) {
  const uncompressed = buildUncompressedStream(magic, entries);
  const compressionOptions = compressionProfiles.get(magic);
  if (!compressionOptions) {
    fail(`${magic}: no compression profile configured`);
  }
  const compressed = zlib.deflateSync(uncompressed, compressionOptions);
  const result = zlib.inflateSync(compressed, { info: true });
  if (result.engine.bytesWritten !== compressed.length) {
    fail(`${magic}: inflater did not consume the complete zlib stream`);
  }
  if (!result.buffer.equals(uncompressed)) {
    fail(`${magic}: inflated bytes differ from the source stream`);
  }
  verifyUncompressedStream(result.buffer, magic, entries);
  return { uncompressed, compressed, compressionOptions };
}

function buildAndVerifyContainer(languageStream, fontStream, languageEntries, fontEntries) {
  const containerSize = addUInt32(
    addUInt32(CONTAINER_HEADER_SIZE, languageStream.compressed.length, 'container size'),
    fontStream.compressed.length,
    'container size'
  );
  const header = Buffer.alloc(CONTAINER_HEADER_SIZE);
  header.write(CONTAINER_MAGIC, 0, 4, 'ascii');
  header.writeUInt16LE(FORMAT_VERSION, 4);
  header.writeUInt16LE(CONTAINER_HEADER_SIZE, 6);
  header.writeUInt32LE(containerSize, 8);
  header.writeUInt32LE(languageStream.compressed.length, 12);
  header.writeUInt32LE(languageStream.uncompressed.length, 16);
  header.writeUInt32LE(fontStream.compressed.length, 20);
  header.writeUInt32LE(fontStream.uncompressed.length, 24);
  header.writeUInt32LE(0, 28);

  const container = Buffer.concat(
    [header, languageStream.compressed, fontStream.compressed],
    containerSize
  );

  if (container.toString('ascii', 0, 4) !== CONTAINER_MAGIC ||
      container.readUInt16LE(4) !== FORMAT_VERSION ||
      container.readUInt16LE(6) !== CONTAINER_HEADER_SIZE ||
      container.readUInt32LE(8) !== container.length ||
      container.readUInt32LE(28) !== 0) {
    fail('CTAR header verification failed');
  }

  const languageEnd = CONTAINER_HEADER_SIZE + container.readUInt32LE(12);
  const fontEnd = languageEnd + container.readUInt32LE(20);
  if (fontEnd !== container.length) {
    fail('CTAR compressed stream bounds are invalid');
  }

  const languageCompressed = container.subarray(CONTAINER_HEADER_SIZE, languageEnd);
  const fontCompressed = container.subarray(languageEnd, fontEnd);
  const languageDecoded = zlib.inflateSync(languageCompressed, { info: true });
  const fontDecoded = zlib.inflateSync(fontCompressed, { info: true });
  if (languageDecoded.engine.bytesWritten !== languageCompressed.length ||
      fontDecoded.engine.bytesWritten !== fontCompressed.length) {
    fail('CTAR verification left unconsumed compressed bytes');
  }
  if (languageDecoded.buffer.length !== container.readUInt32LE(16) ||
      fontDecoded.buffer.length !== container.readUInt32LE(24)) {
    fail('CTAR uncompressed length verification failed');
  }
  verifyUncompressedStream(languageDecoded.buffer, LANGUAGE_MAGIC, languageEntries);
  verifyUncompressedStream(fontDecoded.buffer, FONT_MAGIC, fontEntries);
  return container;
}

function writeIfChanged(filePath, data) {
  const buffer = Buffer.isBuffer(data) ? data : Buffer.from(data, 'utf8');
  if (fs.existsSync(filePath)) {
    const existing = fs.readFileSync(filePath);
    if (existing.equals(buffer)) {
      return;
    }
  }

  const temporaryPath =
    `${filePath}.tmp-${process.pid}-${crypto.randomBytes(6).toString('hex')}`;
  try {
    fs.writeFileSync(temporaryPath, buffer);
    fs.renameSync(temporaryPath, filePath);
  } finally {
    if (fs.existsSync(temporaryPath)) {
      fs.unlinkSync(temporaryPath);
    }
  }
}

function ensureOutputDirectory(filePath) {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
}

const manifest = readManifest();
const resourceIds = readResourceIds();
const containerResourceId = resolveResourceId(
  resourceIds,
  manifest.containerResource,
  'containerResource'
);
const languageEntries = prepareLanguages(manifest, resourceIds);
const fontEntries = prepareFonts(manifest, resourceIds);
validateUniqueIds(languageEntries, 'languages');
validateUniqueIds(fontEntries, 'fonts');
if (languageEntries.some((entry) => entry.resourceId === containerResourceId)) {
  fail('container resource ID conflicts with a language resource ID');
}

const languageStream = compressAndVerify(LANGUAGE_MAGIC, languageEntries);
const fontStream = compressAndVerify(FONT_MAGIC, fontEntries);
const container = buildAndVerifyContainer(
  languageStream,
  fontStream,
  languageEntries,
  fontEntries
);

const outputPaths = [outputBinPath, outputRcPath, auditPath];
if (new Set(outputPaths.map((filePath) => filePath.toLowerCase())).size !==
    outputPaths.length) {
  fail('output.bin, output.rc, and audit.json must use distinct paths');
}

if (path.dirname(outputBinPath) !== path.dirname(outputRcPath)) {
  fail('output.bin and output.rc must be in the same directory');
}

const containerHash = sha256(container);

const rcText = [
  '#include <windows.h>',
  '#include "resource.h"',
  '',
  `/* CTAR SHA-256: ${containerHash} */`,
  `${manifest.containerResource} RCDATA "${path.basename(outputBinPath)}"`,
  ''
].join('\n');

const audit = {
  formatVersion: FORMAT_VERSION,
  container: {
    magic: CONTAINER_MAGIC,
    resource: manifest.containerResource,
    resourceId: containerResourceId,
    length: container.length,
    sha256: containerHash
  },
  compression: {
    format: 'zlib',
    nodeVersion: process.versions.node,
    zlibVersion: process.versions.zlib,
    profiles: {
      languages: compressionAuditOptions(languageStream.compressionOptions),
      fonts: compressionAuditOptions(fontStream.compressionOptions)
    }
  },
  streams: [
    {
      kind: 'languages',
      magic: LANGUAGE_MAGIC,
      uncompressedLength: languageStream.uncompressed.length,
      compressedLength: languageStream.compressed.length,
      uncompressedSha256: sha256(languageStream.uncompressed),
      compressedSha256: sha256(languageStream.compressed),
      entries: languageEntries.map((entry) => entry.audit)
    },
    {
      kind: 'fonts',
      magic: FONT_MAGIC,
      uncompressedLength: fontStream.uncompressed.length,
      compressedLength: fontStream.compressed.length,
      uncompressedSha256: sha256(fontStream.uncompressed),
      compressedSha256: sha256(fontStream.compressed),
      entries: fontEntries.map((entry) => entry.audit)
    }
  ]
};

ensureOutputDirectory(outputBinPath);
ensureOutputDirectory(outputRcPath);
ensureOutputDirectory(auditPath);
writeIfChanged(outputBinPath, container);
writeIfChanged(outputRcPath, rcText);
writeIfChanged(auditPath, `${JSON.stringify(audit, null, 2)}\n`);

const sourceBytes = languageEntries.reduce(
  (total, entry) => total + entry.audit.sourceLength,
  0
) + fontEntries.reduce((total, entry) => total + entry.audit.sourceLength, 0);
console.log(
  `Prepared ${languageEntries.length} languages and ${fontEntries.length} fonts: ` +
  `${sourceBytes} source bytes -> ${container.length} container bytes ` +
  `(saved ${sourceBytes - container.length})`
);

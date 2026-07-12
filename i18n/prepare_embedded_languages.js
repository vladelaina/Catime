#!/usr/bin/env node
/*
 * Generate compact language resources for embedding in the executable.
 * Source files remain key=value for validation and translator maintenance.
 */

const fs = require('node:fs');
const path = require('node:path');

const [, , templateArg, outputDirArg, outputRcArg] = process.argv;

if (!templateArg || !outputDirArg || !outputRcArg) {
  console.error(
    'Usage: prepare_embedded_languages.js <languages.rc> <output-dir> <output.rc>'
  );
  process.exit(1);
}

const templatePath = path.resolve(templateArg);
const sourceDir = path.join(path.dirname(templatePath), 'languages');
const resourceHeaderPath = path.join(path.dirname(templatePath), 'resource.h');
const outputDir = path.resolve(outputDirArg);
const outputRcPath = path.resolve(outputRcArg);

function fail(message) {
  throw new Error(message);
}

function toRcPath(filePath) {
  return path.resolve(filePath).replace(/\\/g, '/');
}

function writeFile(filePath, content, encoding = 'utf8') {
  fs.mkdirSync(path.dirname(filePath), { recursive: true });
  fs.writeFileSync(filePath, content, encoding);
}

function parseLanguageFile(fileName) {
  const filePath = path.join(sourceDir, fileName);
  if (!fs.existsSync(filePath)) {
    fail(`Missing language file: ${filePath}`);
  }

  const text = fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, '');
  const entries = [];

  text.split(/\r?\n/).forEach((line, index) => {
    if (/^\s*$/.test(line) || /^\s*[#;]/.test(line) || /^\s*\[/.test(line)) {
      return;
    }

    const match = line.match(/^\s*"([^"]*)"\s*=\s*"([^"]*)"\s*$/);
    if (!match) {
      fail(`${filePath}:${index + 1}: expected "English Key"="Translated Value"`);
    }

    entries.push({ key: match[1], value: match[2], lineNumber: index + 1 });
  });

  if (entries.length === 0) {
    fail(`${filePath}: no translation entries found`);
  }

  const firstValueByKey = new Map();
  const firstLineByKey = new Map();
  for (const entry of entries) {
    if (!firstValueByKey.has(entry.key)) {
      firstValueByKey.set(entry.key, entry.value);
      firstLineByKey.set(entry.key, entry.lineNumber);
    } else if (firstValueByKey.get(entry.key) !== entry.value) {
      fail(
        `${filePath}:${entry.lineNumber}: duplicate key "${entry.key}" differs from ` +
        `line ${firstLineByKey.get(entry.key)}`
      );
    }
  }

  return { filePath, entries };
}

const template = fs.readFileSync(templatePath, 'utf8');
const templateLines = template.split(/\r?\n/);
const resourcePattern = /^(\s*[A-Z0-9_]+\s+RCDATA\s+)"languages\/([^"]+)"\s*$/;
const resourceFiles = [];

for (const line of templateLines) {
  const match = line.match(resourcePattern);
  if (!match) {
    continue;
  }

  const fileName = match[2];
  if (path.basename(fileName) !== fileName || resourceFiles.includes(fileName)) {
    fail(`Invalid or duplicate language resource path: ${fileName}`);
  }
  resourceFiles.push(fileName);
}

if (!resourceFiles.includes('en.ini')) {
  fail(`${templatePath}: missing en.ini resource entry`);
}

fs.mkdirSync(outputDir, { recursive: true });

const english = parseLanguageFile('en.ini').entries;
let sourceBytes = 0;
let embeddedBytes = 0;

for (const fileName of resourceFiles) {
  const parsed = parseLanguageFile(fileName);
  if (parsed.entries.length !== english.length) {
    fail(
      `${parsed.filePath}: ${parsed.entries.length} entries, expected ${english.length}`
    );
  }

  parsed.entries.forEach((entry, index) => {
    if (entry.key !== english[index].key) {
      fail(
        `${parsed.filePath}:${entry.lineNumber}: key mismatch at entry ${index + 1}; ` +
        `expected "${english[index].key}", got "${entry.key}"`
      );
    }
  });

  const compactLines = fileName === 'en.ini'
    ? parsed.entries.map((entry) => `"${entry.key}"="${entry.value}"`)
    : parsed.entries.map((entry) => `"${entry.value}"`);
  const compactText = `${compactLines.join('\n')}\n`;
  const outputPath = path.join(outputDir, fileName);

  writeFile(outputPath, compactText);
  sourceBytes += fs.statSync(parsed.filePath).size;
  embeddedBytes += Buffer.byteLength(compactText, 'utf8');
}

for (const entry of fs.readdirSync(outputDir, { withFileTypes: true })) {
  if (entry.isFile() && entry.name.endsWith('.ini') && !resourceFiles.includes(entry.name)) {
    fs.unlinkSync(path.join(outputDir, entry.name));
  }
}

const generatedRc = templateLines.map((line) => {
  if (/^\s*#include\s+"resource\.h"\s*$/.test(line)) {
    return `#include "${toRcPath(resourceHeaderPath)}"`;
  }

  const match = line.match(resourcePattern);
  if (!match) {
    return line;
  }

  return `${match[1]}"${toRcPath(path.join(outputDir, match[2]))}"`;
}).join('\n');

writeFile(outputRcPath, generatedRc, 'utf8');

console.log(
  `Prepared ${resourceFiles.length} embedded languages (${english.length} keys): ` +
  `${sourceBytes} -> ${embeddedBytes} bytes, saved ${sourceBytes - embeddedBytes}`
);

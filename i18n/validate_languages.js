#!/usr/bin/env node
/*
 * Validate Catime language resources.
 *
 * en.ini is the translation key template. Every other language file must use
 * the same key=value format and the same key sequence. This prevents the old
 * compact value format from shifting translations when the English template is
 * edited.
 */

const fs = require('node:fs');
const path = require('node:path');

const languageDir = process.argv[2]
  ? path.resolve(process.argv[2])
  : path.resolve(__dirname, '..', 'resource', 'languages');

function isSkippedLine(line) {
  return /^\s*$/.test(line) || /^\s*[#;]/.test(line) || /^\s*\[/.test(line);
}

function parseKeyValueLine(line) {
  const match = line.match(/^\s*"([^"]*)"\s*=\s*"([^"]*)"\s*$/);
  if (!match) {
    return null;
  }

  return {
    key: match[1],
    value: match[2],
  };
}

function parseLanguageFile(filePath, requireKeyValue) {
  const text = fs.readFileSync(filePath, 'utf8').replace(/^\uFEFF/, '');
  const entries = [];
  const errors = [];

  text.split(/\r?\n/).forEach((line, index) => {
    if (isSkippedLine(line)) {
      return;
    }

    const entry = parseKeyValueLine(line);
    if (!entry) {
      const lineNumber = index + 1;
      const hint = requireKeyValue
        ? 'expected "English Key"="Translated Value"'
        : 'expected key=value';
      errors.push(`${filePath}:${lineNumber}: ${hint}: ${line}`);
      return;
    }

    entries.push({
      key: entry.key,
      value: entry.value,
      lineNumber: index + 1,
    });
  });

  return { entries, errors };
}

function fail(errors) {
  for (const error of errors) {
    console.error(error);
  }
  process.exit(1);
}

if (!fs.existsSync(languageDir)) {
  fail([`Language directory does not exist: ${languageDir}`]);
}

const files = fs.readdirSync(languageDir)
  .filter((name) => name.endsWith('.ini'))
  .sort();

if (!files.includes('en.ini')) {
  fail([`Missing language template: ${path.join(languageDir, 'en.ini')}`]);
}

const errors = [];
const englishPath = path.join(languageDir, 'en.ini');
const english = parseLanguageFile(englishPath, true);
errors.push(...english.errors);

if (english.entries.length === 0) {
  errors.push(`${englishPath}: no translation keys found`);
}

function validateDuplicateValues(filePath, entries) {
  const firstValueByKey = new Map();
  const firstLineByKey = new Map();

  for (const entry of entries) {
    if (!firstValueByKey.has(entry.key)) {
      firstValueByKey.set(entry.key, entry.value);
      firstLineByKey.set(entry.key, entry.lineNumber);
      continue;
    }

    if (firstValueByKey.get(entry.key) !== entry.value) {
      errors.push(
        `${filePath}:${entry.lineNumber}: duplicate key "${entry.key}" has a different value; ` +
        `first value is at line ${firstLineByKey.get(entry.key)}`
      );
    }
  }
}

validateDuplicateValues(englishPath, english.entries);

for (const file of files) {
  if (file === 'en.ini') {
    continue;
  }

  const filePath = path.join(languageDir, file);
  const language = parseLanguageFile(filePath, true);
  errors.push(...language.errors);
  validateDuplicateValues(filePath, language.entries);

  if (language.entries.length !== english.entries.length) {
    errors.push(
      `${filePath}: ${language.entries.length} entries, expected ${english.entries.length}`
    );
  }

  const count = Math.min(language.entries.length, english.entries.length);
  for (let i = 0; i < count; i++) {
    const actual = language.entries[i];
    const expected = english.entries[i];
    if (actual.key !== expected.key) {
      errors.push(
        `${filePath}:${actual.lineNumber}: key mismatch at entry ${i + 1}; ` +
        `expected "${expected.key}", got "${actual.key}"`
      );
    }
  }
}

if (errors.length > 0) {
  fail(errors);
}

console.log(`Validated ${files.length} language files (${english.entries.length} keys).`);

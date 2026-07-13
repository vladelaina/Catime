#!/usr/bin/env node
/* Losslessly recompress PNG IDAT streams using the smallest built-in zlib profile. */

const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');

const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);
const inputPaths = process.argv.slice(2);

if (inputPaths.length === 0) {
  throw new Error('Usage: node tools/optimize_png.js <png> [png ...]');
}

function crc32(buffer) {
  let crc = 0xffffffff;
  for (const byte of buffer) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) {
      crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function makeChunk(type, data) {
  const typeBuffer = Buffer.from(type, 'ascii');
  const chunk = Buffer.alloc(12 + data.length);
  chunk.writeUInt32BE(data.length, 0);
  typeBuffer.copy(chunk, 4);
  data.copy(chunk, 8);
  chunk.writeUInt32BE(crc32(Buffer.concat([typeBuffer, data])), 8 + data.length);
  return chunk;
}

function findSmallestDeflate(scanlines) {
  let best = null;

  for (let level = 1; level <= 9; level += 1) {
    for (let windowBits = 9; windowBits <= 15; windowBits += 1) {
      for (let memLevel = 1; memLevel <= 9; memLevel += 1) {
        for (const strategy of [
          zlib.constants.Z_DEFAULT_STRATEGY,
          zlib.constants.Z_FILTERED,
          zlib.constants.Z_HUFFMAN_ONLY,
          zlib.constants.Z_RLE,
          zlib.constants.Z_FIXED,
        ]) {
          const compressed = zlib.deflateSync(scanlines, {
            level,
            windowBits,
            memLevel,
            strategy,
          });
          if (!best || compressed.length < best.compressed.length) {
            best = { compressed, level, windowBits, memLevel, strategy };
          }
        }
      }
    }
  }

  return best;
}

function optimizePng(inputPath) {
  const absolutePath = path.resolve(inputPath);
  const png = fs.readFileSync(absolutePath);
  if (png.length < PNG_SIGNATURE.length ||
      !png.subarray(0, PNG_SIGNATURE.length).equals(PNG_SIGNATURE)) {
    throw new Error(`${absolutePath}: invalid PNG signature`);
  }

  const chunks = [];
  const idatParts = [];
  let offset = PNG_SIGNATURE.length;
  let sawIend = false;

  while (offset < png.length) {
    if (offset + 12 > png.length) {
      throw new Error(`${absolutePath}: truncated PNG chunk`);
    }
    const length = png.readUInt32BE(offset);
    const end = offset + 12 + length;
    if (end > png.length) {
      throw new Error(`${absolutePath}: PNG chunk exceeds file bounds`);
    }

    const type = png.toString('ascii', offset + 4, offset + 8);
    const data = Buffer.from(png.subarray(offset + 8, offset + 8 + length));
    const storedCrc = png.readUInt32BE(offset + 8 + length);
    const actualCrc = crc32(png.subarray(offset + 4, offset + 8 + length));
    if (storedCrc !== actualCrc) {
      throw new Error(`${absolutePath}: invalid ${type} CRC`);
    }

    chunks.push({ type, data });
    if (type === 'IDAT') idatParts.push(data);
    if (type === 'IEND') sawIend = true;
    offset = end;
  }

  if (!sawIend || offset !== png.length || idatParts.length === 0) {
    throw new Error(`${absolutePath}: missing required PNG chunks`);
  }

  const originalIdat = Buffer.concat(idatParts);
  const scanlines = zlib.inflateSync(originalIdat);
  const best = findSmallestDeflate(scanlines);
  if (!zlib.inflateSync(best.compressed).equals(scanlines)) {
    throw new Error(`${absolutePath}: recompressed IDAT changed scanline data`);
  }

  const rebuiltChunks = [];
  let wroteIdat = false;
  for (const chunk of chunks) {
    if (chunk.type === 'IDAT') {
      if (!wroteIdat) {
        rebuiltChunks.push(makeChunk('IDAT', best.compressed));
        wroteIdat = true;
      }
    } else {
      rebuiltChunks.push(makeChunk(chunk.type, chunk.data));
    }
  }

  const optimized = Buffer.concat([PNG_SIGNATURE, ...rebuiltChunks]);
  if (optimized.length > png.length) {
    throw new Error(`${absolutePath}: recompression increased PNG size`);
  }
  if (!optimized.equals(png)) {
    fs.writeFileSync(absolutePath, optimized);
  }

  console.log(
    `Optimized ${absolutePath}: ${png.length} -> ${optimized.length} bytes; ` +
    `zlib level=${best.level}, windowBits=${best.windowBits}, ` +
    `memLevel=${best.memLevel}, strategy=${best.strategy}`
  );
}

for (const inputPath of inputPaths) {
  optimizePng(inputPath);
}

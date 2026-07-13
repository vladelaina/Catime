#!/usr/bin/env node
/* Losslessly recompress the PNG payload in Catime's single-image ICO. */

const crypto = require('node:crypto');
const fs = require('node:fs');
const path = require('node:path');
const zlib = require('node:zlib');

const iconPath = path.resolve(process.argv[2] || 'asset/icon/catime.ico');
const PNG_SIGNATURE = Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]);

function fail(message) {
  throw new Error(message);
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

function paeth(left, above, upperLeft) {
  const estimate = left + above - upperLeft;
  const leftDistance = Math.abs(estimate - left);
  const aboveDistance = Math.abs(estimate - above);
  const upperLeftDistance = Math.abs(estimate - upperLeft);
  if (leftDistance <= aboveDistance && leftDistance <= upperLeftDistance) return left;
  return aboveDistance <= upperLeftDistance ? above : upperLeft;
}

function decodeRgba(scanlines, width, height) {
  const bytesPerPixel = 4;
  const rowBytes = width * bytesPerPixel;
  if (scanlines.length !== height * (rowBytes + 1)) {
    fail(`Unexpected PNG scanline size: ${scanlines.length}`);
  }

  const rgba = Buffer.alloc(width * height * bytesPerPixel);
  let inputOffset = 0;
  for (let y = 0; y < height; y += 1) {
    const filter = scanlines[inputOffset];
    inputOffset += 1;
    const rowOffset = y * rowBytes;
    const previousRowOffset = rowOffset - rowBytes;

    for (let x = 0; x < rowBytes; x += 1) {
      const encoded = scanlines[inputOffset + x];
      const left = x >= bytesPerPixel ? rgba[rowOffset + x - bytesPerPixel] : 0;
      const above = y > 0 ? rgba[previousRowOffset + x] : 0;
      const upperLeft = y > 0 && x >= bytesPerPixel
        ? rgba[previousRowOffset + x - bytesPerPixel]
        : 0;
      let predictor = 0;

      if (filter === 1) predictor = left;
      else if (filter === 2) predictor = above;
      else if (filter === 3) predictor = Math.floor((left + above) / 2);
      else if (filter === 4) predictor = paeth(left, above, upperLeft);
      else if (filter !== 0) fail(`Unsupported PNG filter: ${filter}`);

      rgba[rowOffset + x] = (encoded + predictor) & 0xff;
    }
    inputOffset += rowBytes;
  }
  return rgba;
}

function encodeRgbaWithZeroOptimizedFilters(rgba, width, height) {
  const bytesPerPixel = 4;
  const rowBytes = width * bytesPerPixel;
  const scanlines = Buffer.alloc(height * (rowBytes + 1));
  let outputOffset = 0;

  for (let y = 0; y < height; y += 1) {
    const rowOffset = y * rowBytes;
    const previousRowOffset = rowOffset - rowBytes;
    let bestFilter = 0;
    let bestZeroCount = -1;
    let bestRow = null;

    for (let filter = 0; filter <= 4; filter += 1) {
      const filteredRow = Buffer.allocUnsafe(rowBytes);
      let zeroCount = 0;

      for (let x = 0; x < rowBytes; x += 1) {
        const value = rgba[rowOffset + x];
        const left = x >= bytesPerPixel ? rgba[rowOffset + x - bytesPerPixel] : 0;
        const above = y > 0 ? rgba[previousRowOffset + x] : 0;
        const upperLeft = y > 0 && x >= bytesPerPixel
          ? rgba[previousRowOffset + x - bytesPerPixel]
          : 0;
        let predictor = 0;

        if (filter === 1) predictor = left;
        else if (filter === 2) predictor = above;
        else if (filter === 3) predictor = Math.floor((left + above) / 2);
        else if (filter === 4) predictor = paeth(left, above, upperLeft);

        const encoded = (value - predictor) & 0xff;
        filteredRow[x] = encoded;
        if (encoded === 0) zeroCount += 1;
      }

      if (zeroCount > bestZeroCount) {
        bestFilter = filter;
        bestZeroCount = zeroCount;
        bestRow = filteredRow;
      }
    }

    scanlines[outputOffset] = bestFilter;
    outputOffset += 1;
    bestRow.copy(scanlines, outputOffset);
    outputOffset += rowBytes;
  }

  return scanlines;
}

const ico = fs.readFileSync(iconPath);
if (ico.length < 22 || ico.readUInt16LE(0) !== 0 ||
    ico.readUInt16LE(2) !== 1 || ico.readUInt16LE(4) !== 1) {
  fail('Expected a single-image ICO file');
}

const imageSize = ico.readUInt32LE(14);
const imageOffset = ico.readUInt32LE(18);
if (imageOffset + imageSize > ico.length || imageOffset < 22) {
  fail('ICO image range is invalid');
}

const png = ico.subarray(imageOffset, imageOffset + imageSize);
if (!png.subarray(0, PNG_SIGNATURE.length).equals(PNG_SIGNATURE)) {
  fail('ICO image is not a PNG');
}

const chunks = [];
const idatParts = [];
let ihdr = null;
let offset = PNG_SIGNATURE.length;
let sawIend = false;
while (offset < png.length) {
  if (offset + 12 > png.length) fail('Truncated PNG chunk');
  const length = png.readUInt32BE(offset);
  const end = offset + 12 + length;
  if (end > png.length) fail('PNG chunk exceeds image bounds');

  const type = png.toString('ascii', offset + 4, offset + 8);
  const data = png.subarray(offset + 8, offset + 8 + length);
  const storedCrc = png.readUInt32BE(offset + 8 + length);
  const actualCrc = crc32(png.subarray(offset + 4, offset + 8 + length));
  if (storedCrc !== actualCrc) fail(`Invalid ${type} CRC`);

  chunks.push({ type, data: Buffer.from(data) });
  if (type === 'IHDR') ihdr = Buffer.from(data);
  if (type === 'IDAT') idatParts.push(Buffer.from(data));
  if (type === 'IEND') sawIend = true;
  offset = end;
}

if (!sawIend || offset !== png.length || !ihdr || idatParts.length === 0) {
  fail('PNG is missing required chunks');
}
if (ihdr.length !== 13 || ihdr[8] !== 8 || ihdr[9] !== 6 ||
    ihdr[10] !== 0 || ihdr[11] !== 0 || ihdr[12] !== 0) {
  fail('Expected a non-interlaced 8-bit RGBA PNG');
}

const originalIdat = Buffer.concat(idatParts);
const originalScanlines = zlib.inflateSync(originalIdat);
const width = ihdr.readUInt32BE(0);
const height = ihdr.readUInt32BE(4);
const rgba = decodeRgba(originalScanlines, width, height);
const optimizedScanlines = encodeRgbaWithZeroOptimizedFilters(rgba, width, height);
const optimizedIdat = zlib.deflateSync(optimizedScanlines, {
  level: 9,
  windowBits: 15,
  memLevel: 8,
  strategy: zlib.constants.Z_RLE,
});
const verifiedScanlines = zlib.inflateSync(optimizedIdat);
if (!verifiedScanlines.equals(optimizedScanlines) ||
    !decodeRgba(verifiedScanlines, width, height).equals(rgba)) {
  fail('Recompressed IDAT does not reproduce the original pixels');
}

const rgbaHash = crypto.createHash('sha256').update(rgba).digest('hex');

const rebuiltChunks = [];
let wroteIdat = false;
for (const chunk of chunks) {
  if (chunk.type === 'IDAT') {
    if (!wroteIdat) {
      rebuiltChunks.push(makeChunk('IDAT', optimizedIdat));
      wroteIdat = true;
    }
  } else {
    rebuiltChunks.push(makeChunk(chunk.type, chunk.data));
  }
}

const optimizedPng = Buffer.concat([PNG_SIGNATURE, ...rebuiltChunks]);
const optimizedIco = Buffer.alloc(22 + optimizedPng.length);
ico.copy(optimizedIco, 0, 0, 22);
optimizedIco.writeUInt32LE(optimizedPng.length, 14);
optimizedIco.writeUInt32LE(22, 18);
optimizedPng.copy(optimizedIco, 22);

if (optimizedIco.length > ico.length) {
  fail(`Recompression would increase the ICO size: ${ico.length} -> ${optimizedIco.length}`);
}
if (!optimizedIco.equals(ico)) {
  fs.writeFileSync(iconPath, optimizedIco);
}

console.log(
  `Optimized ${iconPath}: ${ico.length} -> ${optimizedIco.length} bytes; ` +
  `RGBA SHA-256 ${rgbaHash}`
);

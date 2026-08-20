import { access, readFile } from 'node:fs/promises';
import { dirname, extname, resolve } from 'node:path';

const ROOT = resolve(import.meta.dirname, '..');
const errors = [];
const checkedFiles = new Set();

const htmlPages = [
    'index.html',
    'about.html',
    'support.html',
    'guide.html',
    'tray/index.html',
    'plugins/index.html',
    'tools/font-tool/index.html',
    'tools/tray-icon-sorter/index.html',
];

function isExternalReference(value) {
    return /^(?:[a-z][a-z\d+.-]*:|\/\/|data:|#|javascript:|mailto:)/i.test(value);
}

function cleanReference(value) {
    return value.split('#', 1)[0].split('?', 1)[0];
}

function resolveReference(reference, sourceFile) {
    const value = cleanReference(reference);
    if (!value || isExternalReference(value) || value.startsWith('%23')) return null;

    return value.startsWith('/')
        ? resolve(ROOT, `.${value}`)
        : resolve(dirname(sourceFile), value);
}

async function ensureFile(file, source, reference) {
    try {
        await access(file);
        checkedFiles.add(file);
    } catch {
        errors.push(`${source}: missing ${reference} (${file})`);
    }
}

async function scanCss(file, stack = new Set()) {
    if (stack.has(file)) return;
    stack.add(file);

    let source;
    try {
        source = await readFile(file, 'utf8');
        checkedFiles.add(file);
    } catch {
        errors.push(`missing stylesheet ${file}`);
        return;
    }

    const imports = source.matchAll(/@import\s+(?:url\(\s*)?["']([^"']+)["']\s*\)?/gi);
    for (const match of imports) {
        const importedFile = resolveReference(match[1], file);
        if (!importedFile) continue;
        await ensureFile(importedFile, file, match[1]);
        if (extname(importedFile).toLowerCase() === '.css') {
            await scanCss(importedFile, stack);
        }
    }

    const urls = source.matchAll(/url\(\s*["']?([^\)"']+)["']?\s*\)/gi);
    for (const match of urls) {
        const assetFile = resolveReference(match[1].trim(), file);
        if (assetFile) await ensureFile(assetFile, file, match[1].trim());
    }
}

async function scanModuleImports(file, stack = new Set()) {
    if (stack.has(file)) return;
    stack.add(file);

    let source;
    try {
        source = await readFile(file, 'utf8');
        checkedFiles.add(file);
    } catch {
        errors.push(`missing script ${file}`);
        return;
    }

    const imports = source.matchAll(/(?:import\s+(?:[^'";]+?\s+from\s+)?|import\s*\()(['"])([^'"]+)\1/g);
    for (const match of imports) {
        const importedFile = resolveReference(match[2], file);
        if (!importedFile) continue;
        await ensureFile(importedFile, file, match[2]);
        if (extname(importedFile).toLowerCase() === '.js') {
            await scanModuleImports(importedFile, stack);
        }
    }
}

async function scanHtml(file) {
    let source;
    try {
        source = await readFile(file, 'utf8');
        checkedFiles.add(file);
    } catch {
        errors.push(`missing page ${file}`);
        return;
    }

    const localResourceAttributes = [
        /<link\b[^>]*\bhref=["']([^"']+)["']/gi,
        /<(?:script|img|video|audio|source)\b[^>]*\bsrc=["']([^"']+)["']/gi,
    ];

    for (const pattern of localResourceAttributes) {
        for (const match of source.matchAll(pattern)) {
            const resource = resolveReference(match[1], file);
            if (!resource) continue;
            await ensureFile(resource, file, match[1]);

            const extension = extname(resource).toLowerCase();
            if (extension === '.css') await scanCss(resource);
            if (extension === '.js') await scanModuleImports(resource);
        }
    }
}

for (const page of htmlPages) {
    await scanHtml(resolve(ROOT, page));
}

const globalStylesheet = resolve(ROOT, 'styles/style.css');
const globalStyles = await readFile(globalStylesheet, 'utf8');
const importCount = [...globalStyles.matchAll(/@import\s+/g)].length;
if (importCount < 10) {
    errors.push(`styles/style.css contains only ${importCount} imports; expected the full global module list`);
}

if (errors.length) {
    console.error(`FAIL static source audit (${errors.length} issue${errors.length === 1 ? '' : 's'}):`);
    errors.forEach(error => console.error(`- ${error}`));
    process.exitCode = 1;
} else {
    console.log(`PASS static source audit (${checkedFiles.size} local files reachable from ${htmlPages.length} pages)`);
}

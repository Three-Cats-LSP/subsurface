// SPDX-License-Identifier: GPL-2.0
// Syntax conversion only; never patch, tune or rebaseline LSP calculations here.
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const {createRequire} = require('node:module');
const dependencyRoot = process.env.NEO_LSP_BUILD_TOOLS || __dirname;
const load = createRequire(path.join(dependencyRoot, 'package.json'));
const babel = load('@babel/core');
const preset = load('@babel/preset-env');
const root = path.resolve(__dirname, '../..');
const vendor = path.join(root, 'third-party/lsp-planner');
const manifest = JSON.parse(fs.readFileSync(path.join(vendor, 'SOURCE.json'), 'utf8'));
const check = process.argv.includes('--check');
const hashes = {};
for (const [name, sha] of Object.entries(manifest.testingEvidenceFiles || {})) {
    const data = fs.readFileSync(path.join(root, 'tests/data/lsp-planner', name));
    if (crypto.createHash('sha256').update(data).digest('hex') !== sha)
        throw new Error('Pinned upstream evidence changed: ' + name);
}
if (crypto.createHash('sha256').update(fs.readFileSync(path.join(vendor, 'presets.json'))).digest('hex') !== manifest.presetsSha256)
    throw new Error('Pinned LSP preset data changed');
for (const [name, sha] of Object.entries(manifest.files)) {
    const source = fs.readFileSync(path.join(vendor, name));
    if (crypto.createHash('sha256').update(source).digest('hex') !== sha)
        throw new Error('Upstream source checksum changed: ' + name);
    const result = babel.transformSync(source.toString('utf8'), {
        filename: name, sourceType: 'script', babelrc: false, configFile: false,
        comments: true, compact: false,
        presets: [[preset, {targets: {ie: '11'}, modules: false, bugfixes: true, useBuiltIns: false}]],
    });
    const output = Buffer.from('// Generated from LSP+ ' + manifest.commit + '. See SOURCE.json.\n' + result.code + '\n');
    const dest = path.join(vendor, 'qt-runtime', name);
    if (check) {
        if (!fs.existsSync(dest) || !fs.readFileSync(dest).equals(output))
            throw new Error('Generated Qt runtime is stale: ' + name);
    } else {
        fs.mkdirSync(path.dirname(dest), {recursive: true});
        fs.writeFileSync(dest, output);
    }
    hashes[name] = crypto.createHash('sha256').update(output).digest('hex');
}
const generated = {
    schema: 1, sourceCommit: manifest.commit, babel: babel.version,
    presetEnv: load('@babel/preset-env/package.json').version,
    targetSyntax: 'ES5; no algorithm edits or builtin polyfills', files: hashes,
};
const runtimeManifest = path.join(vendor, 'qt-runtime/SOURCE.json');
const wire = JSON.stringify(generated, null, 2) + '\n';
if (check) {
    if (fs.readFileSync(runtimeManifest, 'utf8') !== wire) throw new Error('Runtime manifest is stale');
} else fs.writeFileSync(runtimeManifest, wire);
console.log('LSP+ source checksums and reproducible Qt runtime: PASS');

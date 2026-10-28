// SPDX-License-Identifier: GPL-2.0
// Capture only from the original pinned LSP source in Node. Never auto-approve
// a native result or replace a reviewed reference when running tests.
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const crypto = require('node:crypto');
const root = path.resolve(__dirname, '../..');
const vendor = path.join(root, 'third-party/lsp-planner');
const identity = JSON.parse(fs.readFileSync(path.join(vendor, 'SOURCE.json')));
function load() {
    const context = vm.createContext({});
    for (const file of ['third-party/lsp-planner/zhl-engine-bundle.js', 'qt-models/lspplannerhost.js',
        'third-party/lsp-planner/schedule-runner-core.js', 'third-party/lsp-planner/zhl-headless-adapter.js',
        'third-party/lsp-planner/vpm-engine-bundle.js', 'tests/data/lsp-planner/lib/ccrdiff.js'])
        vm.runInContext(fs.readFileSync(path.join(root, file), 'utf8'), context, {filename: file, timeout: 10000});
    return context;
}
function base() {
    return {schema: 1, model: 'ZHLC_GF', levels: [{depth: 40, time: 28, o2: 21, he: 0}], decoGases: [],
        settings: {metric: true, circuit: 'OC', gfLo: 20, gfHi: 85, descentRate: 20, ascentRate: 10,
            decoAscentRate: 3, surfaceAscentRate: 3, stepSize: 3, lastStop: 3, minStopTime: 2,
            ppO2Bottom: 1.4, ppO2Deco: 1.6, wholeMinStops: true, mdCompatMode: true, heHalfTimeMode: 'baker',
            conservatism: 0, shallowGradient: false, bailout: false},
        environment: {altSurfaceP: 1.01325, barPerMetre: 0.1, waterVapor: 0.0577, altAcclimatized: true, allowO2AtMOD: true}};
}
// This calls the upstream calculation entry, not NeoLspPlanner.calculate.
function original(context, request) {
    context.request = request;
    return JSON.parse(vm.runInContext(`
        altSurfaceP = request.environment.altSurfaceP;
        BAR_PER_METRE = request.environment.barPerMetre;
        WATER_VAPOR = request.environment.waterVapor;
        altAcclimatized = request.environment.altAcclimatized;
        allowO2AtMOD = request.environment.allowO2AtMOD;
        ZhlEngineBundle.setHeHalfTimeMode(request.settings.heHalfTimeMode);
        VPMEngine._syncHeHalfTimes(request.settings.heHalfTimeMode === 'buhl2003' ? ZhlEngineBundle.ZHL16C_HE_HT_BUHL2003 : ZhlEngineBundle.ZHL16C_HE_HT_BAKER);
        var settings = Object.assign({}, request.settings, {altitude:0, acclimatized:altAcclimatized,
            altSurfaceP:altSurfaceP, _barPerMetre:BAR_PER_METRE, _waterVapor:WATER_VAPOR,
            waterVapor:WATER_VAPOR, waterType:3, barPerM:BAR_PER_METRE});
        var result = request.model === 'ZHLC_GF' ? ZHLEngine.calculate(request.levels, request.decoGases, settings) : VPMEngine.calculate(request.levels, request.decoGases, settings, request.model);
        JSON.stringify(result);
    `, context, {timeout: 10000}));
}
const cases = [];
function capture(id, request, invalidCode) {
    const context = load();
    const result = original(context, request);
    if (invalidCode) {
        if (!result.error || result.code !== invalidCode) throw new Error(id + ': wrong upstream rejection: ' + JSON.stringify(result));
    } else {
        context.result = result;
        vm.runInContext(`CCRDiff.assertIntegrity(result, ${JSON.stringify(id)})`, context, {timeout: 10000});
        if (!result.finalTissues || result.finalTissues.length !== 16) throw new Error(id + ': missing tissues');
    }
    cases.push({id, request, expectedUpstream: result});
}
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'tests/data/lsp-planner/manifest.json')));
for (const id of manifest.fixtureIds) {
    const fx = JSON.parse(fs.readFileSync(path.join(root, 'tests/data/lsp-planner/fixtures', id + '.json')));
    const context = load(); context.fixture = fx;
    const request = base();
    request.levels = JSON.parse(vm.runInContext('JSON.stringify(CCRDiff.levelsFromFixture(fixture))', context));
    Object.assign(request.settings, JSON.parse(vm.runInContext('JSON.stringify(CCRDiff.settingsFromFixture(fixture))', context)));
    request.settings.wholeMinStops = fx.rounding.mode !== 'one-second';
    request.environment = {altSurfaceP: fx.environment.surfacePressureBar,
        barPerMetre: 1 / fx.environment.waterColumnMPerBar, waterVapor: fx.environment.waterVaporPressureBar,
        altAcclimatized: fx.environment.acclimatized, allowO2AtMOD: true};
    request.decoGases = (fx.bailoutGases || []).filter(g => g.available !== false).map(g => ({o2: g.o2 * 100, he: (g.he || 0) * 100}));
    if (fx.repetitive && fx.repetitive.repeatProfile) {
        const first = original(context, request);
        request.settings._preTissues = first.finalTissues;
        request.settings._surfaceInterval = fx.repetitive.surfaceIntervalMin;
    }
    capture(id, request, fx.expectInvalid ? fx.expectedCode : null);
}
function extra(id, edit) {const request = base(); edit(request); capture(id, request);}
extra('OC-air-NDL', r => {r.levels[0].depth = 15; r.levels[0].time = 20;});
extra('OC-air-deco', r => {r.levels[0].depth = 45; r.levels[0].time = 30;});
extra('OC-nitrox', r => {r.levels[0].o2 = 32;});
extra('OC-multilevel', r => {r.levels.push({depth:25,time:8,o2:21,he:0});});
extra('OC-three-gases', r => {r.decoGases = [{o2:50,he:0},{o2:100,he:0}];});
extra('OC-lost-oxygen', r => {r.decoGases = [{o2:50,he:0}];});
extra('OC-altitude-freshwater', r => {r.environment.altSurfaceP = 0.843; r.environment.barPerMetre = 1/10.33;});
extra('OC-helium-2003', r => {r.levels[0].o2 = 18; r.levels[0].he = 45; r.settings.heHalfTimeMode = 'buhl2003';});
for (const model of ['VPMA', 'VPMB', 'VPMBE', 'VPMB_GFS', 'VPMBFBO'])
    extra('OC-' + model, r => {r.model = model;});
for (const model of ['ZHLC_GF', 'VPMB']) {
    extra('pSCR-' + model, r => {r.model=model; r.levels[0].o2=32; Object.assign(r.settings,{circuit:'pSCR',bottomSetpoint:0,decoSetpoint:0,scrLoopVolume:8,scrMetabolicO2:1});});
    extra('CCR-' + model + '-setpoint', r => {r.model=model; Object.assign(r.settings,{circuit:'CCR',descentSetpoint:0.7,bottomSetpoint:1.3,decoSetpoint:1.3,setpoint:1.3});});
}
const output = process.argv[process.argv.indexOf('--output') + 1];
if (process.argv.includes('--check')) {
    const reference = JSON.parse(fs.readFileSync(path.join(root, 'tests/data/neo-lsp-regression.json')));
    if (reference.sourceCommit !== identity.commit || reference.cases.length !== cases.length)
        throw new Error('Reviewed reference identity/count changed');
    function compare(expected, actual, location) {
        if (typeof expected === 'number') {
            if (!Number.isFinite(actual) || Math.abs(expected-actual) > reference.numericTolerance)
                throw new Error(location + ': upstream numeric result changed');
        } else if (expected && typeof expected === 'object') {
            if (!actual || JSON.stringify(Object.keys(expected)) !== JSON.stringify(Object.keys(actual)))
                throw new Error(location + ': upstream structure changed');
            for (const key of Object.keys(expected)) compare(expected[key], actual[key], location+'.'+key);
        } else if (expected !== actual) throw new Error(location + ': upstream value changed');
    }
    compare(reference.cases, cases, 'cases');
    console.log('All ' + cases.length + ' reviewed references reproduced with original LSP source: PASS');
    process.exit(0);
}
if (!process.argv.includes('--output') || !output || fs.existsSync(output)) throw new Error('Provide a new --output path; reviewed references are never overwritten');
const data = {schema: 1, engine: identity.project, version: identity.version, sourceCommit: identity.commit,
    provenance: 'Original pinned LSP production bundles, Node ' + process.version + '; no Qt result used as an expectation.',
    numericTolerance: 1e-7, cases};
fs.writeFileSync(output, JSON.stringify(data, null, 2) + '\n');
console.log('Captured ' + cases.length + ' upstream cases: ' + output + ' sha256=' + crypto.createHash('sha256').update(fs.readFileSync(output)).digest('hex'));

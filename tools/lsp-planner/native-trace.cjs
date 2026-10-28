// SPDX-License-Identifier: GPL-2.0
// Read-only integration patch against the pinned LSP source. Expose physical
// durations before presentation folds transit into stop rows. Never change any
// loading, rounding, gas-switch, ceiling, exposure or schedule calculation.
const crypto = require('node:crypto');
function once(source, before, after) {
    if (source.split(before).length !== 2) throw new Error('LSP native trace anchor changed: ' + before);
    return source.replace(before, after);
}
module.exports = function nativeTrace(name, source) {
    if (name === 'schedule-runner-core.js') {
        source=once(source,'function calcGasMODm(fO2, ppO2Limit) {','function calcGasMODm(fO2, ppO2Limit, nativeLastStop) {');
        return once(source,"const lastStop = parseInt(document.getElementById('lastDecoStop')?.value, 10) || 3;",
            "const lastStop = Number.isFinite(nativeLastStop) ? nativeLastStop : (parseInt(document.getElementById('lastDecoStop')?.value, 10) || 3);");
    }
    if (name !== 'zhl-engine-bundle.js') return source;
    source = once(source, "  'use strict';", "  'use strict';\n  let neoNativeTrace = null;");
    for (const display of ['mustStopDisplay', 'minStopDisplay', 'lastStopDisplay', 'mdpDisplay'])
        source = once(source, 'dur: ' + display + ',', 'dur: ' + display + ', nativeDuration: stopT,');
    source = once(source, '  const lastPlan = {', `  neoNativeTrace = {
    runtimeMinutes: rt, steps: steps.map(step => ({...step})),
    tissuesAtBottom: tissuesAtBottom.map(t => ({...t})),
    finalTissues: tissues.map(t => ({...t})), hitSafetyGuard: !!hitSafetyGuard,
    bottomRuntimeMinutes: bt, firstStopDepth: firstStopDepth || 0,
  };
  const lastPlan = {`);
    source = once(source, '  const api = {', '  const api = {\n    neoNativeTrace: () => neoNativeTrace,');
    return source;
};
module.exports.sha256 = crypto.createHash('sha256').update(require('node:fs').readFileSync(__filename)).digest('hex');

// SPDX-License-Identifier: GPL-2.0
// Native host glue only. Decompression and exposure calculations stay upstream.
var window = globalThis;
var altSurfaceP = 1.01325;
var BAR_PER_METRE = 0.1;
var WATER_VAPOR = 0.0577;
var altAcclimatized = true;
var allowO2AtMOD = true;
var OTU_EXPONENT = ZhlEngineBundle.OTU_EXPONENT;

var NeoLspPlanner = {
    calculate: function(request) {
        function fail(code, message) {
            return { error: message, code: code, plan: [], stops: [], totalRuntime: 0 };
        }
        function finiteBetween(value, low, high) {
            return typeof value === 'number' && Number.isFinite(value) && value >= low && value <= high;
        }
        if (!request || request.schema !== 1)
            return fail('INVALID_REQUEST', 'Unsupported LSP planner request version.');
        if (['ZHLC_GF', 'VPMA', 'VPMB', 'VPMBE', 'VPMB_GFS', 'VPMBFBO'].indexOf(request.model) < 0)
            return fail('INVALID_MODEL', 'Unknown LSP decompression model.');
        var levels = request.levels, gases = request.decoGases;
        var settings = request.settings, environment = request.environment;
        if (!Array.isArray(levels) || levels.length < 1 || levels.length > 64 ||
                !Array.isArray(gases) || gases.length > 32 || !settings || !environment)
            return fail('INVALID_REQUEST', 'Explicit levels, gases, settings and environment are required.');
        // Do not infer environment from a display unit or hidden browser state.
        if (!finiteBetween(environment.altSurfaceP, 0.3, 1.2) ||
                !finiteBetween(environment.barPerMetre, 0.08, 0.13) ||
                !finiteBetween(environment.waterVapor, 0, 0.1) ||
                typeof environment.altAcclimatized !== 'boolean' ||
                typeof environment.allowO2AtMOD !== 'boolean')
            return fail('INVALID_ENVIRONMENT', 'Explicit finite pressure and water assumptions are required.');
        if (settings.metric !== true || ['OC', 'CCR', 'pSCR'].indexOf(settings.circuit) < 0)
            return fail('INVALID_SETTINGS', 'Native LSP requests use metric engine units and an explicit circuit.');
        for (var key of ['descentRate', 'ascentRate', 'decoAscentRate', 'surfaceAscentRate']) {
            if (!finiteBetween(settings[key], 0.1, 100))
                return fail('INVALID_SETTINGS', 'An explicit positive ' + key + ' is required.');
        }
        for (var key of ['stepSize', 'lastStop', 'minStopTime']) {
            if (!finiteBetween(settings[key], 0.01, 30))
                return fail('INVALID_SETTINGS', 'An explicit positive ' + key + ' is required.');
        }
        if (!finiteBetween(settings.gfLo, 1, 100) || !finiteBetween(settings.gfHi, settings.gfLo, 100) ||
                !finiteBetween(settings.ppO2Bottom, 0.5, 2) || !finiteBetween(settings.ppO2Deco, 0.5, 2) ||
                typeof settings.wholeMinStops !== 'boolean' || typeof settings.mdCompatMode !== 'boolean' ||
                ['baker', 'buhl2003'].indexOf(settings.heHalfTimeMode) < 0)
            return fail('INVALID_SETTINGS', 'Explicit gradient, oxygen, transit, rounding and helium settings are required.');
        // Run the existing LSP input guards before reaching either bundle.
        if (levels.some(function(level) { return !level; }) || gases.some(function(gas) { return !gas; }))
            return fail('INVALID_REQUEST', 'Profile and gas entries must be objects.');
        var validation = validateCcrCalculationInputs(levels, settings, gases);
        if (!validation.ok) return engineValidationError(validation);
        validation = validateEngineInputs(levels, gases);
        if (!validation.ok) return engineValidationError(validation);
        validation = validateZhlHeadlessProfile(levels);
        if (!validation.ok) return engineValidationError({errors: [validation]});
        for (var level of levels) {
            if (!finiteBetween(level.depth, 0.01, 500) || !finiteBetween(level.time, 0.01, 1440))
                return fail('INVALID_PROFILE', 'Profile exceeds the bounded native calculation limits.');
        }
        if (settings._preTissues != null && (!Array.isArray(settings._preTissues) || settings._preTissues.length !== 16 ||
                !settings._preTissues.every(function(t) { return t && finiteBetween(t.pN2, 0, 100) && finiteBetween(t.pHe, 0, 100); })))
            return fail('INVALID_REPETITIVE_STATE', 'Repetitive loading requires sixteen explicit finite tissue pairs.');
        if (settings._preTissues != null && !finiteBetween(settings._surfaceInterval, 0, 10080))
            return fail('INVALID_REPETITIVE_STATE', 'Repetitive loading requires an explicit surface interval in minutes.');
        altSurfaceP = environment.altSurfaceP;
        BAR_PER_METRE = environment.barPerMetre;
        WATER_VAPOR = environment.waterVapor;
        altAcclimatized = environment.altAcclimatized;
        allowO2AtMOD = environment.allowO2AtMOD;
        ZhlEngineBundle.setHeHalfTimeMode(settings.heHalfTimeMode);
        VPMEngine._syncHeHalfTimes(settings.heHalfTimeMode === 'buhl2003' ? ZhlEngineBundle.ZHL16C_HE_HT_BUHL2003 : ZhlEngineBundle.ZHL16C_HE_HT_BAKER);
        // The request environment is authoritative. These are LSP's existing
        // host parameters, not a second pressure or altitude calculation.
        var s = Object.assign({}, settings, {altitude: 0, acclimatized: altAcclimatized,
            altSurfaceP: altSurfaceP, _barPerMetre: BAR_PER_METRE, _waterVapor: WATER_VAPOR,
            waterVapor: WATER_VAPOR, waterType: 3, barPerM: BAR_PER_METRE});
        var result = request.model === 'ZHLC_GF' ? ZHLEngine.calculate(levels, gases, s) : VPMEngine.calculate(levels, gases, s, request.model);
        if (!result || result.error) return result || fail('ENGINE_ERROR', 'LSP produced no result.');
        // Reject non-finite values before JSON serialization can turn them into
        // null and conceal a failed calculation.
        function finiteTree(value) {
            if (typeof value === 'number') return Number.isFinite(value);
            if (!value || typeof value !== 'object') return true;
            return Object.keys(value).every(function(key) { return finiteTree(value[key]); });
        }
        if (!finiteTree(result) || !Array.isArray(result.plan) || !result.plan.length ||
                !finiteBetween(result.totalRuntime, 0.0001, 10080) || !result.finalTissues || result.finalTissues.length !== 16)
            return fail('INVALID_RESULT', 'LSP produced an incomplete or non-finite result.');
        return result;
    }
};

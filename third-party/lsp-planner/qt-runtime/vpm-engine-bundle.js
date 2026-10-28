// Generated from LSP+ 05fef18bade6354cebcc60df01f83e0ac4cc3b27. See SOURCE.json.
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), !0).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: !0, configurable: !0, writable: !0 }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == _typeof(i) ? i : i + ""; }
function _toPrimitive(t, e) { if ("object" != _typeof(t) || !t) return t; var r; if ("undefined" != typeof Symbol && void 0 !== (r = t[Symbol.toPrimitive])) { var i = r.call(t, e || "default"); if ("object" != _typeof(i)) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === e ? String : Number)(t); }
/**
 * VPM Engine Bundle — @generated from vpm-engine-core.js via tools/build_vpm_bundle.py
 * Tier 3 isolated VPM-B / VPM-B+GFS module.
 * Loaded on main thread; exposes window.VPMEngine for tests and runVPMSchedule().
 *
 * Host runtime deps (resolved at calculate() time, not load time):
 *   validateEngineInputs, validateCcrCalculationInputs, engineValidationError,
 *   isRebreatherCircuit, mergeCCRSettings, getEffectivePpo2, computePlanExposureTotals
 *   globals: altSurfaceP, BAR_PER_METRE (exposure fallbacks)
 */
var VPMEngine = function () {
  'use strict';

  var ZHL16C_N2 = [{
    ht: 5.0,
    a: 1.2599,
    b: 0.5050
  }, {
    ht: 8.0,
    a: 1.0000,
    b: 0.6514
  }, {
    ht: 12.5,
    a: 0.8618,
    b: 0.7222
  }, {
    ht: 18.5,
    a: 0.7562,
    b: 0.7825
  }, {
    ht: 27.0,
    a: 0.6200,
    b: 0.8126
  }, {
    ht: 38.3,
    a: 0.5043,
    b: 0.8434
  }, {
    ht: 54.3,
    a: 0.4410,
    b: 0.8693
  }, {
    ht: 77.0,
    a: 0.4000,
    b: 0.8910
  }, {
    ht: 109.0,
    a: 0.3750,
    b: 0.9092
  }, {
    ht: 146.0,
    a: 0.3500,
    b: 0.9222
  }, {
    ht: 187.0,
    a: 0.3295,
    b: 0.9319
  }, {
    ht: 239.0,
    a: 0.3065,
    b: 0.9403
  }, {
    ht: 305.0,
    a: 0.2835,
    b: 0.9477
  }, {
    ht: 390.0,
    a: 0.2610,
    b: 0.9544
  }, {
    ht: 498.0,
    a: 0.2480,
    b: 0.9602
  }, {
    ht: 635.0,
    a: 0.2327,
    b: 0.9653
  }];
  var ZHL16C_He = [{
    ht: 1.88,
    a: 1.7424,
    b: 0.4245
  }, {
    ht: 3.02,
    a: 1.3830,
    b: 0.5747
  }, {
    ht: 4.72,
    a: 1.1919,
    b: 0.6527
  }, {
    ht: 6.99,
    a: 1.0458,
    b: 0.7223
  }, {
    ht: 10.21,
    a: 0.9220,
    b: 0.7582
  }, {
    ht: 14.48,
    a: 0.8205,
    b: 0.7957
  }, {
    ht: 20.53,
    a: 0.7305,
    b: 0.8279
  }, {
    ht: 29.11,
    a: 0.6502,
    b: 0.8553
  }, {
    ht: 41.20,
    a: 0.5950,
    b: 0.8757
  }, {
    ht: 55.19,
    a: 0.5545,
    b: 0.8903
  }, {
    ht: 70.69,
    a: 0.5333,
    b: 0.8997
  }, {
    ht: 90.34,
    a: 0.5189,
    b: 0.9073
  }, {
    ht: 115.29,
    a: 0.5181,
    b: 0.9122
  }, {
    ht: 147.42,
    a: 0.5176,
    b: 0.9171
  }, {
    ht: 188.24,
    a: 0.5172,
    b: 0.9217
  }, {
    ht: 240.03,
    a: 0.5119,
    b: 0.9267
  }];
  var NC = 16;
  var GAMMA = 0.0179;
  var GAMMA_C = 0.257;
  var INITIAL_RADIUS_N2 = 0.55e-6;
  var INITIAL_RADIUS_He = 0.45e-6;
  var REGEN_TIME_MIN = 20160.0;
  var REGEN_TIME = REGEN_TIME_MIN;
  function getWaterVaporPressure(settings) {
    return settings && settings.waterVapor != null ? settings.waterVapor : 0.0627;
  }
  var CONSTANT_PRESSURE_OTHER_GASES = 102.0 / 760.0 * 1.01325;
  var CRIT_VOLUME_LAMBDA_FSW_MIN = 7500.0;
  function twoGammaOverR(gamma, r) {
    return 2.0 * gamma / r / 1.0e5;
  }
  var SLP_SW_M = 10.000; // salt, metric — MultiDeco/DiveKit/ApexDeco standard
  var SLP_FW_M = 10.330; // fresh, metric — matches ZHL WATER_DENSITY.fresh (0.09681 bar/m)
  var SLP_EN_M = 10.080; // EN13319, metric — matches DiveKit EN13319
  var SLP_SW_F = 32.808; // salt, imperial — 10.000 × 3.28084
  var SLP_FW_F = 33.891; // fresh, imperial — 10.330 × 3.28084
  var SLP_EN_F = 33.071; // EN13319, imperial — 10.080 × 3.28084
  function getSLP(settings) {
    if (settings.waterType === 3) {
      var barPerM = settings.barPerM || (settings.customWaterDensity ? settings.customWaterDensity * 9.80665 / 100000 : 0);
      if (barPerM > 0) {
        var slpM = 1 / barPerM;
        return settings.metric ? slpM : slpM * 3.28084;
      }
    }
    if (settings.metric) {
      if (settings.waterType === 2) return SLP_EN_M;
      return settings.waterType === 0 ? SLP_SW_M : SLP_FW_M;
    }
    if (settings.waterType === 2) return SLP_EN_F;
    return settings.waterType === 0 ? SLP_SW_F : SLP_FW_F;
  }
  function getSurfacePressure(settings) {
    if (settings && Number.isFinite(settings.altSurfaceP) && settings.altSurfaceP > 0) {
      return settings.altSurfaceP;
    }
    var alt = settings && settings.altitude != null ? settings.altitude : 0;
    return 1.01325 * Math.exp(-alt / 8434);
  }
  function isFiniteTissueRow(t) {
    return t && Number.isFinite(t.pN2) && Number.isFinite(t.pHe);
  }
  function isValidTissueRow(t) {
    return isFiniteTissueRow(t) && t.pN2 >= 0 && t.pHe >= 0;
  }
  function validatePreTissues(pre, nc) {
    if (!pre) return true;
    if (!Array.isArray(pre) || pre.length !== nc) return false;
    return pre.every(isValidTissueRow);
  }
  function validateRadiusArray(arr, nc) {
    if (!Array.isArray(arr) || arr.length !== nc) return false;
    return arr.every(function (v) {
      return Number.isFinite(v) && v > 0;
    });
  }
  function validateRepetitiveState(settings, nc) {
    if (settings._preTissues != null && !validatePreTissues(settings._preTissues, nc)) {
      return {
        ok: false,
        code: 'INVALID_REPETITIVE_STATE',
        message: 'Repetitive tissue state must be 16 finite non-negative {pN2,pHe} rows'
      };
    }
    var pb = settings._prevBubbleState;
    if (pb != null) {
      var keys = ['adjustedCritRadiiN2', 'adjustedCritRadiiHe', 'regeneratedRadiiN2', 'regeneratedRadiiHe'];
      if (!keys.every(function (k) {
        return validateRadiusArray(pb[k], nc);
      })) {
        return {
          ok: false,
          code: 'INVALID_REPETITIVE_STATE',
          message: 'Repetitive bubble state arrays must be 16 finite strictly positive values'
        };
      }
    }
    return {
      ok: true
    };
  }
  function validateVpmSettings(settings) {
    var rates = ['descentRate', 'ascentRate', 'decoAscentRate', 'surfaceAscentRate'];
    for (var _i = 0, _rates = rates; _i < _rates.length; _i++) {
      var key = _rates[_i];
      var v = settings[key];
      if (v != null && (!Number.isFinite(v) || v <= 0)) {
        return {
          ok: false,
          code: 'INVALID_VPM_SETTINGS',
          message: "".concat(key, " must be positive")
        };
      }
    }
    var step = settings.stepSize;
    if (step != null && (!Number.isFinite(step) || step <= 0)) {
      return {
        ok: false,
        code: 'INVALID_VPM_SETTINGS',
        message: 'stepSize must be positive'
      };
    }
    var last = settings.lastStop;
    if (last != null && (!Number.isFinite(last) || last < 0)) {
      return {
        ok: false,
        code: 'INVALID_VPM_SETTINGS',
        message: 'lastStop must be non-negative'
      };
    }
    var minStop = settings.minStopTime;
    if (minStop != null && (!Number.isFinite(minStop) || minStop < 0)) {
      return {
        ok: false,
        code: 'INVALID_VPM_SETTINGS',
        message: 'minStopTime must be non-negative'
      };
    }
    return {
      ok: true
    };
  }
  function zhlBundle() {
    if (typeof ZhlEngineBundle !== 'undefined') return ZhlEngineBundle;
    if (typeof window !== 'undefined' && window.ZhlEngineBundle) return window.ZhlEngineBundle;
    return null;
  }
  function syncZhlEnvFromSettings(settings) {
    var b = zhlBundle();
    if (!b || typeof b.applyEnvironment !== 'function' || !settings) return;
    b.applyEnvironment({
      altSurfaceP: getSurfacePressure(settings),
      barPerMetre: settings._barPerMetre != null ? settings._barPerMetre : typeof BAR_PER_METRE !== 'undefined' ? BAR_PER_METRE : 0.1,
      waterVapor: settings._waterVapor != null ? settings._waterVapor : typeof WATER_VAPOR !== 'undefined' ? WATER_VAPOR : 0.0627,
      altAcclimatized: settings.acclimatized !== false,
      allowO2AtMOD: true
    });
  }
  function ccrNormalize(settings) {
    var b = zhlBundle();
    if (b && typeof b.normalizeCCRSettings === 'function') return b.normalizeCCRSettings(settings);
    if (typeof mergeCCRSettings === 'function') return mergeCCRSettings(settings);
    return settings;
  }
  function ccrSetpointAtDepth(depthM, ccr, surfP, phase) {
    var b = zhlBundle();
    if (b && typeof b.getEffectiveSetpointAtDepth === 'function') {
      syncZhlEnvFromSettings({
        altitude: 0,
        acclimatized: true,
        _barPerMetre: typeof BAR_PER_METRE !== 'undefined' ? BAR_PER_METRE : 0.1
      });
      if (typeof b.applyEnvironment === 'function') {
        b.applyEnvironment({
          altSurfaceP: surfP,
          barPerMetre: typeof BAR_PER_METRE !== 'undefined' ? BAR_PER_METRE : 0.1,
          waterVapor: typeof WATER_VAPOR !== 'undefined' ? WATER_VAPOR : 0.0627,
          altAcclimatized: true,
          allowO2AtMOD: true
        });
      }
      return b.getEffectiveSetpointAtDepth(depthM, ccr, surfP, phase);
    }
    if (typeof getEffectiveSetpointAtDepth === 'function') {
      return getEffectiveSetpointAtDepth(depthM, ccr, surfP, phase);
    }
    return 0;
  }
  function ccrSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, ccr, settings) {
    var b = zhlBundle();
    if (b && typeof b.getCCRInertSchreinerParams === 'function') {
      if (settings) syncZhlEnvFromSettings(settings);
      return b.getCCRInertSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, ccr);
    }
    if (typeof getCCRInertSchreinerParams === 'function') {
      return getCCRInertSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, ccr);
    }
    var fN2 = Math.max(0, 1 - fO2 - fHe);
    var ppH2O = typeof WATER_VAPOR !== 'undefined' ? WATER_VAPOR : 0.0627;
    return {
      inspN2Start: (pAmbStart - ppH2O) * fN2,
      inspHeStart: (pAmbStart - ppH2O) * fHe,
      rN2: fN2 * pressureRate,
      rHe: fHe * pressureRate
    };
  }
  function ccrSplitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP) {
    var b = zhlBundle();
    if (b && typeof b.splitSegmentAtSetpoint === 'function') {
      syncZhlEnvFromSettings({
        altitude: 0,
        acclimatized: true
      });
      if (typeof b.applyEnvironment === 'function') {
        b.applyEnvironment({
          altSurfaceP: surfP,
          barPerMetre: typeof BAR_PER_METRE !== 'undefined' ? BAR_PER_METRE : 0.1,
          waterVapor: typeof WATER_VAPOR !== 'undefined' ? WATER_VAPOR : 0.0627,
          altAcclimatized: true,
          allowO2AtMOD: true
        });
      }
      return b.splitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP);
    }
    if (typeof splitSegmentAtSetpoint === 'function') {
      return splitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP);
    }
    return [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
  }
  function getAmbientPressure(depth, settings) {
    return getSurfacePressure(settings) + depth / getSLP(settings);
  }
  function createVPMState(settings) {
    var surfP = getSurfacePressure(settings);
    var ppH2O = getWaterVaporPressure(settings);
    var inspiredN2 = 0.7902 * (surfP - ppH2O);
    var tissues = [];
    var critRadiiN2 = [];
    var critRadiiHe = [];
    var maxActualGradientN2 = [];
    var maxActualGradientHe = [];
    var maxCrushingPressureN2 = [];
    var maxCrushingPressureHe = [];
    var allowableGradientN2 = [];
    var allowableGradientHe = [];
    var decoGradientN2 = [];
    var decoGradientHe = [];
    var initialAllowableGradientN2 = [];
    var initialAllowableGradientHe = [];
    var adjustedCritRadiiN2 = [];
    var adjustedCritRadiiHe = [];
    var regeneratedRadiiN2 = [];
    var regeneratedRadiiHe = [];
    var adjustedCrushingPressureN2 = [];
    var adjustedCrushingPressureHe = [];
    var surfacePhaseVolumeTime = [];
    var lastPhaseVolumeTime = [];
    // ── Feature A: Altitude-adjusted critical radii ────────────────────────────────
    // At altitude, surface pressure is lower, so bubbles equilibrate to a larger
    // radius (less crushing pressure). Formula: r_alt = r_0 * (P_SL / P_alt)^(1/3)
    // Volume scales linearly with pressure (ideal gas); radius is the cube root.
    // At sea level surfP == 1.01325 so the factor is exactly 1.0 — no change.
    var P_SL = 1.01325; // bar — standard sea-level pressure
    var altFactor = Math.pow(P_SL / surfP, 1.0 / 3.0);
    var initRadN2 = INITIAL_RADIUS_N2 * altFactor; // enlarged at altitude
    var initRadHe = INITIAL_RADIUS_He * altFactor;
    // ─────────────────────────────────────────────────────────────────────────────
    for (var i = 0; i < NC; i++) {
      tissues.push({
        pN2: inspiredN2,
        pHe: 0
      });
      critRadiiN2.push(initRadN2);
      critRadiiHe.push(initRadHe);
      maxActualGradientN2.push(0);
      maxActualGradientHe.push(0);
      maxCrushingPressureN2.push(0);
      maxCrushingPressureHe.push(0);
      var gN2 = twoGammaOverR(GAMMA, initRadN2) * (GAMMA_C - GAMMA) / GAMMA_C;
      var gHe = twoGammaOverR(GAMMA, initRadHe) * (GAMMA_C - GAMMA) / GAMMA_C;
      allowableGradientN2.push(gN2);
      allowableGradientHe.push(gHe);
      decoGradientN2.push(gN2);
      decoGradientHe.push(gHe);
      initialAllowableGradientN2.push(gN2);
      initialAllowableGradientHe.push(gHe);
      adjustedCritRadiiN2.push(initRadN2);
      adjustedCritRadiiHe.push(initRadHe);
      regeneratedRadiiN2.push(initRadN2);
      regeneratedRadiiHe.push(initRadHe);
      adjustedCrushingPressureN2.push(0);
      adjustedCrushingPressureHe.push(0);
      surfacePhaseVolumeTime.push(0);
      lastPhaseVolumeTime.push(0);
    }
    if (settings._preTissues && validatePreTissues(settings._preTissues, NC)) {
      for (var _i2 = 0; _i2 < NC; _i2++) {
        tissues[_i2].pN2 = settings._preTissues[_i2].pN2;
        tissues[_i2].pHe = settings._preTissues[_i2].pHe;
      }
      if (settings._surfaceInterval > 0) {
        var inspN2 = 0.7902 * (surfP - ppH2O);
        for (var _i3 = 0; _i3 < NC; _i3++) {
          var kN2 = Math.LN2 / ZHL16C_N2[_i3].ht;
          var kHe = Math.LN2 / ZHL16C_He[_i3].ht;
          tissues[_i3].pN2 = inspN2 + (tissues[_i3].pN2 - inspN2) * Math.exp(-kN2 * settings._surfaceInterval);
          tissues[_i3].pHe = tissues[_i3].pHe * Math.exp(-kHe * settings._surfaceInterval);
        }
      }
    }
    // ── Feature B: Repetitive dive bubble state carry ────────────────────────────
    // When _prevBubbleState is provided (from a previous VPM dive), carry the
    // end-of-dive adjusted critical radii and regenerated radii into this dive,
    // applying surface-interval partial regeneration (exponential, same REGEN_TIME).
    // This is the correct VPM treatment: nuclei that were crushed and compressed
    // during the previous dive don't fully return to their original size overnight —
    // they partially regenerate, and subsequent dives start with those smaller radii
    // (conservative: smaller radius → larger allowable gradient → less deco time
    //  is WRONG; actually smaller nucleus = harder to grow = LESS bubble risk;
    //  but larger nuclei survive surface interval better → more conservative for next dive).
    // Net effect: carrying state is conservative — correct VPM-B behaviour.
    var bubbleCarryApplied = false;
    var pb = settings._prevBubbleState;
    var bubbleStateOk = pb && ['adjustedCritRadiiN2', 'adjustedCritRadiiHe', 'regeneratedRadiiN2', 'regeneratedRadiiHe'].every(function (k) {
      return validateRadiusArray(pb[k], NC);
    });
    if (bubbleStateOk) {
      var si = Math.max(0, settings._surfaceInterval != null ? settings._surfaceInterval : 0);
      var regenFactor = Math.exp(-si / REGEN_TIME);
      var prevAltFactor = pb._altFactor != null && pb._altFactor > 0 ? pb._altFactor : altFactor;
      for (var _i4 = 0; _i4 < NC; _i4++) {
        // End-of-previous-dive radius (after nuclear regeneration was applied)
        var prevN2 = pb.regeneratedRadiiN2[_i4];
        var prevHe = pb.regeneratedRadiiHe[_i4];
        var prevN2AtCurrentAlt = prevN2 / prevAltFactor * altFactor;
        var prevHeAtCurrentAlt = prevHe / prevAltFactor * altFactor;
        // During surface interval, radii regenerate toward the altitude-adjusted
        // initial values (baseline for this altitude). Partial regeneration:
        //   r(t) = r_init + (r_prev - r_init) * exp(-t / REGEN_TIME_MIN)
        var carriedN2 = initRadN2 + (prevN2AtCurrentAlt - initRadN2) * regenFactor;
        var carriedHe = initRadHe + (prevHeAtCurrentAlt - initRadHe) * regenFactor;
        critRadiiN2[_i4] = carriedN2;
        critRadiiHe[_i4] = carriedHe;
        adjustedCritRadiiN2[_i4] = carriedN2;
        adjustedCritRadiiHe[_i4] = carriedHe;
        regeneratedRadiiN2[_i4] = carriedN2;
        regeneratedRadiiHe[_i4] = carriedHe;
        // Recalculate allowable gradients from carried radii
        var gN2c = twoGammaOverR(GAMMA, carriedN2) * (GAMMA_C - GAMMA) / GAMMA_C;
        var gHec = twoGammaOverR(GAMMA, carriedHe) * (GAMMA_C - GAMMA) / GAMMA_C;
        allowableGradientN2[_i4] = gN2c;
        allowableGradientHe[_i4] = gHec;
        decoGradientN2[_i4] = gN2c;
        decoGradientHe[_i4] = gHec;
        initialAllowableGradientN2[_i4] = gN2c;
        initialAllowableGradientHe[_i4] = gHec;
      }
      bubbleCarryApplied = true;
    }
    // ─────────────────────────────────────────────────────────────────────────────
    return {
      tissues: tissues,
      critRadiiN2: critRadiiN2,
      critRadiiHe: critRadiiHe,
      maxActualGradientN2: maxActualGradientN2,
      maxActualGradientHe: maxActualGradientHe,
      maxCrushingPressureN2: maxCrushingPressureN2,
      maxCrushingPressureHe: maxCrushingPressureHe,
      allowableGradientN2: allowableGradientN2,
      allowableGradientHe: allowableGradientHe,
      decoGradientN2: decoGradientN2,
      decoGradientHe: decoGradientHe,
      initialAllowableGradientN2: initialAllowableGradientN2,
      initialAllowableGradientHe: initialAllowableGradientHe,
      adjustedCritRadiiN2: adjustedCritRadiiN2,
      adjustedCritRadiiHe: adjustedCritRadiiHe,
      regeneratedRadiiN2: regeneratedRadiiN2,
      regeneratedRadiiHe: regeneratedRadiiHe,
      adjustedCrushingPressureN2: adjustedCrushingPressureN2,
      adjustedCrushingPressureHe: adjustedCrushingPressureHe,
      surfacePhaseVolumeTime: surfacePhaseVolumeTime,
      lastPhaseVolumeTime: lastPhaseVolumeTime,
      maxDepth: 0,
      maxAmbientPressure: surfP,
      firstStopDepth: 0,
      useDecoGradients: false,
      _bubbleCarryApplied: bubbleCarryApplied,
      _altFactor: altFactor,
      _conservatismRadiiApplied: false
    };
  }
  function cloneVPMState(baseState) {
    return _objectSpread(_objectSpread({}, baseState), {}, {
      tissues: cloneTissues(baseState.tissues),
      critRadiiN2: baseState.critRadiiN2.slice(),
      critRadiiHe: baseState.critRadiiHe.slice(),
      maxActualGradientN2: baseState.maxActualGradientN2.slice(),
      maxActualGradientHe: baseState.maxActualGradientHe.slice(),
      maxCrushingPressureN2: baseState.maxCrushingPressureN2.slice(),
      maxCrushingPressureHe: baseState.maxCrushingPressureHe.slice(),
      allowableGradientN2: baseState.allowableGradientN2.slice(),
      allowableGradientHe: baseState.allowableGradientHe.slice(),
      decoGradientN2: baseState.decoGradientN2.slice(),
      decoGradientHe: baseState.decoGradientHe.slice(),
      initialAllowableGradientN2: baseState.initialAllowableGradientN2.slice(),
      initialAllowableGradientHe: baseState.initialAllowableGradientHe.slice(),
      adjustedCritRadiiN2: baseState.adjustedCritRadiiN2.slice(),
      adjustedCritRadiiHe: baseState.adjustedCritRadiiHe.slice(),
      regeneratedRadiiN2: baseState.regeneratedRadiiN2.slice(),
      regeneratedRadiiHe: baseState.regeneratedRadiiHe.slice(),
      adjustedCrushingPressureN2: baseState.adjustedCrushingPressureN2.slice(),
      adjustedCrushingPressureHe: baseState.adjustedCrushingPressureHe.slice(),
      surfacePhaseVolumeTime: baseState.surfacePhaseVolumeTime.slice(),
      lastPhaseVolumeTime: baseState.lastPhaseVolumeTime.slice()
    });
  }
  function haldane(pStart, pInspired, ht, time) {
    var k = Math.LN2 / ht;
    return pStart + (pInspired - pStart) * (1 - Math.exp(-k * time));
  }
  function schreiner(pInspiredStart, rate, time, ht, pStart) {
    var k = Math.LN2 / ht;
    return pInspiredStart + rate * (time - 1 / k) - (pInspiredStart - pStart - rate / k) * Math.exp(-k * time);
  }
  function cloneTissues(tissues) {
    return tissues.map(function (t) {
      return {
        pN2: t.pN2,
        pHe: t.pHe
      };
    });
  }
  function restoreTissues(state, tissues) {
    for (var i = 0; i < NC; i++) {
      state.tissues[i].pN2 = tissues[i].pN2;
      state.tissues[i].pHe = tissues[i].pHe;
    }
  }
  function loadTissuesConstant(state, depth, time, o2Frac, heFrac, settings, setpoint) {
    var pAmb = getAmbientPressure(depth, settings);
    var ccr = {
      circuit: settings.circuit || 'OC',
      setpoint: setpoint,
      descentSetpoint: settings.descentSetpoint,
      bailout: settings.bailout,
      scrLoopVolume: settings.scrLoopVolume,
      scrMetabolicO2: settings.scrMetabolicO2,
      scrRuntimeMin: settings._scrRuntimeMin || 0
    };
    var insp = getInspiredInertPressures(pAmb, setpoint, o2Frac, heFrac, ccr);
    if (pAmb > state.maxAmbientPressure) {
      state.maxAmbientPressure = pAmb;
      state.maxDepth = depth;
    }
    for (var i = 0; i < NC; i++) {
      state.tissues[i].pN2 = haldane(state.tissues[i].pN2, insp.pN2, ZHL16C_N2[i].ht, time);
      state.tissues[i].pHe = haldane(state.tissues[i].pHe, insp.pHe, ZHL16C_He[i].ht, time);
    }
  }
  function loadTissuesLinear(state, startDepth, endDepth, rate, o2Frac, heFrac, settings, setpoint, phase) {
    var time = Math.abs(endDepth - startDepth) / rate;
    if (time <= 0) return 0;
    var slp = getSLP(settings);
    var surfP = getSurfacePressure(settings);
    var maxP = Math.max(getAmbientPressure(startDepth, settings), getAmbientPressure(endDepth, settings));
    if (maxP > state.maxAmbientPressure) {
      state.maxAmbientPressure = maxP;
      state.maxDepth = Math.max(startDepth, endDepth);
    }
    var ccr = {
      circuit: settings.circuit || 'OC',
      setpoint: setpoint,
      descentSetpoint: settings.descentSetpoint,
      bailout: settings.bailout,
      scrLoopVolume: settings.scrLoopVolume,
      scrMetabolicO2: settings.scrMetabolicO2,
      scrRuntimeMin: settings._scrRuntimeMin || 0
    };
    var segments = ccrSplitSegmentAtSetpoint(startDepth, endDepth, setpoint, surfP);
    var elapsed = 0;
    var _iterator = _createForOfIteratorHelper(segments),
      _step;
    try {
      for (_iterator.s(); !(_step = _iterator.n()).done;) {
        var seg = _step.value;
        var segTime = Math.abs(seg.toDepth - seg.fromDepth) / rate;
        if (!(segTime > 0)) continue;
        var pAmbStart = surfP + seg.fromDepth / slp;
        var pAmbEnd = surfP + seg.toDepth / slp;
        var pressureRate = (pAmbEnd - pAmbStart) / segTime;
        var ascending = seg.toDepth < seg.fromDepth;
        var endpointDepth = ascending ? seg.toDepth : seg.fromDepth;
        var segSP = ccrSetpointAtDepth(endpointDepth, ccr, surfP, phase || (ascending ? 'deco' : 'descent'));
        var params = ccrSchreinerParams(pAmbStart, segSP, o2Frac, heFrac, pressureRate, _objectSpread(_objectSpread({}, ccr), {}, {
          setpoint: segSP
        }), settings);
        for (var i = 0; i < NC; i++) {
          var kN2 = Math.LN2 / ZHL16C_N2[i].ht;
          var kHe = Math.LN2 / ZHL16C_He[i].ht;
          state.tissues[i].pN2 = params.inspN2Start + params.rN2 * (segTime - 1 / kN2) - (params.inspN2Start - state.tissues[i].pN2 - params.rN2 / kN2) * Math.exp(-kN2 * segTime);
          state.tissues[i].pHe = params.inspHeStart + params.rHe * (segTime - 1 / kHe) - (params.inspHeStart - state.tissues[i].pHe - params.rHe / kHe) * Math.exp(-kHe * segTime);
        }
        elapsed += segTime;
      }
    } catch (err) {
      _iterator.e(err);
    } finally {
      _iterator.f();
    }
    return elapsed || time;
  }
  function getWeightedAllowableGradient(state, i, pHe, pN2, useDecoGradients) {
    var gradHe = useDecoGradients ? state.decoGradientHe[i] : state.allowableGradientHe[i];
    var gradN2 = useDecoGradients ? state.decoGradientN2[i] : state.allowableGradientN2[i];
    var gasLoading = pHe + pN2;
    if (gasLoading > 0) {
      return (gradHe * pHe + gradN2 * pN2) / gasLoading;
    }
    return Math.min(gradHe, gradN2);
  }
  function calcCrushing(state, settings) {
    var surfP = getSurfacePressure(settings);
    var pMaxAmb = state.maxAmbientPressure;
    var pCrush = Math.max(0, pMaxAmb - surfP);
    for (var i = 0; i < NC; i++) {
      state.maxCrushingPressureN2[i] = Math.max(state.maxCrushingPressureN2[i], pCrush);
      state.maxCrushingPressureHe[i] = Math.max(state.maxCrushingPressureHe[i], pCrush);
    }
  }
  function calcCrushRadius(r0, pCrush) {
    if (!(r0 > 0)) return 0;
    var twoGammaC = 2.0 * GAMMA_C / r0 / 1.0e5;
    if (twoGammaC + pCrush <= 0) return r0;
    return r0 * twoGammaC / (twoGammaC + pCrush);
  }
  function applyNuclearRegeneration(state, diveTime) {
    var regenFactor = Math.exp(-diveTime / REGEN_TIME_MIN);
    for (var i = 0; i < NC; i++) {
      var crushN2 = state.maxCrushingPressureN2[i];
      var crushHe = state.maxCrushingPressureHe[i];
      var baseN2 = state.adjustedCritRadiiN2[i];
      var baseHe = state.adjustedCritRadiiHe[i];
      var endingRadiusN2 = calcCrushRadius(baseN2, crushN2);
      var endingRadiusHe = calcCrushRadius(baseHe, crushHe);
      var regeneratedN2 = baseN2 + (endingRadiusN2 - baseN2) * regenFactor;
      var regeneratedHe = baseHe + (endingRadiusHe - baseHe) * regenFactor;
      state.regeneratedRadiiN2[i] = regeneratedN2;
      state.regeneratedRadiiHe[i] = regeneratedHe;
      var ratioN2Den = regeneratedN2 * (baseN2 - endingRadiusN2);
      var ratioHeDen = regeneratedHe * (baseHe - endingRadiusHe);
      var ratioN2 = Math.abs(ratioN2Den) > 1e-18 ? endingRadiusN2 * (baseN2 - regeneratedN2) / ratioN2Den : 1;
      var ratioHe = Math.abs(ratioHeDen) > 1e-18 ? endingRadiusHe * (baseHe - regeneratedHe) / ratioHeDen : 1;
      state.adjustedCrushingPressureN2[i] = crushN2 * ratioN2;
      state.adjustedCrushingPressureHe[i] = crushHe * ratioHe;
    }
  }
  function calcSurfacePhaseVolumeTime(state, settings) {
    var surfP = getSurfacePressure(settings);
    var surfaceInspiredN2 = 0.7902 * (surfP - getWaterVaporPressure(settings));
    for (var i = 0; i < NC; i++) {
      var pHe = state.tissues[i].pHe;
      var pN2 = state.tissues[i].pN2;
      var denomBase = pHe + pN2 - surfaceInspiredN2;
      if (pN2 > surfaceInspiredN2) {
        state.surfacePhaseVolumeTime[i] = (pHe / (Math.LN2 / ZHL16C_He[i].ht) + (pN2 - surfaceInspiredN2) / (Math.LN2 / ZHL16C_N2[i].ht)) / Math.max(denomBase, Math.max(0.001, pN2 * 1e-4));
      } else if (pN2 <= surfaceInspiredN2 && pHe + pN2 >= surfaceInspiredN2 && pHe > 0) {
        var kHe = Math.LN2 / ZHL16C_He[i].ht;
        var kN2 = Math.LN2 / ZHL16C_N2[i].ht;
        var logArg = (surfaceInspiredN2 - pN2) / pHe;
        if (logArg <= 0 || Math.abs(kN2 - kHe) < 1e-12) {
          state.surfacePhaseVolumeTime[i] = 0;
          continue;
        }
        var decayTime = Math.log(logArg) / (kN2 - kHe);
        if (decayTime < 0) {
          state.surfacePhaseVolumeTime[i] = 0;
          continue;
        }
        var integral = pHe / kHe * (1 - Math.exp(-kHe * decayTime)) + (pN2 - surfaceInspiredN2) / kN2 * (1 - Math.exp(-kN2 * decayTime));
        state.surfacePhaseVolumeTime[i] = integral / Math.max(denomBase, Math.max(0.001, pN2 * 1e-4));
      } else {
        state.surfacePhaseVolumeTime[i] = 0;
      }
    }
  }
  function calcCriticalVolume(state, decoPhaseVolumeTime) {
    var lambdaPaMin = CRIT_VOLUME_LAMBDA_FSW_MIN / 32.0 * 101325.0;
    for (var i = 0; i < NC; i++) {
      var phaseVolumeTime = decoPhaseVolumeTime + state.surfacePhaseVolumeTime[i];
      if (phaseVolumeTime <= 0) continue;
      var adjCrushHePa = state.adjustedCrushingPressureHe[i] * 1.0e5;
      var initAllowHePa = state.initialAllowableGradientHe[i] * 1.0e5;
      var bHe = initAllowHePa + lambdaPaMin * GAMMA / (GAMMA_C * phaseVolumeTime);
      var cHe = GAMMA * GAMMA * lambdaPaMin * adjCrushHePa / (GAMMA_C * GAMMA_C * phaseVolumeTime);
      var discHe = Math.max(0, bHe * bHe - 4.0 * cHe);
      state.allowableGradientHe[i] = (bHe + Math.sqrt(discHe)) / 2.0 / 1.0e5;
      var adjCrushN2Pa = state.adjustedCrushingPressureN2[i] * 1.0e5;
      var initAllowN2Pa = state.initialAllowableGradientN2[i] * 1.0e5;
      var bN2 = initAllowN2Pa + lambdaPaMin * GAMMA / (GAMMA_C * phaseVolumeTime);
      var cN2 = GAMMA * GAMMA * lambdaPaMin * adjCrushN2Pa / (GAMMA_C * GAMMA_C * phaseVolumeTime);
      var discN2 = Math.max(0, bN2 * bN2 - 4.0 * cN2);
      state.allowableGradientN2[i] = (bN2 + Math.sqrt(discN2)) / 2.0 / 1.0e5;
    }
  }
  function calcStartOfDecoZone(state, startingDepth, ascentRate, o2Frac, heFrac, settings, setpoint) {
    var slp = getSLP(settings);
    var surfP = getSurfacePressure(settings);
    var startAmb = getAmbientPressure(startingDepth, settings);
    var pressureRate = -Math.abs(ascentRate) / slp;
    var ccr = {
      circuit: settings.circuit || 'OC',
      setpoint: setpoint,
      descentSetpoint: settings.descentSetpoint,
      bailout: settings.bailout,
      scrLoopVolume: settings.scrLoopVolume,
      scrMetabolicO2: settings.scrMetabolicO2,
      scrRuntimeMin: settings._scrRuntimeMin || 0
    };
    var params = ccrSchreinerParams(startAmb, setpoint, o2Frac, heFrac, pressureRate, ccr, settings);
    var inspiredHe0 = params.inspHeStart;
    var inspiredN20 = params.inspN2Start;
    var heRate = params.rHe;
    var n2Rate = params.rN2;
    var lowBound = 0;
    var highBound = Math.max(0, -startAmb / pressureRate);
    var depthStart = 0;
    for (var i = 0; i < NC; i++) {
      var pHe0 = state.tissues[i].pHe;
      var pN20 = state.tissues[i].pN2;
      var f0 = pHe0 + pN20 + CONSTANT_PRESSURE_OTHER_GASES - startAmb;
      var pHeHi = schreiner(inspiredHe0, heRate, highBound, ZHL16C_He[i].ht, pHe0);
      var pN2Hi = schreiner(inspiredN20, n2Rate, highBound, ZHL16C_N2[i].ht, pN20);
      var fHi = pHeHi + pN2Hi + CONSTANT_PRESSURE_OTHER_GASES;
      if (f0 * fHi >= 0) continue;
      var t = f0 < 0 ? lowBound : highBound;
      var diff = f0 < 0 ? highBound - lowBound : lowBound - highBound;
      for (var iter = 0; iter < 100; iter++) {
        diff *= 0.5;
        var midT = t + diff;
        var midHe = schreiner(inspiredHe0, heRate, midT, ZHL16C_He[i].ht, pHe0);
        var midN2 = schreiner(inspiredN20, n2Rate, midT, ZHL16C_N2[i].ht, pN20);
        var midAmb = startAmb + pressureRate * midT;
        var fMid = midHe + midN2 + CONSTANT_PRESSURE_OTHER_GASES - midAmb;
        if (fMid <= 0) t = midT;
        if (Math.abs(diff) < 1e-3 || fMid === 0) break;
      }
      var compartmentDepth = Math.max(0, startAmb + pressureRate * t - surfP) * slp;
      depthStart = Math.max(depthStart, compartmentDepth);
    }
    return depthStart;
  }
  function projectedAscent(state, startingDepth, ascentRate, stopDepth, stepSize, o2Frac, heFrac, settings, setpoint) {
    var slp = getSLP(settings);
    var surfP = getSurfacePressure(settings);
    var startAmb = getAmbientPressure(startingDepth, settings);
    var pressureRate = -Math.abs(ascentRate) / slp;
    var ccr = {
      circuit: settings.circuit || 'OC',
      setpoint: setpoint,
      descentSetpoint: settings.descentSetpoint,
      bailout: settings.bailout,
      scrLoopVolume: settings.scrLoopVolume,
      scrMetabolicO2: settings.scrMetabolicO2,
      scrRuntimeMin: settings._scrRuntimeMin || 0
    };
    var params = ccrSchreinerParams(startAmb, setpoint, o2Frac, heFrac, pressureRate, ccr, settings);
    var inspiredHe0 = params.inspHeStart;
    var inspiredN20 = params.inspN2Start;
    var heRate = params.rHe;
    var n2Rate = params.rN2;
    var candidateStop = stopDepth;
    while (candidateStop > 0) {
      var endAmb = getAmbientPressure(candidateStop, settings);
      var segmentTime = (endAmb - startAmb) / pressureRate;
      var violated = false;
      for (var i = 0; i < NC; i++) {
        var tempHe = schreiner(inspiredHe0, heRate, segmentTime, ZHL16C_He[i].ht, state.tissues[i].pHe);
        var tempN2 = schreiner(inspiredN20, n2Rate, segmentTime, ZHL16C_N2[i].ht, state.tissues[i].pN2);
        var allowGrad = getWeightedAllowableGradient(state, i, tempHe, tempN2, false);
        var allowableLoading = endAmb + allowGrad - CONSTANT_PRESSURE_OTHER_GASES;
        if (tempHe + tempN2 > allowableLoading) {
          candidateStop += stepSize;
          violated = true;
          break;
        }
      }
      if (!violated) return candidateStop;
    }
    return stopDepth;
  }
  var VPM_CRITICAL_RADIUS_FACTOR = [1.00, 1.05, 1.12, 1.22, 1.35, 1.50];
  function setCriticalRadiiForConservatism(state, conservatism, settings) {
    if (state._conservatismRadiiApplied) return;
    var consIdx = Math.max(0, Math.min(5, Math.round(conservatism || 0)));
    var factor = VPM_CRITICAL_RADIUS_FACTOR[consIdx];
    var baseFactor = VPM_CRITICAL_RADIUS_FACTOR[0];
    var surfP = settings ? getSurfacePressure(settings) : 1.01325;
    var altFactor = Math.pow(1.01325 / surfP, 1.0 / 3.0);
    var rN2 = INITIAL_RADIUS_N2 * altFactor * factor;
    var rHe = INITIAL_RADIUS_He * altFactor * factor;
    var scaleCarried = !!state._bubbleCarryApplied;
    var scale = factor / baseFactor;
    for (var i = 0; i < NC; i++) {
      if (scaleCarried) {
        if (scale !== 1) {
          state.critRadiiN2[i] *= scale;
          state.critRadiiHe[i] *= scale;
          state.adjustedCritRadiiN2[i] *= scale;
          state.adjustedCritRadiiHe[i] *= scale;
          state.regeneratedRadiiN2[i] *= scale;
          state.regeneratedRadiiHe[i] *= scale;
        }
      } else {
        state.critRadiiN2[i] = rN2;
        state.critRadiiHe[i] = rHe;
        state.adjustedCritRadiiN2[i] = rN2;
        state.adjustedCritRadiiHe[i] = rHe;
        state.regeneratedRadiiN2[i] = rN2;
        state.regeneratedRadiiHe[i] = rHe;
      }
    }
    state._conservatismRadiiApplied = true;
    if (scaleCarried) state._bubbleCarryApplied = false;
  }
  function calcAllowableGradients(state, model, settings, _conservatism) {
    // Inter-level conservatism relaxation is applied via bubble-radius scaling in
    // runInterLevelDecoAscent before this call; gradients derive from radii state.
    for (var i = 0; i < NC; i++) {
      var rN2 = state.regeneratedRadiiN2[i];
      var baseGradN2 = twoGammaOverR(GAMMA, rN2) * (GAMMA_C - GAMMA) / GAMMA_C;
      var rHe = state.regeneratedRadiiHe[i];
      var baseGradHe = twoGammaOverR(GAMMA, rHe) * (GAMMA_C - GAMMA) / GAMMA_C;
      state.initialAllowableGradientN2[i] = baseGradN2;
      state.initialAllowableGradientHe[i] = baseGradHe;
      state.allowableGradientN2[i] = state.initialAllowableGradientN2[i];
      state.allowableGradientHe[i] = state.initialAllowableGradientHe[i];
      state.decoGradientN2[i] = state.allowableGradientN2[i];
      state.decoGradientHe[i] = state.allowableGradientHe[i];
    }
  }
  function solveBubbleRadius(a, b, c, lowBound, highBound) {
    var low = lowBound;
    var high = Math.max(highBound, lowBound);
    for (var iter = 0; iter < 80; iter++) {
      var mid = (low + high) * 0.5;
      var fMid = mid * (mid * (a * mid - b)) - c;
      if (Math.abs(fMid) < 1e-18 || Math.abs(high - low) < 1e-14) return mid;
      var fLow = low * (low * (a * low - b)) - c;
      if (fLow <= 0 && fMid <= 0 || fLow >= 0 && fMid >= 0) {
        low = mid;
      } else {
        high = mid;
      }
    }
    return (low + high) * 0.5;
  }
  function boyleLawCompensation(state, firstStopDepth, decoStopDepth, stepSize, settings) {
    var surfP = getSurfacePressure(settings);
    var unitsFactor = getSLP(settings);
    var firstStop = Math.max(0, firstStopDepth || state.firstStopDepth || 0);
    var currentStop = Math.max(0, decoStopDepth || firstStop);
    var nextStop = Math.max(0, currentStop - (stepSize || 0));
    var ambFirstPa = (surfP + firstStop / unitsFactor) * 101325.0;
    var ambNextPa = (surfP + nextStop / unitsFactor) * 101325.0;
    var safeAmbNextPa = Math.max(ambNextPa, 1.0);
    var rootFactor = Math.pow(Math.max(ambFirstPa / safeAmbNextPa, 1.0), 1.0 / 3.0);
    for (var i = 0; i < NC; i++) {
      var gradHePa = Math.max(1e-9, state.allowableGradientHe[i] / unitsFactor * 101325.0);
      var radiusHe = 2.0 * GAMMA / gradHePa;
      var cHe = (ambFirstPa + 2.0 * GAMMA / radiusHe) * radiusHe * radiusHe * radiusHe;
      var endRadiusHe = solveBubbleRadius(safeAmbNextPa, -2.0 * GAMMA, cHe, radiusHe, radiusHe * rootFactor);
      state.decoGradientHe[i] = 2.0 * GAMMA / endRadiusHe / 101325.0 * unitsFactor;
      var gradN2Pa = Math.max(1e-9, state.allowableGradientN2[i] / unitsFactor * 101325.0);
      var radiusN2 = 2.0 * GAMMA / gradN2Pa;
      var cN2 = (ambFirstPa + 2.0 * GAMMA / radiusN2) * radiusN2 * radiusN2 * radiusN2;
      var endRadiusN2 = solveBubbleRadius(safeAmbNextPa, -2.0 * GAMMA, cN2, radiusN2, radiusN2 * rootFactor);
      state.decoGradientN2[i] = 2.0 * GAMMA / endRadiusN2 / 101325.0 * unitsFactor;
    }
    state.useDecoGradients = true;
  }
  function extendedCompensation(state, settings) {
    var surfP = getSurfacePressure(settings);
    var pMaxAmb = state.maxAmbientPressure;
    var maxDepthBar = pMaxAmb - surfP;
    var threshold = 6.0;
    if (maxDepthBar <= threshold) return;
    var extFactor = 1.0 - (maxDepthBar - threshold) * 0.02;
    var clampedFactor = Math.max(0.5, Math.min(1.0, extFactor));
    for (var i = 0; i < NC; i++) {
      state.decoGradientN2[i] *= clampedFactor;
      state.decoGradientHe[i] *= clampedFactor;
    }
  }
  function applyExtendedAfterBoyle(state, model, settings) {
    if (model === 'VPMBE' && state.useDecoGradients) {
      extendedCompensation(state, settings);
    }
  }
  function getVPMCeiling(state, settings) {
    var surfP = getSurfacePressure(settings);
    var slp = getSLP(settings);
    var maxCeilingP = 0;
    for (var i = 0; i < NC; i++) {
      var pN2 = state.tissues[i].pN2;
      var pHe = state.tissues[i].pHe;
      var pTotal = pN2 + pHe;
      var allowGrad = getWeightedAllowableGradient(state, i, pHe, pN2, state.useDecoGradients);
      var ceilingP = pTotal + CONSTANT_PRESSURE_OTHER_GASES - allowGrad;
      if (ceilingP > maxCeilingP) {
        maxCeilingP = ceilingP;
      }
    }
    var ceilingDepth = (maxCeilingP - surfP) * slp;
    return Math.max(0, ceilingDepth);
  }
  function applyGFSurfacing(state, stopDepth, firstStopDepth, gfHi, settings) {
    if (firstStopDepth <= 0) return;
    var fraction = stopDepth / firstStopDepth;
    var surfP = getSurfacePressure(settings);
    var pAmb = getAmbientPressure(stopDepth, settings);
    var gf = gfHi / 100;
    for (var i = 0; i < NC; i++) {
      var pN2 = state.tissues[i].pN2;
      var pHe = state.tissues[i].pHe;
      var pTotal = pN2 + pHe;
      var a = void 0,
        b = void 0;
      if (pTotal > 0) {
        a = (pN2 * ZHL16C_N2[i].a + pHe * ZHL16C_He[i].a) / pTotal;
        b = (pN2 * ZHL16C_N2[i].b + pHe * ZHL16C_He[i].b) / pTotal;
      } else {
        a = ZHL16C_N2[i].a;
        b = ZHL16C_N2[i].b;
      }
      var mValue = a + pAmb / b;
      var buhlGrad = gf * (mValue - pAmb);
      var vpmGradN2 = void 0;
      var vpmGradHe = void 0;
      if (state.useDecoGradients) {
        vpmGradN2 = state.decoGradientN2[i];
        vpmGradHe = state.decoGradientHe[i];
      } else {
        vpmGradN2 = state.allowableGradientN2[i];
        vpmGradHe = state.allowableGradientHe[i];
      }
      var blendedGradN2 = vpmGradN2 * fraction + buhlGrad * (1 - fraction);
      var blendedGradHe = vpmGradHe * fraction + buhlGrad * (1 - fraction);
      state.allowableGradientN2[i] = blendedGradN2;
      state.allowableGradientHe[i] = blendedGradHe;
    }
  }
  function isClearToAscendVPM(state, targetDepth, firstStopDepth, model, settings) {
    var pAmb = getAmbientPressure(targetDepth, settings);
    for (var i = 0; i < NC; i++) {
      var pN2 = state.tissues[i].pN2;
      var pHe = state.tissues[i].pHe;
      var pTotal = pN2 + pHe;
      var allowGrad = getWeightedAllowableGradient(state, i, pHe, pN2, state.useDecoGradients);
      if (pTotal + CONSTANT_PRESSURE_OTHER_GASES - allowGrad > pAmb) return false;
    }
    return true;
  }
  function getGasPpO2Limit(gas, settings) {
    var o2pct = gas.o2 * 100;
    if (settings.ppO2Low && settings.ppO2Mid && settings.ppO2High) {
      if (o2pct < 28) return settings.ppO2Low; // <28%: lean → 1.4
      if (o2pct < 45) return settings.ppO2Mid; // 28–44%: mid → 1.5
      if (o2pct < 100) return settings.ppO2High; // ≥45%: rich → 1.6
    }
    return settings.ppO2Deco || 1.6;
  }
  function getManualDecoSwitchDepth(gas) {
    if (!gas || !(gas.depthOverrideOn === true || gas.depthOverrideOn === 1 || gas.depthOverrideOn === '1')) {
      return null;
    }
    var depth = Number(gas.depthOverride);
    return Number.isFinite(depth) && depth > 0 ? depth : null;
  }
  function selectDecoGas(depth, decoGases, ppO2Limit, settings) {
    var bestGas = null;
    var bestO2 = 0;
    var o2MaxDepth = settings.o2MaxDepth || 6;
    var _iterator2 = _createForOfIteratorHelper(decoGases),
      _step2;
    try {
      for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
        var gas = _step2.value;
        var manualSwitchDepth = getManualDecoSwitchDepth(gas);
        if (manualSwitchDepth != null) {
          if (depth <= manualSwitchDepth && gas.o2 > bestO2) {
            bestO2 = gas.o2;
            bestGas = gas;
          }
          continue;
        }
        var pAmb = getAmbientPressure(depth, settings);
        var ppO2 = gas.o2 * pAmb;
        var limit = getGasPpO2Limit(gas, settings);
        if (gas.o2 >= 0.995 && depth > o2MaxDepth) continue;
        // Pure O2: allowed at o2MaxDepth if allowO2AtMOD is on
        var pureO2Ok = gas.o2 >= 0.995 && settings.allowO2AtMOD !== false;
        if (pureO2Ok || ppO2 <= limit) {
          if (gas.o2 > bestO2) {
            bestO2 = gas.o2;
            bestGas = gas;
          }
        }
      }
    } catch (err) {
      _iterator2.e(err);
    } finally {
      _iterator2.f();
    }
    return bestGas;
  }
  function roundUpToStop(depth, stepSize) {
    return Math.ceil((depth + 1e-9) / stepSize) * stepSize;
  }
  function getEffectiveSetpoint(level, isCCR, settings, depthM, phase) {
    var _settings$descentSetp, _settings$bottomSetpo, _ref, _settings$decoSetpoin, _settings$setpoint;
    if (!isCCR) return 0;
    if (level && (level.oc || level.scr)) return 0;
    if (settings && settings.bailout) return 0;
    var surfP = settings ? getSurfacePressure(settings) : altSurfaceP;
    var ccr = {
      circuit: (settings === null || settings === void 0 ? void 0 : settings.circuit) || 'CCR',
      descentSetpoint: (_settings$descentSetp = settings === null || settings === void 0 ? void 0 : settings.descentSetpoint) !== null && _settings$descentSetp !== void 0 ? _settings$descentSetp : 0.7,
      bottomSetpoint: (_settings$bottomSetpo = settings === null || settings === void 0 ? void 0 : settings.bottomSetpoint) !== null && _settings$bottomSetpo !== void 0 ? _settings$bottomSetpo : 1.2,
      decoSetpoint: (_ref = (_settings$decoSetpoin = settings === null || settings === void 0 ? void 0 : settings.decoSetpoint) !== null && _settings$decoSetpoin !== void 0 ? _settings$decoSetpoin : settings === null || settings === void 0 ? void 0 : settings.setpoint) !== null && _ref !== void 0 ? _ref : 1.3,
      setpoint: (_settings$setpoint = settings === null || settings === void 0 ? void 0 : settings.setpoint) !== null && _settings$setpoint !== void 0 ? _settings$setpoint : 1.3,
      bailout: false
    };
    return ccrSetpointAtDepth(depthM != null ? depthM : 0, ccr, surfP, phase);
  }
  function vpmSetpointAtDepth(depthM, phase, forcedOC, vpmSettings) {
    var _cfg$descentSetpoint, _cfg$bottomSetpoint, _ref2, _cfg$decoSetpoint, _cfg$setpoint;
    var cfg = vpmSettings || {};
    if (forcedOC || !isRebreatherCircuit(cfg.circuit) || cfg.bailout) return 0;
    return ccrSetpointAtDepth(depthM != null ? depthM : 0, {
      circuit: cfg.circuit || 'CCR',
      descentSetpoint: (_cfg$descentSetpoint = cfg.descentSetpoint) !== null && _cfg$descentSetpoint !== void 0 ? _cfg$descentSetpoint : 0.7,
      bottomSetpoint: (_cfg$bottomSetpoint = cfg.bottomSetpoint) !== null && _cfg$bottomSetpoint !== void 0 ? _cfg$bottomSetpoint : 1.2,
      decoSetpoint: (_ref2 = (_cfg$decoSetpoint = cfg.decoSetpoint) !== null && _cfg$decoSetpoint !== void 0 ? _cfg$decoSetpoint : cfg.setpoint) !== null && _ref2 !== void 0 ? _ref2 : 1.3,
      setpoint: (_cfg$setpoint = cfg.setpoint) !== null && _cfg$setpoint !== void 0 ? _cfg$setpoint : 1.3,
      bailout: false
    }, getSurfacePressure(cfg), phase);
  }
  function vpmAccumPpo2(pAmb, sp, fO2, fHe, settings, depthM, useOC) {
    if (sp > 0) return Math.min(sp, pAmb);
    if (!useOC && String(settings.circuit || '').trim().toUpperCase() === 'PSCR' && !settings.bailout) {
      var ccr = ccrNormalize(_objectSpread(_objectSpread({}, settings), {}, {
        circuit: 'pSCR',
        bailout: false,
        scrRuntimeMin: settings._scrRuntimeMin || 0
      }));
      var b = zhlBundle();
      if (b && typeof b.getEffectivePpo2 === 'function') {
        syncZhlEnvFromSettings(settings);
        return b.getEffectivePpo2(pAmb, 0, fO2, ccr, depthM, fHe);
      }
      if (typeof getEffectivePpo2 === 'function') {
        return getEffectivePpo2(pAmb, 0, fO2, ccr, depthM, fHe);
      }
    }
    return fO2 * pAmb;
  }
  function calculateOTU(ppO2, time) {
    if (ppO2 <= 0.5) return 0;
    return time * Math.pow((ppO2 - 0.5) / 0.5, 0.8333);
  }
  // Fine-grained ppO2→CNS rate table (131 steps, 0.50–1.80 bar); aligned to NOAA band structure.
  var CNS_RATE_ANDROID = [0.120, 0.122, 0.125, 0.127, 0.129, 0.130, 0.132, 0.134, 0.135, 0.138, 0.140, 0.140, 0.140, 0.145, 0.150, 0.155, 0.160, 0.165, 0.170, 0.175, 0.180, 0.180, 0.180, 0.185, 0.185, 0.190, 0.200, 0.200, 0.210, 0.210, 0.220, 0.225, 0.230, 0.235, 0.240, 0.245, 0.250, 0.255, 0.260, 0.270, 0.280, 0.290, 0.290, 0.300, 0.300, 0.305, 0.310, 0.315, 0.320, 0.325, 0.330, 0.340, 0.350, 0.355, 0.360, 0.370, 0.380, 0.385, 0.400, 0.410, 0.420, 0.430, 0.435, 0.4375, 0.439, 0.440, 0.445, 0.455, 0.460, 0.465, 0.470, 0.475, 0.480, 0.495, 0.510, 0.515, 0.520, 0.530, 0.540, 0.550, 0.560, 0.565, 0.570, 0.590, 0.600, 0.610, 0.620, 0.630, 0.640, 0.645, 0.650, 0.660, 0.680, 0.695, 0.710, 0.720, 0.740, 0.770, 0.780, 0.800, 0.830, 0.880, 0.930, 0.980, 1.040, 1.110, 1.190, 1.320, 1.470, 1.800, 2.220, 2.500, 3.000, 3.500, 4.000, 4.500, 5.000, 6.000, 8.000, 9.000, 10.000, 11.000, 12.500, 15.000, 20.000, 21.000, 22.000, 25.000, 31.250, 40.000, 50.000];
  function getCNSRate(ppO2) {
    if (ppO2 < 0.50) return 0;
    if (ppO2 > 2.00) return 100.0;
    if (ppO2 > 1.80) return 50.0;
    var idx = Math.max(50, Math.min(180, Math.round(ppO2 * 100))) - 50;
    return CNS_RATE_ANDROID[idx];
  }
  function calculateCNS(ppO2, time) {
    if (ppO2 < 0.50) return 0;
    return time * getCNSRate(ppO2);
  }
  function vpmGasLabel(o2Pct, hePct) {
    var o2 = Math.round(Number(o2Pct) || 0);
    var he = Math.round(Number(hePct) || 0);
    if (o2 >= 100 && he === 0) return '100%';
    if (o2 === 21 && he === 0) return 'Air';
    return "".concat(String(o2).padStart(2, '0'), "/").concat(String(he).padStart(2, '0'));
  }
  function calculate(levels, decoGases, settings, model) {
    model = model || 'VPMB';
    settings = Object.assign({}, settings || {});
    if (settings.metric == null) settings.metric = true;
    if (!Array.isArray(levels) || levels.length === 0) {
      return {
        error: 'No bottom segments defined',
        code: 'INVALID_PROFILE',
        stops: [],
        plan: [],
        totalTime: 0,
        totalRuntime: 0
      };
    }
    if (typeof validateEngineInputs === 'function') {
      var validation = validateEngineInputs(levels, decoGases);
      if (!validation.ok) return engineValidationError(validation);
    }
    var s = settings;
    if (isRebreatherCircuit(s.circuit || 'OC')) {
      if (typeof validateCcrCalculationInputs === 'function') {
        var ccrVal = validateCcrCalculationInputs(levels, s, decoGases);
        if (!ccrVal.ok) return engineValidationError(ccrVal);
      }
    }
    var vpmSettingsVal = validateVpmSettings(s);
    if (!vpmSettingsVal.ok) {
      return engineValidationError({
        errors: [{
          code: vpmSettingsVal.code,
          message: vpmSettingsVal.message
        }]
      });
    }
    var repStateVal = validateRepetitiveState(s, 16);
    if (!repStateVal.ok) {
      return engineValidationError({
        errors: [{
          code: repStateVal.code,
          message: repStateVal.message
        }]
      });
    }
    var safeDecoGases = Array.isArray(decoGases) ? decoGases.filter(function (g) {
      return g != null;
    }) : [];
    function ctxUseOCForPpo2(calcSettings) {
      return calcSettings.bailout || calcSettings.circuit !== 'pSCR';
    }
    function ctxOffLoop(ctx) {
      return !!(ctx.forcedOCMode || ctxUseOCForPpo2(settings));
    }
    var stepSize = settings.stepSize || (settings.metric ? 3 : 10);
    var lastStop = settings.lastStop || (settings.metric ? 3 : 10);
    var descentRate = settings.descentRate || (settings.metric ? 20 : 60);
    var ascentRate = settings.ascentRate || (settings.metric ? 10 : 30);
    var decoAscentRate = settings.decoAscentRate || (settings.metric ? 3 : 10);
    var surfaceAscentRate = settings.surfaceAscentRate || decoAscentRate;
    var ppO2Deco = settings.ppO2Deco || 1.6;
    var minStopTime = settings.minStopTime || 1;
    var conservatism = settings.conservatism || 0;
    var firstStop30sec = settings.firstStop30sec || false;
    var firstStopDoubleStep = settings.firstStopDoubleStep || false;
    var state = createVPMState(settings);
    setCriticalRadiiForConservatism(state, conservatism, settings);
    var normalizedDecoGases = safeDecoGases.map(function (g) {
      var f = gasFractionsFromPct(g.o2, g.he);
      return {
        o2: f.o2Frac,
        he: f.heFrac,
        label: vpmGasLabel(f.o2Pct, f.hePct),
        depthOverrideOn: g.depthOverrideOn === true || g.depthOverrideOn === 1 || g.depthOverrideOn === '1',
        depthOverride: g.depthOverride != null && g.depthOverride !== '' ? Number(g.depthOverride) : null
      };
    });
    var deepestLevelIndex = 0;
    for (var i = 1; i < levels.length; i++) {
      if (levels[i].depth > levels[deepestLevelIndex].depth) {
        deepestLevelIndex = i;
      }
    }
    var continuationLevels = [];
    if (deepestLevelIndex < levels.length - 1) {
      var monotonicContinuation = true;
      for (var _i5 = deepestLevelIndex + 1; _i5 < levels.length; _i5++) {
        if (levels[_i5].depth > levels[_i5 - 1].depth) {
          monotonicContinuation = false;
          break;
        }
      }
      if (monotonicContinuation) {
        continuationLevels = levels.slice(deepestLevelIndex + 1);
        levels = levels.slice(0, deepestLevelIndex + 1);
      }
    }
    var plan = [];
    var _origPush = plan.push;
    plan.push = function (seg) {
      try {
        if (!seg._tissues) {
          seg._tissues = state.tissues.map(function (t) {
            return {
              pN2: t.pN2,
              pHe: t.pHe
            };
          });
        }
        if (seg._cumOTU == null) seg._cumOTU = totalOTU;
        if (seg._cumCNS == null) seg._cumCNS = totalCNS;
        if (seg._ceilingDepth == null) {
          seg._ceilingDepth = Math.max(0, getVPMCeiling(state, settings));
        }
      } catch (e) {}
      return _origPush.call(this, seg);
    };
    var runtime = 0;
    var bottomPhaseRuntime = 0;
    var currentDepth = 0;
    var vpmStopCapFailedDepth = null;
    var vpmMaxStopMin = settings._vpmMaxStopMin != null && Number.isFinite(settings._vpmMaxStopMin) ? Math.max(0, settings._vpmMaxStopMin) : 180;
    function vpmStopCapError(stopDepth, partialPlan) {
      var plan = partialPlan || [];
      var depthDisp = settings.metric ? stopDepth : Math.round(stopDepth * 3.28084);
      var unitSuffix = settings.metric ? ' m' : ' ft';
      return {
        error: "VPM stop at ".concat(depthDisp).concat(unitSuffix, " exceeded safety limit without clearance \u2014 schedule invalid"),
        code: 'VPM_STOP_CAP',
        stops: plan,
        plan: plan,
        totalRuntime: plan.length ? plan[plan.length - 1].runtime || 0 : 0,
        totalTime: plan.length ? plan[plan.length - 1].runtime || 0 : 0,
        finalTissues: null,
        finalBubbleState: null
      };
    }
    var totalOTU = settings._preOTU || 0; // carry OTU from previous dive if repetitive
    var totalCNS = settings._preCNS || 0; // carry CNS (decayed) from previous dive if repetitive
    function addExposureToContext(ctx, fromDepth, toDepth, time) {
      if (time <= 0) return;
      var steps = Math.max(1, Math.ceil(Math.abs(toDepth - fromDepth)));
      var dt = time / steps;
      for (var _s = 0; _s < steps; _s++) {
        var frac = _s / steps;
        var depth = fromDepth + (toDepth - fromDepth) * frac;
        var pAmb = getAmbientPressure(depth, settings);
        settings._scrRuntimeMin = ctx.runtime + frac * time;
        var ppO2 = vpmAccumPpo2(pAmb, ctx.currentSP, ctx.currentO2, ctx.currentHe, settings, depth, ctxOffLoop(ctx));
        ctx.totalOTU += calculateOTU(ppO2, dt);
        ctx.totalCNS += calculateCNS(ppO2, dt);
      }
    }
    function addConstantExposure(ctx, depth, time) {
      if (time <= 0) return;
      var pAmb = getAmbientPressure(depth, settings);
      settings._scrRuntimeMin = ctx.runtime;
      var ppO2 = vpmAccumPpo2(pAmb, ctx.currentSP, ctx.currentO2, ctx.currentHe, settings, depth, ctxOffLoop(ctx));
      ctx.totalOTU += calculateOTU(ppO2, time);
      ctx.totalCNS += calculateCNS(ppO2, time);
    }
    function makeScheduleContext(baseState, startDepth, startRuntime, startOTU, startCNS, o2, he, gasLabel, sp, outPlan, forcedOCModeAtStart) {
      return {
        state: cloneVPMState(baseState),
        currentDepth: startDepth,
        runtime: startRuntime,
        totalOTU: startOTU,
        totalCNS: startCNS,
        currentO2: o2,
        currentHe: he,
        currentGasLabel: gasLabel,
        currentSP: sp,
        forcedOCMode: !!forcedOCModeAtStart,
        firstStopDepth: 0,
        plan: outPlan || null,
        continuationFinalPhase: false
      };
    }
    function appendPlan(ctx, segment) {
      if (!ctx.plan) return;
      try {
        if (segment.setpoint == null) {
          segment.setpoint = ctx.currentSP > 0 ? ctx.currentSP : 0;
        }
        segment._tissues = ctx.state.tissues.map(function (t) {
          return {
            pN2: t.pN2,
            pHe: t.pHe
          };
        });
        segment._cumOTU = ctx.totalOTU;
        segment._cumCNS = ctx.totalCNS;
        segment._ceilingDepth = Math.max(0, getVPMCeiling(ctx.state, settings));
      } catch (e) {}
      ctx.plan.push(segment);
    }
    function runAscentSegment(ctx, toDepth, rate, phase) {
      settings._scrRuntimeMin = ctx.runtime;
      var fromDepth = ctx.currentDepth;
      var midDepth = (fromDepth + toDepth) * 0.5;
      ctx.currentSP = vpmSetpointAtDepth(midDepth, phase || 'deco', ctx.forcedOCMode, settings);
      var segTime = loadTissuesLinear(ctx.state, fromDepth, toDepth, rate, ctx.currentO2, ctx.currentHe, settings, ctx.currentSP, phase);
      ctx.runtime += segTime;
      addExposureToContext(ctx, fromDepth, toDepth, segTime);
      appendPlan(ctx, {
        type: 'ascent',
        startDepth: fromDepth,
        endDepth: toDepth,
        time: Math.round(segTime * 10) / 10,
        runtime: Math.round(ctx.runtime * 10) / 10,
        gas: ctx.currentGasLabel,
        o2: Math.round(ctx.currentO2 * 100),
        he: Math.round(ctx.currentHe * 100)
      });
      ctx.currentDepth = toDepth;
      return segTime;
    }
    function vpmMdpMinAtDepth(depthM) {
      var mdp = settings.minDecoProfile;
      if (!mdp || !mdp.enabled) return 0;
      if (Math.abs(depthM - 9) < 0.25 && (mdp.m9 || 0) > 0) return mdp.m9;
      if (Math.abs(depthM - 6) < 0.25 && (mdp.m6 || 0) > 0) return mdp.m6;
      return 0;
    }
    function vpmMdpSurfaceFirstStopDepth(fromDepthM) {
      var mdp = settings.minDecoProfile;
      if (!mdp || !mdp.enabled) return 0;
      if ((mdp.m9 || 0) > 0 && fromDepthM > 8.75) return 9;
      if ((mdp.m6 || 0) > 0 && fromDepthM > 5.75) return 6;
      return 0;
    }
    function vpmNextMdpStopDepth(stopDepth, targetDepth, stepSize, toSurface) {
      var mdp = settings.minDecoProfile;
      if (toSurface) {
        if (mdp && mdp.enabled) {
          if (stopDepth > 9.25 && (mdp.m9 || 0) > 0) return 9;
          if (stopDepth > 6.25 && (mdp.m6 || 0) > 0) return 6;
        }
        return 0;
      }
      var next = Math.max(targetDepth, stopDepth - stepSize);
      if (mdp && mdp.enabled) {
        if (stopDepth > 9.25 && next < 8.75 && (mdp.m9 || 0) > 0) return 9;
        if (stopDepth > 6.25 && next < 5.75 && (mdp.m6 || 0) > 0) return 6;
      }
      return next;
    }
    function runRoundedDecoStop(ctx, stopDepth, nextStop) {
      if (settings._vpmTestForceStopCap) {
        vpmStopCapFailedDepth = stopDepth;
        return;
      }
      settings._scrRuntimeMin = ctx.runtime;
      ctx.currentSP = vpmSetpointAtDepth(stopDepth, 'deco', ctx.forcedOCMode, settings);
      var effectiveMinStop = firstStop30sec && stopDepth === ctx.firstStopDepth ? 0.5 : minStopTime;
      var useWholeMinStops = settings.wholeMinStops !== false;
      var useOneMinuteGrid = settings.wholeMinStops === true;
      var roundedRuntime = ctx.runtime;
      if (useWholeMinStops && !useOneMinuteGrid) {
        roundedRuntime = ctx.continuationFinalPhase && stopDepth <= 12 ? ctx.runtime : Math.round(ctx.runtime / effectiveMinStop + 0.5) * effectiveMinStop;
      } else if (useOneMinuteGrid) {
        roundedRuntime = ctx.continuationFinalPhase && stopDepth <= 12 ? ctx.runtime : Math.ceil((ctx.runtime - 1e-9) / 1) * 1;
      }
      var stopStep = useOneMinuteGrid ? 1 : effectiveMinStop;
      var segmentTime = roundedRuntime - ctx.runtime;
      ctx.runtime = roundedRuntime;
      var totalStopTime = segmentTime;
      var pAmb = getAmbientPressure(stopDepth, settings);
      var ccr = {
        circuit: settings.circuit || 'OC',
        setpoint: ctx.currentSP,
        descentSetpoint: settings.descentSetpoint,
        bailout: settings.bailout,
        scrLoopVolume: settings.scrLoopVolume,
        scrMetabolicO2: settings.scrMetabolicO2,
        scrRuntimeMin: ctx.runtime
      };
      var insp = getInspiredInertPressures(pAmb, ctx.currentSP, ctx.currentO2, ctx.currentHe, ccr);
      var inspHe = insp.pHe;
      var inspN2 = insp.pN2;
      var stopLoopStart = typeof performance !== 'undefined' && performance.now ? performance.now() : 0;
      for (var guard = 0; guard < 1000; guard++) {
        if (guard > 0 && guard % 50 === 0 && stopLoopStart > 0 && performance.now() - stopLoopStart > 10000) {
          vpmStopCapFailedDepth = stopDepth;
          return;
        }
        for (var _i6 = 0; _i6 < NC; _i6++) {
          ctx.state.tissues[_i6].pHe = haldane(ctx.state.tissues[_i6].pHe, inspHe, ZHL16C_He[_i6].ht, segmentTime);
          ctx.state.tissues[_i6].pN2 = haldane(ctx.state.tissues[_i6].pN2, inspN2, ZHL16C_N2[_i6].ht, segmentTime);
        }
        var decoCeiling = getVPMCeiling(ctx.state, settings);
        var clearTolerance = ctx.continuationFinalPhase && stopDepth <= 6 ? nextStop <= 0 ? 0.35 : 0.1 : 1e-9;
        if (decoCeiling <= nextStop + clearTolerance) break;
        segmentTime = stopStep;
        totalStopTime += stopStep;
        ctx.runtime += stopStep;
      }
      if (useOneMinuteGrid && totalStopTime > 0) {
        var snapped = Math.max(effectiveMinStop, Math.ceil((totalStopTime - 1e-9) / 1) * 1);
        var roundDelta = snapped - totalStopTime;
        if (roundDelta > 1e-9) {
          for (var _i7 = 0; _i7 < NC; _i7++) {
            ctx.state.tissues[_i7].pHe = haldane(ctx.state.tissues[_i7].pHe, inspHe, ZHL16C_He[_i7].ht, roundDelta);
            ctx.state.tissues[_i7].pN2 = haldane(ctx.state.tissues[_i7].pN2, inspN2, ZHL16C_N2[_i7].ht, roundDelta);
          }
          ctx.runtime += roundDelta;
          totalStopTime = snapped;
        }
      }
      var mdpMin = vpmMdpMinAtDepth(stopDepth);
      if (mdpMin > totalStopTime) {
        var mdpExtra = mdpMin - totalStopTime;
        for (var _i8 = 0; _i8 < NC; _i8++) {
          ctx.state.tissues[_i8].pHe = haldane(ctx.state.tissues[_i8].pHe, inspHe, ZHL16C_He[_i8].ht, mdpExtra);
          ctx.state.tissues[_i8].pN2 = haldane(ctx.state.tissues[_i8].pN2, inspN2, ZHL16C_N2[_i8].ht, mdpExtra);
        }
        ctx.runtime += mdpExtra;
        totalStopTime = mdpMin;
      }
      var finalCeiling = getVPMCeiling(ctx.state, settings);
      var clearToleranceFinal = ctx.continuationFinalPhase && stopDepth <= 6 ? nextStop <= 0 ? 0.35 : 0.1 : 1e-9;
      if (finalCeiling > nextStop + clearToleranceFinal && totalStopTime >= vpmMaxStopMin) {
        vpmStopCapFailedDepth = stopDepth;
        appendPlan(ctx, {
          type: 'stop',
          depth: stopDepth,
          time: totalStopTime,
          runtime: Math.round(ctx.runtime * 10) / 10,
          gas: ctx.currentGasLabel,
          o2: Math.round(ctx.currentO2 * 100),
          he: Math.round(ctx.currentHe * 100),
          vpmStopCapHit: true
        });
        return;
      }
      if (totalStopTime <= 1e-9) return 0;
      settings._scrRuntimeMin = ctx.runtime;
      var ppO2Stop = vpmAccumPpo2(pAmb, ctx.currentSP, ctx.currentO2, ctx.currentHe, settings, stopDepth, ctxOffLoop(ctx));
      ctx.totalOTU += calculateOTU(ppO2Stop, totalStopTime);
      ctx.totalCNS += calculateCNS(ppO2Stop, totalStopTime);
      appendPlan(ctx, {
        type: 'stop',
        depth: stopDepth,
        time: totalStopTime,
        runtime: Math.round(ctx.runtime * 10) / 10,
        gas: ctx.currentGasLabel,
        o2: Math.round(ctx.currentO2 * 100),
        he: Math.round(ctx.currentHe * 100)
      });
      return totalStopTime;
    }
    function maybeSwitchDecoGas(ctx, depth) {
      if (isRebreatherCircuit(settings.circuit) && !settings.bailout && !ctx.forcedOCMode) return;
      var decoGas = selectDecoGas(depth, normalizedDecoGases, ppO2Deco, settings);
      if (!decoGas) return;
      var pAmb = getAmbientPressure(depth, settings);
      var ocPpO2 = decoGas.o2 * pAmb;
      var ccrPpO2 = ctx.currentSP > 0 ? Math.min(ctx.currentSP, pAmb) : 0;
      if (decoGas.o2 > ctx.currentO2 || ctx.currentSP > 0 && ocPpO2 > ccrPpO2) {
        ctx.currentO2 = decoGas.o2;
        ctx.currentHe = decoGas.he;
        ctx.currentGasLabel = decoGas.label;
        ctx.currentSP = 0;
      }
    }
    function calcIntermediateFirstStopDepth(ctx, targetDepth) {
      var rawCeiling = getVPMCeiling(ctx.state, settings);
      if (rawCeiling <= targetDepth) return 0;
      var firstStopDepth = Math.floor(rawCeiling / stepSize) * stepSize;
      if (firstStopDoubleStep && firstStopDepth > Math.max(lastStop, targetDepth)) {
        var doubleStep = stepSize * 2;
        firstStopDepth = Math.ceil(rawCeiling / doubleStep) * doubleStep;
      }
      if (firstStopDepth >= ctx.currentDepth) firstStopDepth = ctx.currentDepth - stepSize;
      if (firstStopDepth < targetDepth + stepSize) firstStopDepth = targetDepth + stepSize;
      ctx.state.firstStopDepth = firstStopDepth;
      if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
        boyleLawCompensation(ctx.state, firstStopDepth, firstStopDepth, stepSize, settings);
        applyExtendedAfterBoyle(ctx.state, model, settings);
        rawCeiling = getVPMCeiling(ctx.state, settings);
        firstStopDepth = Math.floor(rawCeiling / stepSize) * stepSize;
        if (firstStopDoubleStep && firstStopDepth > Math.max(lastStop, targetDepth)) {
          var _doubleStep = stepSize * 2;
          firstStopDepth = Math.ceil(rawCeiling / _doubleStep) * _doubleStep;
        }
        if (firstStopDepth >= ctx.currentDepth) firstStopDepth = ctx.currentDepth - stepSize;
        if (firstStopDepth < targetDepth + stepSize) firstStopDepth = targetDepth + stepSize;
        ctx.state.firstStopDepth = firstStopDepth;
      }
      return firstStopDepth;
    }
    function runStopSequenceToDepth(ctx, firstStopDepth, targetDepth, anchorFirstStopDepth) {
      if (firstStopDepth <= 0 || firstStopDepth <= targetDepth) {
        var mdpFirst = vpmMdpSurfaceFirstStopDepth(ctx.currentDepth);
        if (mdpFirst > targetDepth) {
          ctx.continuationFinalPhase = true;
          return runStopSequenceToDepth(ctx, mdpFirst, targetDepth, mdpFirst);
        }
        runAscentSegment(ctx, targetDepth, ascentRate, 'deco');
        return ctx;
      }
      var phaseFirstStopDepth = anchorFirstStopDepth || firstStopDepth;
      ctx.firstStopDepth = phaseFirstStopDepth;
      ctx.currentSP = vpmSetpointAtDepth(firstStopDepth, 'deco', ctx.forcedOCMode, settings);
      runAscentSegment(ctx, firstStopDepth, ascentRate, 'deco');
      var stopDepth = firstStopDepth;
      while (stopDepth > targetDepth) {
        maybeSwitchDecoGas(ctx, stopDepth);
        var nextStop = vpmNextMdpStopDepth(stopDepth, targetDepth, stepSize, false);
        if (model === 'VPMB_GFS') {
          var gfsValue = settings.gfs || settings.gfHi || 85;
          applyGFSurfacing(ctx.state, stopDepth, phaseFirstStopDepth, gfsValue, settings);
        }
        if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
          boyleLawCompensation(ctx.state, phaseFirstStopDepth, stopDepth + stepSize, stepSize, settings);
          applyExtendedAfterBoyle(ctx.state, model, settings);
        }
        runRoundedDecoStop(ctx, stopDepth, nextStop);
        if (vpmStopCapFailedDepth != null) return ctx;
        if (nextStop <= targetDepth) {
          var segTime = loadTissuesLinear(ctx.state, stopDepth, targetDepth, decoAscentRate, ctx.currentO2, ctx.currentHe, settings, ctx.currentSP);
          ctx.runtime += segTime;
          addExposureToContext(ctx, stopDepth, targetDepth, segTime);
          ctx.currentDepth = targetDepth;
          break;
        }
        runAscentSegment(ctx, nextStop, decoAscentRate);
        stopDepth = nextStop;
      }
      return ctx;
    }
    function appendLevelHold(ctx, level) {
      if (level.oc) ctx.forcedOCMode = true;
      var nextLevelOffLoop = isCCR && !!(level.oc || level.scr);
      ctx.currentSP = ctx.forcedOCMode || nextLevelOffLoop ? 0 : getEffectiveSetpoint(level, isCCR, settings, level.depth, 'bottom');
      if (level.oc) maybeSwitchDecoGas(ctx, ctx.currentDepth);
      ctx.currentDepth = level.depth;
      ctx.currentO2 = level.o2 / 100;
      ctx.currentHe = level.he / 100;
      ctx.currentGasLabel = vpmGasLabel(level.o2, level.he);
      if (level.time <= 0) return;
      settings._scrRuntimeMin = ctx.runtime;
      loadTissuesConstant(ctx.state, level.depth, level.time, ctx.currentO2, ctx.currentHe, settings, ctx.currentSP);
      ctx.runtime += level.time;
      addConstantExposure(ctx, level.depth, level.time);
      appendPlan(ctx, {
        type: 'bottom',
        depth: level.depth,
        time: Math.round(level.time * 10) / 10,
        runtime: Math.round(ctx.runtime * 10) / 10,
        gas: ctx.currentGasLabel,
        o2: level.o2,
        he: level.he
      });
    }
    function calcSurfaceFirstStopDepth(scheduleState, fromDepth, o2, he, sp, anchorFirstStopDepth) {
      var rawCeiling = getVPMCeiling(scheduleState, settings);
      if (rawCeiling <= 0) return 0;
      var firstStopDepth = roundUpToStop(rawCeiling, stepSize);
      if (firstStopDoubleStep && firstStopDepth > lastStop) {
        var doubleStep = stepSize * 2;
        firstStopDepth = Math.ceil(rawCeiling / doubleStep) * doubleStep;
      }
      if (firstStopDepth >= fromDepth) firstStopDepth = fromDepth - stepSize;
      if (firstStopDepth < lastStop) firstStopDepth = lastStop;
      firstStopDepth = projectedAscent(scheduleState, Math.max(fromDepth, 0), ascentRate, firstStopDepth, stepSize, o2, he, settings, sp);
      if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
        boyleLawCompensation(scheduleState, anchorFirstStopDepth || firstStopDepth, firstStopDepth, stepSize, settings);
        applyExtendedAfterBoyle(scheduleState, model, settings);
        rawCeiling = getVPMCeiling(scheduleState, settings);
        if (rawCeiling > 0) {
          firstStopDepth = roundUpToStop(rawCeiling, stepSize);
          if (firstStopDoubleStep && firstStopDepth > lastStop) {
            var _doubleStep2 = stepSize * 2;
            firstStopDepth = Math.ceil(rawCeiling / _doubleStep2) * _doubleStep2;
          }
          if (firstStopDepth >= fromDepth) firstStopDepth = fromDepth - stepSize;
          if (firstStopDepth < lastStop) firstStopDepth = lastStop;
          firstStopDepth = projectedAscent(scheduleState, Math.max(fromDepth, 0), ascentRate, firstStopDepth, stepSize, o2, he, settings, sp);
        } else {
          firstStopDepth = 0;
        }
      }
      return firstStopDepth;
    }
    function runContinuationSchedule(baseState, startDepth, startRuntime, startOTU, startCNS, o2, he, gasLabel, sp, outPlan, forcedOCAtStart) {
      var ctx = makeScheduleContext(baseState, startDepth, startRuntime, startOTU, startCNS, o2, he, gasLabel, sp, outPlan, forcedOCAtStart);
      var phaseFirstStopDepth = 0;
      var _iterator3 = _createForOfIteratorHelper(continuationLevels),
        _step3;
      try {
        for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
          var level = _step3.value;
          var levelFirstStopDepth = calcIntermediateFirstStopDepth(ctx, level.depth);
          if (!phaseFirstStopDepth && levelFirstStopDepth > 0) {
            phaseFirstStopDepth = levelFirstStopDepth;
          }
          runStopSequenceToDepth(ctx, levelFirstStopDepth, level.depth, phaseFirstStopDepth || levelFirstStopDepth);
          appendLevelHold(ctx, level);
        }
      } catch (err) {
        _iterator3.e(err);
      } finally {
        _iterator3.f();
      }
      var finalFirstStopDepth = calcSurfaceFirstStopDepth(ctx.state, ctx.currentDepth, ctx.currentO2, ctx.currentHe, ctx.currentSP, phaseFirstStopDepth);
      if (!phaseFirstStopDepth && finalFirstStopDepth > 0) {
        phaseFirstStopDepth = finalFirstStopDepth;
      }
      ctx.continuationFinalPhase = true;
      runStopSequence(ctx, finalFirstStopDepth, true, phaseFirstStopDepth || finalFirstStopDepth);
      return {
        ctx: ctx,
        firstStopDepth: phaseFirstStopDepth
      };
    }
    function runStopSequence(ctx, firstStopDepth, recordSurface, anchorFirstStopDepth) {
      if (firstStopDepth <= 0) {
        var mdpFirst = vpmMdpSurfaceFirstStopDepth(ctx.currentDepth);
        if (mdpFirst > 0) {
          ctx.continuationFinalPhase = true;
          return runStopSequence(ctx, mdpFirst, recordSurface, mdpFirst);
        }
        runAscentSegment(ctx, 0, surfaceAscentRate, 'deco');
        if (recordSurface) {
          appendPlan(ctx, {
            type: 'surface',
            depth: 0,
            time: 0,
            runtime: Math.round(ctx.runtime * 10) / 10,
            gas: ctx.currentGasLabel
          });
        }
        return ctx;
      }
      var phaseFirstStopDepth = anchorFirstStopDepth || firstStopDepth;
      ctx.firstStopDepth = phaseFirstStopDepth;
      ctx.currentSP = vpmSetpointAtDepth(firstStopDepth, 'deco', ctx.forcedOCMode, settings);
      runAscentSegment(ctx, firstStopDepth, ascentRate, 'deco');
      var stopDepth = firstStopDepth;
      while (stopDepth > 0) {
        maybeSwitchDecoGas(ctx, stopDepth);
        var nextStop = vpmNextMdpStopDepth(stopDepth, 0, stepSize, stopDepth <= lastStop);
        if (model === 'VPMB_GFS') {
          var gfsValue = settings.gfs || settings.gfHi || 85;
          applyGFSurfacing(ctx.state, stopDepth, phaseFirstStopDepth, gfsValue, settings);
        }
        if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
          boyleLawCompensation(ctx.state, phaseFirstStopDepth, stopDepth + stepSize, stepSize, settings);
          applyExtendedAfterBoyle(ctx.state, model, settings);
        }
        runRoundedDecoStop(ctx, stopDepth, nextStop);
        if (vpmStopCapFailedDepth != null) return ctx;
        if (nextStop <= 0) {
          runAscentSegment(ctx, 0, surfaceAscentRate, 'deco');
          break;
        }
        runAscentSegment(ctx, nextStop, decoAscentRate, 'deco');
        stopDepth = nextStop;
      }
      if (recordSurface) {
        appendPlan(ctx, {
          type: 'surface',
          depth: 0,
          time: 0,
          runtime: Math.round(ctx.runtime * 10) / 10,
          gas: ctx.currentGasLabel
        });
      }
      return ctx;
    }
    var isCCR = isRebreatherCircuit(settings.circuit) && !settings.bailout;
    var curO2 = levels[0].o2 / 100;
    var curHe = levels[0].he / 100;
    var curGasLabel = vpmGasLabel(levels[0].o2, levels[0].he);
    var forcedOCMode = isCCR && !!levels[0].oc;
    var curSP = forcedOCMode ? 0 : getEffectiveSetpoint(levels[0], isCCR, settings, levels[0].depth, 'descent');
    function runInterLevelDecoAscent(targetDepth) {
      var stateBeforeInterLevel = cloneVPMState(state);
      function restoreInterLevelDerivedState() {
        var snap = stateBeforeInterLevel;
        var arrayKeys = ['regeneratedRadiiN2', 'regeneratedRadiiHe', 'adjustedCritRadiiN2', 'adjustedCritRadiiHe', 'maxCrushingPressureN2', 'maxCrushingPressureHe', 'adjustedCrushingPressureN2', 'adjustedCrushingPressureHe', 'allowableGradientN2', 'allowableGradientHe', 'decoGradientN2', 'decoGradientHe', 'initialAllowableGradientN2', 'initialAllowableGradientHe', 'maxActualGradientN2', 'maxActualGradientHe', 'critRadiiN2', 'critRadiiHe'];
        for (var _i9 = 0, _arrayKeys = arrayKeys; _i9 < _arrayKeys.length; _i9++) {
          var key = _arrayKeys[_i9];
          if (snap[key] && state[key]) {
            for (var _i0 = 0; _i0 < NC; _i0++) state[key][_i0] = snap[key][_i0];
          }
        }
        state.firstStopDepth = snap.firstStopDepth;
        state.useDecoGradients = snap.useDecoGradients;
        state._conservatismRadiiApplied = snap._conservatismRadiiApplied;
        state.maxAmbientPressure = snap.maxAmbientPressure;
      }
      calcCrushing(state, settings);
      var offLoopPath = isCCR && (forcedOCMode || curSP <= 0);
      var interLevelConservatism = offLoopPath ? Math.max(0, conservatism - 1) : conservatism;
      if (offLoopPath && interLevelConservatism < conservatism) {
        var consIdx = Math.max(0, Math.min(5, Math.round(conservatism || 0)));
        var interIdx = Math.max(0, Math.min(5, Math.round(interLevelConservatism || 0)));
        var relax = VPM_CRITICAL_RADIUS_FACTOR[interIdx] / VPM_CRITICAL_RADIUS_FACTOR[consIdx];
        if (relax < 0.999) {
          for (var _i1 = 0; _i1 < NC; _i1++) {
            state.regeneratedRadiiN2[_i1] *= relax;
            state.regeneratedRadiiHe[_i1] *= relax;
            state.adjustedCritRadiiN2[_i1] *= relax;
            state.adjustedCritRadiiHe[_i1] *= relax;
          }
        }
      }
      calcAllowableGradients(state, model, settings, interLevelConservatism);
      var rawCeiling = getVPMCeiling(state, settings);
      if (rawCeiling <= targetDepth) {
        settings._scrRuntimeMin = runtime;
        var ascTime = loadTissuesLinear(state, currentDepth, targetDepth, ascentRate, curO2, curHe, settings, curSP);
        runtime += ascTime;
        var stepsA = Math.max(1, Math.ceil(Math.abs(currentDepth - targetDepth)));
        var dtA = ascTime / stepsA;
        for (var _s2 = 0; _s2 < stepsA; _s2++) {
          var frac = _s2 / stepsA;
          var depth = currentDepth + (targetDepth - currentDepth) * frac;
          var pAmbSeg = getAmbientPressure(depth, settings);
          var ppO2Seg = vpmAccumPpo2(pAmbSeg, curSP, curO2, curHe, settings, depth, forcedOCMode);
          totalOTU += calculateOTU(ppO2Seg, dtA);
          totalCNS += calculateCNS(ppO2Seg, dtA);
        }
        plan.push({
          type: 'ascent',
          startDepth: currentDepth,
          endDepth: targetDepth,
          time: Math.round(ascTime * 10) / 10,
          runtime: Math.round(runtime * 10) / 10,
          gas: curGasLabel,
          o2: Math.round(curO2 * 100),
          he: Math.round(curHe * 100),
          setpoint: curSP > 0 ? curSP : 0
        });
        restoreInterLevelDerivedState();
        return {
          depth: targetDepth,
          o2: curO2,
          he: curHe,
          gasLabel: curGasLabel,
          sp: curSP
        };
      }
      var firstStopDepth = offLoopPath ? Math.floor(rawCeiling / stepSize) * stepSize : roundUpToStop(rawCeiling, stepSize);
      if (firstStopDoubleStep && firstStopDepth > Math.max(lastStop, targetDepth)) {
        var doubleStep = stepSize * 2;
        firstStopDepth = Math.ceil(rawCeiling / doubleStep) * doubleStep;
      }
      if (firstStopDepth >= currentDepth) firstStopDepth = currentDepth - stepSize;
      if (firstStopDepth < targetDepth + stepSize) firstStopDepth = targetDepth + stepSize;
      state.firstStopDepth = firstStopDepth;
      if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
        boyleLawCompensation(state, firstStopDepth, firstStopDepth, stepSize, settings);
        applyExtendedAfterBoyle(state, model, settings);
        rawCeiling = getVPMCeiling(state, settings);
        firstStopDepth = offLoopPath ? Math.floor(rawCeiling / stepSize) * stepSize : roundUpToStop(rawCeiling, stepSize);
        if (firstStopDoubleStep && firstStopDepth > Math.max(lastStop, targetDepth)) {
          var _doubleStep3 = stepSize * 2;
          firstStopDepth = Math.ceil(rawCeiling / _doubleStep3) * _doubleStep3;
        }
        if (firstStopDepth >= currentDepth) firstStopDepth = currentDepth - stepSize;
        if (firstStopDepth < targetDepth + stepSize) firstStopDepth = targetDepth + stepSize;
        state.firstStopDepth = firstStopDepth;
      }
      settings._scrRuntimeMin = runtime;
      var ascTimeToFirst = loadTissuesLinear(state, currentDepth, firstStopDepth, ascentRate, curO2, curHe, settings, curSP);
      runtime += ascTimeToFirst;
      {
        var _stepsA = Math.max(1, Math.ceil(Math.abs(currentDepth - firstStopDepth)));
        var _dtA = ascTimeToFirst / _stepsA;
        for (var _s3 = 0; _s3 < _stepsA; _s3++) {
          var _frac = _s3 / _stepsA;
          var depthSeg = currentDepth + (firstStopDepth - currentDepth) * _frac;
          var _pAmbSeg = getAmbientPressure(depthSeg, settings);
          var _ppO2Seg = vpmAccumPpo2(_pAmbSeg, curSP, curO2, curHe, settings, depthSeg, forcedOCMode);
          totalOTU += calculateOTU(_ppO2Seg, _dtA);
          totalCNS += calculateCNS(_ppO2Seg, _dtA);
        }
      }
      plan.push({
        type: 'ascent',
        startDepth: currentDepth,
        endDepth: firstStopDepth,
        time: Math.round(ascTimeToFirst * 10) / 10,
        runtime: Math.round(runtime * 10) / 10,
        gas: curGasLabel,
        o2: Math.round(curO2 * 100),
        he: Math.round(curHe * 100),
        setpoint: curSP > 0 ? curSP : 0
      });
      var stopDepth = firstStopDepth;
      var maxIter = 500;
      while (stopDepth > targetDepth && maxIter-- > 0) {
        if (!isCCR) {
          var decoGas = selectDecoGas(stopDepth, normalizedDecoGases, ppO2Deco, settings);
          if (decoGas) {
            var pAmbHere = getAmbientPressure(stopDepth, settings);
            var ocPpO2 = decoGas.o2 * pAmbHere;
            var ccrPpO2 = curSP > 0 ? Math.min(curSP, pAmbHere) : 0;
            if (decoGas.o2 > curO2 || curSP > 0 && ocPpO2 > ccrPpO2) {
              curO2 = decoGas.o2;
              curHe = decoGas.he;
              curGasLabel = decoGas.label;
              curSP = 0;
            }
          }
        }
        var nextStop = vpmNextMdpStopDepth(stopDepth, targetDepth, stepSize, false);
        var nextStopClamped = nextStop;
        if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
          boyleLawCompensation(state, firstStopDepth, stopDepth + stepSize, stepSize, settings);
          applyExtendedAfterBoyle(state, model, settings);
        }
        var effectiveMinStop = firstStop30sec && stopDepth === firstStopDepth ? 0.5 : minStopTime;
        var stopTime = 0;
        while (!isClearToAscendVPM(state, nextStopClamped, firstStopDepth, model, settings) && stopTime < vpmMaxStopMin) {
          settings._scrRuntimeMin = runtime;
          loadTissuesConstant(state, stopDepth, effectiveMinStop, curO2, curHe, settings, curSP);
          stopTime += effectiveMinStop;
        }
        if (settings._vpmTestForceStopCap || !isClearToAscendVPM(state, nextStopClamped, firstStopDepth, model, settings)) {
          vpmStopCapFailedDepth = stopDepth;
          restoreInterLevelDerivedState();
          return null;
        }
        var mdpMin = vpmMdpMinAtDepth(stopDepth);
        if (mdpMin > stopTime) {
          var mdpExtra = mdpMin - stopTime;
          settings._scrRuntimeMin = runtime;
          loadTissuesConstant(state, stopDepth, mdpExtra, curO2, curHe, settings, curSP);
          stopTime = mdpMin;
        }
        if (stopTime > 0) {
          if (stopTime < effectiveMinStop) stopTime = effectiveMinStop;
          runtime += stopTime;
          var pAmbStop = getAmbientPressure(stopDepth, settings);
          var ppO2Stop = vpmAccumPpo2(pAmbStop, curSP, curO2, curHe, settings, stopDepth, forcedOCMode);
          totalOTU += calculateOTU(ppO2Stop, stopTime);
          totalCNS += calculateCNS(ppO2Stop, stopTime);
          plan.push({
            type: 'stop',
            depth: stopDepth,
            time: stopTime,
            runtime: Math.round(runtime * 10) / 10,
            gas: curGasLabel,
            o2: Math.round(curO2 * 100),
            he: Math.round(curHe * 100),
            setpoint: curSP > 0 ? curSP : 0
          });
        }
        if (nextStopClamped < stopDepth) {
          settings._scrRuntimeMin = runtime;
          var ascSegTime = loadTissuesLinear(state, stopDepth, nextStopClamped, decoAscentRate, curO2, curHe, settings, curSP);
          runtime += ascSegTime;
          var _stepsA2 = Math.max(1, Math.ceil(Math.abs(stopDepth - nextStopClamped)));
          var _dtA2 = ascSegTime / _stepsA2;
          for (var _s4 = 0; _s4 < _stepsA2; _s4++) {
            var _frac2 = _s4 / _stepsA2;
            var _depthSeg = stopDepth + (nextStopClamped - stopDepth) * _frac2;
            var _pAmbSeg2 = getAmbientPressure(_depthSeg, settings);
            var _ppO2Seg2 = vpmAccumPpo2(_pAmbSeg2, curSP, curO2, curHe, settings, _depthSeg, forcedOCMode);
            totalOTU += calculateOTU(_ppO2Seg2, _dtA2);
            totalCNS += calculateCNS(_ppO2Seg2, _dtA2);
          }
        }
        stopDepth = nextStopClamped;
      }
      currentDepth = targetDepth;
      restoreInterLevelDerivedState();
      return {
        depth: targetDepth,
        o2: curO2,
        he: curHe,
        gasLabel: curGasLabel,
        sp: curSP
      };
    }
    var _iterator4 = _createForOfIteratorHelper(levels),
      _step4;
    try {
      for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
        var level = _step4.value;
        var depth = level.depth;
        var time = level.time;
        var o2Frac = level.o2 / 100;
        var heFrac = level.he / 100;
        if (level.oc) forcedOCMode = true;
        var nextLevelOffLoop = isCCR && !!(level.oc || level.scr);
        var sp = forcedOCMode || nextLevelOffLoop ? 0 : getEffectiveSetpoint(level, isCCR, settings, depth, depth > currentDepth ? 'descent' : 'bottom');
        var bottomSp = forcedOCMode || nextLevelOffLoop ? 0 : getEffectiveSetpoint(level, isCCR, settings, depth, 'bottom');
        if (level.oc) {
          var decoGas = selectDecoGas(currentDepth, normalizedDecoGases, ppO2Deco, settings);
          if (decoGas) {
            curO2 = decoGas.o2;
            curHe = decoGas.he;
            curGasLabel = decoGas.label;
            curSP = 0;
          }
        }
        if (depth > currentDepth) {
          settings._scrRuntimeMin = runtime;
          var descTime = loadTissuesLinear(state, currentDepth, depth, descentRate, o2Frac, heFrac, settings, sp);
          runtime += descTime;
          bottomPhaseRuntime += descTime;
          var pAmbStart = getAmbientPressure(currentDepth, settings);
          var pAmbEnd = getAmbientPressure(depth, settings);
          var ppO2Start = vpmAccumPpo2(pAmbStart, sp, o2Frac, heFrac, settings, currentDepth, forcedOCMode || nextLevelOffLoop);
          var ppO2End = vpmAccumPpo2(pAmbEnd, sp, o2Frac, heFrac, settings, depth, forcedOCMode || nextLevelOffLoop);
          var ppO2Avg = (ppO2Start + ppO2End) / 2;
          totalOTU += calculateOTU(ppO2Avg, descTime);
          totalCNS += calculateCNS(ppO2Avg, descTime);
          plan.push({
            type: 'descent',
            startDepth: currentDepth,
            endDepth: depth,
            time: Math.round(descTime * 10) / 10,
            runtime: Math.round(runtime * 10) / 10,
            gas: vpmGasLabel(level.o2, level.he),
            o2: level.o2,
            he: level.he,
            setpoint: sp > 0 ? sp : 0
          });
          curO2 = o2Frac;
          curHe = heFrac;
          curGasLabel = vpmGasLabel(level.o2, level.he);
          curSP = sp;
        } else if (depth < currentDepth) {
          if (runInterLevelDecoAscent(depth) === null) {
            return vpmStopCapError(vpmStopCapFailedDepth != null ? vpmStopCapFailedDepth : depth, plan);
          }
          curO2 = o2Frac;
          curHe = heFrac;
          curGasLabel = vpmGasLabel(level.o2, level.he);
          curSP = sp;
        }
        var travelRate = depth < currentDepth ? ascentRate : descentRate;
        var descTimeFromLevel = Math.abs(depth - currentDepth) / travelRate;
        var bottomTime = Math.max(0, time - descTimeFromLevel);
        if (bottomTime > 0) {
          settings._scrRuntimeMin = runtime;
          loadTissuesConstant(state, depth, bottomTime, o2Frac, heFrac, settings, bottomSp);
          runtime += bottomTime;
          bottomPhaseRuntime += bottomTime;
          var pAmbB = getAmbientPressure(depth, settings);
          var ppO2B = vpmAccumPpo2(pAmbB, bottomSp, o2Frac, heFrac, settings, depth, forcedOCMode || nextLevelOffLoop);
          totalOTU += calculateOTU(ppO2B, bottomTime);
          totalCNS += calculateCNS(ppO2B, bottomTime);
          plan.push({
            type: 'bottom',
            depth: depth,
            time: Math.round(bottomTime * 10) / 10,
            runtime: Math.round(runtime * 10) / 10,
            gas: vpmGasLabel(level.o2, level.he),
            o2: level.o2,
            he: level.he,
            setpoint: bottomSp > 0 ? bottomSp : 0
          });
          curSP = bottomSp;
        }
        currentDepth = depth;
      }
    } catch (err) {
      _iterator4.e(err);
    } finally {
      _iterator4.f();
    }
    setCriticalRadiiForConservatism(state, conservatism, settings);
    calcCrushing(state, settings);
    applyNuclearRegeneration(state, bottomPhaseRuntime);
    calcAllowableGradients(state, model, settings, conservatism);
    var lastLevel = continuationLevels.length > 0 ? continuationLevels[continuationLevels.length - 1] : levels[levels.length - 1];
    var currentO2 = lastLevel.o2 / 100;
    var currentHe = lastLevel.he / 100;
    var currentGasLabel = vpmGasLabel(lastLevel.o2, lastLevel.he);
    var currentSP = forcedOCMode ? 0 : getEffectiveSetpoint(lastLevel, isCCR, settings, lastLevel.depth, 'bottom');
    var startOfAscentState = cloneVPMState(state);
    var runtimeStartOfAscent = runtime;
    var totalOTUStartOfAscent = totalOTU;
    var totalCNSStartOfAscent = totalCNS;
    var depthStartOfAscent = currentDepth;
    settings._scrRuntimeMin = runtimeStartOfAscent;
    var depthStartOfDecoZone = calcStartOfDecoZone(startOfAscentState, depthStartOfAscent, ascentRate, currentO2, currentHe, settings, currentSP);
    var trialStartState = cloneVPMState(startOfAscentState);
    var runtimeStartOfDecoZone = runtimeStartOfAscent;
    if (depthStartOfDecoZone > 0 && depthStartOfDecoZone < depthStartOfAscent) {
      settings._scrRuntimeMin = runtimeStartOfAscent;
      runtimeStartOfDecoZone += loadTissuesLinear(trialStartState, depthStartOfAscent, depthStartOfDecoZone, ascentRate, currentO2, currentHe, settings, currentSP);
    }
    var decoPhaseRuntimeOrigin = depthStartOfDecoZone > 0 ? runtimeStartOfDecoZone : runtimeStartOfAscent;
    var decoZoneStart = depthStartOfDecoZone;
    var firstStopDepth = 0;
    var scheduleConverged = false;
    for (var cvIter = 0; cvIter < 12; cvIter++) {
      var trialBaseState = cloneVPMState(state);
      restoreTissues(trialBaseState, trialStartState.tissues);
      if (continuationLevels.length === 0) {
        var rawCeiling = getVPMCeiling(trialBaseState, settings);
        if (rawCeiling <= 0) {
          firstStopDepth = 0;
        } else {
          firstStopDepth = roundUpToStop(rawCeiling, stepSize);
          if (firstStopDoubleStep && firstStopDepth > lastStop) {
            var doubleStep = stepSize * 2;
            firstStopDepth = Math.ceil(rawCeiling / doubleStep) * doubleStep;
          }
          if (firstStopDepth > depthStartOfDecoZone && depthStartOfDecoZone > 0) {
            firstStopDepth = Math.floor(depthStartOfDecoZone / stepSize) * stepSize;
          }
          if (firstStopDepth >= depthStartOfAscent) firstStopDepth = depthStartOfAscent - stepSize;
          if (firstStopDepth < lastStop) firstStopDepth = rawCeiling > 0 ? lastStop : 0;
          firstStopDepth = projectedAscent(trialBaseState, Math.max(depthStartOfDecoZone, 0), ascentRate, firstStopDepth, stepSize, currentO2, currentHe, settings, currentSP);
        }
        if (model === 'VPMB' || model === 'VPMBE' || model === 'VPMB_GFS' || model === 'VPMBFBO') {
          boyleLawCompensation(trialBaseState, firstStopDepth, firstStopDepth, stepSize, settings);
          applyExtendedAfterBoyle(trialBaseState, model, settings);
          rawCeiling = getVPMCeiling(trialBaseState, settings);
          if (rawCeiling > 0) {
            firstStopDepth = roundUpToStop(rawCeiling, stepSize);
            if (firstStopDoubleStep && firstStopDepth > lastStop) {
              var _doubleStep4 = stepSize * 2;
              firstStopDepth = Math.ceil(rawCeiling / _doubleStep4) * _doubleStep4;
            }
            if (firstStopDepth > depthStartOfDecoZone && depthStartOfDecoZone > 0) {
              firstStopDepth = Math.floor(depthStartOfDecoZone / stepSize) * stepSize;
            }
            if (firstStopDepth >= depthStartOfAscent) firstStopDepth = depthStartOfAscent - stepSize;
            if (firstStopDepth < lastStop) firstStopDepth = lastStop;
            firstStopDepth = projectedAscent(trialBaseState, Math.max(depthStartOfDecoZone, 0), ascentRate, firstStopDepth, stepSize, currentO2, currentHe, settings, currentSP);
          } else {
            firstStopDepth = 0;
          }
        }
      } else {
        firstStopDepth = 0;
      }
      var trialCtx = void 0;
      if (continuationLevels.length > 0) {
        trialCtx = runContinuationSchedule(trialBaseState, depthStartOfAscent, runtimeStartOfAscent, totalOTUStartOfAscent, totalCNSStartOfAscent, levels[levels.length - 1].o2 / 100, levels[levels.length - 1].he / 100, vpmGasLabel(levels[levels.length - 1].o2, levels[levels.length - 1].he), forcedOCMode ? 0 : getEffectiveSetpoint(levels[levels.length - 1], isCCR, settings, depthStartOfAscent, 'deco'), null, forcedOCMode).ctx;
        if (vpmStopCapFailedDepth != null) return vpmStopCapError(vpmStopCapFailedDepth, plan);
      } else {
        trialCtx = makeScheduleContext(trialBaseState, depthStartOfDecoZone > 0 ? depthStartOfDecoZone : depthStartOfAscent, depthStartOfDecoZone > 0 ? runtimeStartOfDecoZone : runtimeStartOfAscent, totalOTUStartOfAscent, totalCNSStartOfAscent, currentO2, currentHe, currentGasLabel, currentSP, null);
        runStopSequence(trialCtx, firstStopDepth, false);
        if (vpmStopCapFailedDepth != null) return vpmStopCapError(vpmStopCapFailedDepth, plan);
      }
      var decoPhaseVolumeTime = trialCtx.runtime - decoPhaseRuntimeOrigin;
      calcSurfacePhaseVolumeTime(trialCtx.state, settings);
      scheduleConverged = true;
      for (var _i10 = 0; _i10 < NC; _i10++) {
        var phaseVolumeTime = decoPhaseVolumeTime + trialCtx.state.surfacePhaseVolumeTime[_i10];
        if (Math.abs(phaseVolumeTime - state.lastPhaseVolumeTime[_i10]) > 1.0) {
          scheduleConverged = false;
        }
        state.lastPhaseVolumeTime[_i10] = phaseVolumeTime;
        state.surfacePhaseVolumeTime[_i10] = trialCtx.state.surfacePhaseVolumeTime[_i10];
      }
      if (scheduleConverged || cvIter === 11) {
        var finalState = cloneVPMState(state);
        restoreTissues(finalState, startOfAscentState.tissues);
        var finalCtx = void 0;
        if (continuationLevels.length > 0) {
          finalCtx = runContinuationSchedule(finalState, depthStartOfAscent, runtimeStartOfAscent, totalOTUStartOfAscent, totalCNSStartOfAscent, levels[levels.length - 1].o2 / 100, levels[levels.length - 1].he / 100, vpmGasLabel(levels[levels.length - 1].o2, levels[levels.length - 1].he), forcedOCMode ? 0 : getEffectiveSetpoint(levels[levels.length - 1], isCCR, settings, depthStartOfAscent, 'deco'), plan, forcedOCMode).ctx;
        } else {
          finalCtx = makeScheduleContext(finalState, depthStartOfAscent, runtimeStartOfAscent, totalOTUStartOfAscent, totalCNSStartOfAscent, currentO2, currentHe, currentGasLabel, currentSP, plan);
          runStopSequence(finalCtx, firstStopDepth, true);
          if (vpmStopCapFailedDepth != null) return vpmStopCapError(vpmStopCapFailedDepth, plan);
        }
        runtime = finalCtx.runtime;
        totalOTU = finalCtx.totalOTU;
        totalCNS = finalCtx.totalCNS;
        restoreTissues(state, finalCtx.state.tissues);
        if (vpmStopCapFailedDepth != null) return vpmStopCapError(vpmStopCapFailedDepth, plan);
        return buildResult(plan, runtime, totalOTU, totalCNS, settings, state, decoZoneStart);
      }
      calcCriticalVolume(state, decoPhaseVolumeTime);
    }
    if (vpmStopCapFailedDepth != null) return vpmStopCapError(vpmStopCapFailedDepth, plan);
    return buildResult(plan, runtime, totalOTU, totalCNS, settings, state, decoZoneStart);
  }
  function buildResult(plan, runtime, totalOTU, totalCNS, settings, state, decoZoneStart) {
    // Expose end-of-dive bubble state so the app can pass it to a repetitive dive
    var finalBubbleState = state ? {
      adjustedCritRadiiN2: state.adjustedCritRadiiN2.slice(),
      adjustedCritRadiiHe: state.adjustedCritRadiiHe.slice(),
      regeneratedRadiiN2: state.regeneratedRadiiN2.slice(),
      regeneratedRadiiHe: state.regeneratedRadiiHe.slice(),
      _altFactor: state._altFactor != null ? state._altFactor : 1
    } : null;
    var normPlan = plan.map(function (seg) {
      return _objectSpread(_objectSpread({}, seg), {}, {
        time: seg.time != null ? seg.time : undefined,
        run: seg.run != null ? seg.run : seg.runtime
      });
    });
    var botSeg = normPlan.find(function (s) {
      return s.type === 'bottom';
    }) || normPlan[0];
    return {
      plan: normPlan,
      totalRuntime: Math.ceil(runtime),
      totalOTU: totalOTU,
      totalCNS: totalCNS,
      depthUnit: settings.metric ? 'm' : 'ft',
      decoZoneStart: decoZoneStart || 0,
      stops: plan.filter(function (s) {
        return s.type === 'stop';
      }),
      finalTissues: state ? state.tissues.map(function (t) {
        return {
          pN2: t.pN2,
          pHe: t.pHe
        };
      }) : null,
      finalBubbleState: finalBubbleState,
      // Feature B: carries per-compartment bubble state
      error: null
    };
  }
  return {
    calculate: calculate,
    load: function load() {
      return true;
    },
    createVPMState: createVPMState,
    /** Sync all 16 He compartment half-times from ZHL16C_HE_HT array (Baker / Bühlmann 2003). */
    _syncHeHalfTimes: function _syncHeHalfTimes(htArray) {
      for (var i = 0; i < 16 && htArray && i < htArray.length; i++) {
        if (ZHL16C_He[i]) {
          var v = htArray[i];
          ZHL16C_He[i].ht = v != null && _typeof(v) === 'object' ? v.ht : v;
        }
      }
    },
    /** Compartment-1-only shortcut — kept for callers that pass a single ht value. */
    _setHeHT1: function _setHeHT1(htRow) {
      if (ZHL16C_He[0]) ZHL16C_He[0].ht = htRow != null && _typeof(htRow) === 'object' ? htRow.ht : htRow;
    },
    MODELS: ['VPMA', 'VPMB', 'VPMBE', 'VPMB_GFS', 'VPMBFBO'],
    MODEL_NAMES: {
      'VPMA': 'VPM-A',
      'VPMB': 'VPM-B',
      'VPMBE': 'VPM-B/E',
      'VPMB_GFS': 'VPM-B+GFS',
      'VPMBFBO': 'VPM-B/FBO'
    }
  };
}();
if (typeof module !== 'undefined') module.exports = VPMEngine;
if (typeof window !== 'undefined') window.VPMEngine = VPMEngine;

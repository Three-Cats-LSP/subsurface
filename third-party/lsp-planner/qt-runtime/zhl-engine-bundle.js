// Generated from LSP+ 05fef18bade6354cebcc60df01f83e0ac4cc3b27. See SOURCE.json.
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function _toConsumableArray(r) { return _arrayWithoutHoles(r) || _iterableToArray(r) || _unsupportedIterableToArray(r) || _nonIterableSpread(); }
function _nonIterableSpread() { throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArray(r) { if ("undefined" != typeof Symbol && null != r[Symbol.iterator] || null != r["@@iterator"]) return Array.from(r); }
function _arrayWithoutHoles(r) { if (Array.isArray(r)) return _arrayLikeToArray(r); }
function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), !0).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: !0, configurable: !0, writable: !0 }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == _typeof(i) ? i : i + ""; }
function _toPrimitive(t, e) { if ("object" != _typeof(t) || !t) return t; var r; if ("undefined" != typeof Symbol && void 0 !== (r = t[Symbol.toPrimitive])) { var i = r.call(t, e || "default"); if ("object" != _typeof(i)) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === e ? String : Number)(t); }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
/**
 * ZHL Engine Bundle — Tier 3 isolated Bühlmann module.
 * Loaded on main thread and in zhl-schedule-worker.js (importScripts).
 */
(function (global) {
  'use strict';

  var _neoNativeTrace = null;
  var ZHL16C = [[5.0, 1.2599, 0.5050], [8.0, 1.0000, 0.6514], [12.5, 0.8618, 0.7222], [18.5, 0.7562, 0.7825], [27.0, 0.6200, 0.8126], [38.3, 0.5043, 0.8434], [54.3, 0.4410, 0.8693], [77.0, 0.4000, 0.8910], [109.0, 0.3750, 0.9092], [146.0, 0.3500, 0.9222], [187.0, 0.3295, 0.9319], [239.0, 0.3065, 0.9403], [305.0, 0.2835, 0.9477], [390.0, 0.2610, 0.9544], [498.0, 0.2480, 0.9602], [635.0, 0.2327, 0.9653]];
  var ZHL16C_HE_HT_BAKER = [1.88, 3.02, 4.72, 6.99, 10.21, 14.48, 20.53, 29.11, 41.20, 55.19, 70.69, 90.34, 115.29, 147.42, 188.24, 240.03];
  var ZHL16C_HE_HT_BUHL2003 = [1.51, 3.02, 4.72, 6.99, 10.21, 14.48, 20.53, 29.11, 41.20, 55.19, 70.69, 90.34, 115.29, 147.42, 188.24, 240.03];
  var ZHL16C_HE_HT = ZHL16C_HE_HT_BAKER.slice();
  var ZHL16C_HE_AB = [[1.7424, 0.4245], [1.3830, 0.5747], [1.1919, 0.6527], [1.0458, 0.7223], [0.9220, 0.7582], [0.8205, 0.7957], [0.7305, 0.8279], [0.6502, 0.8553], [0.5950, 0.8757], [0.5545, 0.8903], [0.5333, 0.8997], [0.5189, 0.9073], [0.5181, 0.9122], [0.5176, 0.9171], [0.5172, 0.9217], [0.5119, 0.9267]];
  var OTU_EXPONENT = 0.8333;
  var SEA_LEVEL_P = 1.01325;
  var PSCR_MIN_PPO2 = 0.16;
  var altSurfaceP = SEA_LEVEL_P;
  var BAR_PER_METRE = 0.1;
  var WATER_VAPOR = 0.0627;
  var altAcclimatized = true;
  var allowO2AtMOD = true;
  function applyEnvironment(env) {
    var _env$altSurfaceP, _env$barPerMetre, _env$waterVapor;
    env = env || {};
    altSurfaceP = (_env$altSurfaceP = env.altSurfaceP) !== null && _env$altSurfaceP !== void 0 ? _env$altSurfaceP : SEA_LEVEL_P;
    BAR_PER_METRE = (_env$barPerMetre = env.barPerMetre) !== null && _env$barPerMetre !== void 0 ? _env$barPerMetre : 0.1;
    WATER_VAPOR = (_env$waterVapor = env.waterVapor) !== null && _env$waterVapor !== void 0 ? _env$waterVapor : 0.0627;
    altAcclimatized = env.altAcclimatized !== false;
    allowO2AtMOD = env.allowO2AtMOD !== false;
  }
  function defaultEnvironment() {
    return {
      altSurfaceP: SEA_LEVEL_P,
      barPerMetre: 0.1,
      waterVapor: 0.0627,
      altAcclimatized: true,
      allowO2AtMOD: true
    };
  }
  function setHeHalfTimeMode(mode) {
    var src = mode === 'buhl2003' ? ZHL16C_HE_HT_BUHL2003 : ZHL16C_HE_HT_BAKER;
    for (var i = 0; i < 16; i++) ZHL16C_HE_HT[i] = src[i];
  }
  function depthBar(m) {
    return altSurfaceP + m * BAR_PER_METRE;
  }
  function schreiner(p0, pGas, ht, t) {
    return pGas + (p0 - pGas) * Math.exp(-Math.LN2 / ht * t);
  }
  /** @param {number} R - pressure rate bar/min; may be 0 for constant depth (standard Schreiner). */
  function schreinerLinear(p0, fN2, ht, t, p0Amb, R) {
    var k = Math.LN2 / ht;
    var piN2 = (p0Amb - WATER_VAPOR) * fN2;
    var rN2 = R * fN2;
    return piN2 + rN2 * (t - 1 / k) - (piN2 - p0 - rN2 / k) * Math.exp(-k * t);
  }
  function initTissues() {
    var surfP = altAcclimatized ? altSurfaceP : SEA_LEVEL_P;
    var pN2 = (surfP - WATER_VAPOR) * 0.7902;
    return ZHL16C.map(function () {
      return {
        pN2: pN2,
        pHe: 0
      };
    });
  }
  function saturateLinear(tissues, fromDepth, toDepth, t, fN2, fHe) {
    if (t <= 0) return tissues;
    var p0Amb = depthBar(fromDepth);
    var pEndAmb = depthBar(toDepth);
    var R = (pEndAmb - p0Amb) / t;
    var fH = fHe || 0;
    return tissues.map(function (t0, i) {
      return {
        pN2: schreinerLinear(t0.pN2, fN2, ZHL16C[i][0], t, p0Amb, R),
        pHe: fH > 0 ? schreinerLinear(t0.pHe, fH, ZHL16C_HE_HT[i], t, p0Amb, R) : t0.pHe
      };
    });
  }
  function saturate(tissues, depthM, t, fN2, fHe) {
    var pAmb = depthBar(depthM);
    var pN2insp = (pAmb - WATER_VAPOR) * fN2;
    var pHeinsp = (pAmb - WATER_VAPOR) * (fHe || 0);
    return tissues.map(function (t0, i) {
      return {
        pN2: schreiner(t0.pN2, pN2insp, ZHL16C[i][0], t),
        pHe: schreiner(t0.pHe, pHeinsp, ZHL16C_HE_HT[i], t)
      };
    });
  }
  function ceiling(tissues, gfHigh) {
    if (!(gfHigh > 0)) return 0;
    var maxC = 0;
    tissues.forEach(function (t0, i) {
      var pN2 = t0.pN2;
      var pHe = t0.pHe || 0;
      var pTotal = pN2 + pHe;
      var a, b;
      if (pHe > 0 && pTotal > 0) {
        a = (pN2 * ZHL16C[i][1] + pHe * ZHL16C_HE_AB[i][0]) / pTotal;
        b = (pN2 * ZHL16C[i][2] + pHe * ZHL16C_HE_AB[i][1]) / pTotal;
      } else {
        var _ZHL16C$i = _slicedToArray(ZHL16C[i], 3);
        a = _ZHL16C$i[1];
        b = _ZHL16C$i[2];
      }
      var pAmbMin = (pTotal - gfHigh * a) / (1 - gfHigh + gfHigh / b);
      var cM = Math.max(0, (pAmbMin - altSurfaceP) / BAR_PER_METRE);
      if (cM > maxC) maxC = cM;
    });
    return maxC;
  }
  function computeSurfaceGF(tissues) {
    if (!tissues || !tissues.length) return null;
    var P_surf = altSurfaceP;
    var maxGF = -Infinity;
    tissues.forEach(function (t, i) {
      var pTotal = (t.pN2 || 0) + (t.pHe || 0);
      if (pTotal <= 0) return;
      var a, b;
      var pN2 = t.pN2 || 0,
        pHe = t.pHe || 0;
      if (pHe > 0 && pTotal > 0) {
        a = (pN2 * ZHL16C[i][1] + pHe * ZHL16C_HE_AB[i][0]) / pTotal;
        b = (pN2 * ZHL16C[i][2] + pHe * ZHL16C_HE_AB[i][1]) / pTotal;
      } else {
        var _ZHL16C$i2 = _slicedToArray(ZHL16C[i], 3);
        a = _ZHL16C$i2[1];
        b = _ZHL16C$i2[2];
      }
      var mValue = a + P_surf / b;
      var mMargin = mValue - P_surf;
      if (mMargin <= 0) return;
      var gf = (pTotal - P_surf) / mMargin;
      if (gf > maxGF) maxGF = gf;
    });
    return maxGF === -Infinity ? 0 : Math.max(0, maxGF * 100);
  }
  function ambientCrossingDepth(tissues) {
    var maxD = 0;
    tissues.forEach(function (t0) {
      var pTotal = t0.pN2 + (t0.pHe || 0);
      var d = (pTotal - altSurfaceP) / BAR_PER_METRE;
      if (d > maxD) maxD = d;
    });
    return Math.max(0, maxD);
  }
  function gfAtDepth(depthM, gfL, gfH, firstStopDepth, lastStop, shallowGradient) {
    if (!firstStopDepth || firstStopDepth <= 0) return gfH;
    if (depthM >= firstStopDepth) return gfL;
    if (shallowGradient && depthM <= lastStop) return gfH;
    var interpBase = shallowGradient ? lastStop : 0;
    if (firstStopDepth <= interpBase) return gfH;
    var gf = gfL + (gfH - gfL) * (firstStopDepth - depthM) / (firstStopDepth - interpBase);
    return Math.min(gfH, Math.max(gfL, gf));
  }
  function ndlClearAtDepth(tissues, depthM, gfL, gfH, lastStop, decoStep, shallowGradient) {
    if (!(decoStep > 0)) decoStep = 3;
    if (!(lastStop >= 0)) lastStop = 3;
    var ceilL = ceiling(tissues, gfL);
    if (ceilL <= 0) return true;
    var firstStop = Math.max(lastStop, Math.ceil(ceilL / decoStep) * decoStep);
    var depths = [depthM];
    for (var d = firstStop; d >= 0; d -= decoStep) {
      if (d < depthM - 1e-6) depths.push(d);
    }
    if (depths[depths.length - 1] !== 0) depths.push(0);
    for (var _i = 0, _depths = depths; _i < _depths.length; _i++) {
      var _d = _depths[_i];
      var ceil = ceiling(tissues, gfAtDepth(_d, gfL, gfH, firstStop, lastStop, !!shallowGradient));
      if (ceil > _d + 0.01) return false;
    }
    return true;
  }
  function buhNDL(depthM, fN2, gfLow, gfHigh, fHe, lastStop, decoStep, shallowGradient) {
    var fH = fHe || 0;
    var gfL = gfLow / 100;
    var gfH = gfHigh / 100;
    var tissues = initTissues();
    for (var t = 0; t <= 500; t++) {
      var next = saturate(tissues, depthM, 1, fN2, fH);
      if (ndlClearAtDepth(next, depthM, gfL, gfH, lastStop, decoStep, shallowGradient)) {
        tissues = next;
        continue;
      }
      return t;
    }
    return 500;
  }

  // Depends on zhl-physics-core.js: altSurfaceP, BAR_PER_METRE, allowO2AtMOD
  function enforceMinDecoProfile(steps, enabled, min9m, min6m, isMetric, fallbackGas, fallbackFN2, fallbackFHe) {
    if (!enabled || !min9m && !min6m) return steps;
    var depth9 = 9;
    var depth6 = 6;
    var FT_PER_M = 3.28084;
    function stepDepthToM(s) {
      var _ref, _s$depth;
      var raw = (_ref = (_s$depth = s.depth) !== null && _s$depth !== void 0 ? _s$depth : s.from) !== null && _ref !== void 0 ? _ref : s.to;
      if (raw == null) return null;
      return isMetric ? raw : raw / FT_PER_M;
    }
    function matchesStdMinStop(depthM, targetM) {
      return depthM != null && Math.abs(depthM - targetM) < 0.25;
    }
    var result = [];
    var enforced = {
      9: false,
      6: false
    };
    var _iterator = _createForOfIteratorHelper(steps),
      _step;
    try {
      for (_iterator.s(); !(_step = _iterator.n()).done;) {
        var s = _step.value;
        if (s.type === 'deco' || s.type === 'safety') {
          var depthM = stepDepthToM(s);
          if (matchesStdMinStop(depthM, depth9) && min9m > 0) {
            result.push(_objectSpread(_objectSpread({}, s), {}, {
              type: 'deco',
              dur: Math.max(s.dur, min9m)
            }));
            enforced[9] = true;
            continue;
          }
          if (matchesStdMinStop(depthM, depth6) && min6m > 0) {
            result.push(_objectSpread(_objectSpread({}, s), {}, {
              type: 'deco',
              dur: Math.max(s.dur, min6m)
            }));
            enforced[6] = true;
            continue;
          }
        }
        result.push(_objectSpread({}, s));
      }
    } catch (err) {
      _iterator.e(err);
    } finally {
      _iterator.f();
    }
    function resolveGasAtDepth(targetDepthM) {
      var activeGas = fallbackGas || '';
      var activeFN2 = fallbackFN2 !== null && fallbackFN2 !== void 0 ? fallbackFN2 : 0;
      var activeFHe = fallbackFHe !== null && fallbackFHe !== void 0 ? fallbackFHe : 0;
      for (var i = result.length - 1; i >= 0; i--) {
        var s = result[i];
        if (!s.gas || s.gas.trim() === '') continue;
        var stepDepthM = stepDepthToM(s);
        if (stepDepthM == null) continue;
        if (stepDepthM >= targetDepthM) {
          var _ref2, _s$fN, _ref3, _s$fHe;
          return {
            gas: s.gas,
            fN2: (_ref2 = (_s$fN = s.fN2) !== null && _s$fN !== void 0 ? _s$fN : activeFN2) !== null && _ref2 !== void 0 ? _ref2 : 0,
            fHe: (_ref3 = (_s$fHe = s.fHe) !== null && _s$fHe !== void 0 ? _s$fHe : activeFHe) !== null && _ref3 !== void 0 ? _ref3 : 0
          };
        }
      }
      return {
        gas: activeGas,
        fN2: activeFN2 !== null && activeFN2 !== void 0 ? activeFN2 : 0,
        fHe: activeFHe !== null && activeFHe !== void 0 ? activeFHe : 0
      };
    }
    function injectStop(targetDepthM, minDur) {
      var targetDisplay = isMetric ? targetDepthM : Math.round(targetDepthM * 3.28084);
      var insertIdx = result.length;
      for (var i = 0; i < result.length; i++) {
        var _s$to;
        var s = result[i];
        if (s.type === 'descent' || s.type === 'bottom') continue;
        var rawD = s.type === 'ascent' ? (_s$to = s.to) !== null && _s$to !== void 0 ? _s$to : s.depth : s.depth;
        if (rawD == null) continue;
        var d = isMetric ? rawD : rawD / FT_PER_M;
        if (d != null && d <= targetDepthM) {
          insertIdx = i;
          break;
        }
      }
      var _resolveGasAtDepth = resolveGasAtDepth(targetDepthM),
        gas = _resolveGasAtDepth.gas,
        fN2 = _resolveGasAtDepth.fN2,
        fHe = _resolveGasAtDepth.fHe;
      var straddle = result[insertIdx];
      if (straddle && straddle.type === 'ascent') {
        var sFromM = stepDepthToM({
          depth: straddle.from,
          from: straddle.from,
          to: straddle.to
        });
        var sToM = stepDepthToM({
          depth: straddle.to,
          from: straddle.from,
          to: straddle.to
        });
        if (sFromM > targetDepthM && sToM <= targetDepthM) {
          var lowerDur = straddle.dur * (sFromM - targetDepthM) / (sFromM - sToM);
          var upperDur = straddle.dur * (targetDepthM - sToM) / (sFromM - sToM);
          var lowerPiece = _objectSpread(_objectSpread({}, straddle), {}, {
            to: targetDisplay,
            dur: lowerDur
          });
          var upperPiece = _objectSpread(_objectSpread({}, straddle), {}, {
            from: targetDisplay,
            dur: upperDur
          });
          var injectRow = {
            type: 'deco',
            depth: targetDisplay,
            to: targetDisplay,
            dur: minDur,
            gas: gas,
            fN2: fN2,
            fHe: fHe,
            pO2: null
          };
          result.splice(insertIdx, 1, lowerPiece, injectRow, upperPiece);
          return;
        }
      }
      result.splice(insertIdx, 0, {
        type: 'deco',
        depth: targetDisplay,
        to: targetDisplay,
        dur: minDur,
        gas: gas,
        fN2: fN2,
        fHe: fHe,
        pO2: null
      });
    }
    if (!enforced[9] && min9m > 0) injectStop(depth9, min9m);
    if (!enforced[6] && min6m > 0) injectStop(depth6, min6m);
    return result;
  }
  function getActiveGas(curDepthM, bottomFN2, bottomFHe, decoGases, getPPO2LimitFn, bottomLabel) {
    var fHeBottom = bottomFHe || 0;
    var best = null;
    var bestFO2 = -1;
    var _iterator2 = _createForOfIteratorHelper(decoGases),
      _step2;
    try {
      for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
        var dg = _step2.value;
        if (curDepthM > dg.depth) continue;
        var fO2 = dg.fO2 != null ? dg.fO2 : Math.max(0, 1 - dg.fN2 - (dg.fHe || 0));
        var isPureO2 = fO2 >= 0.995 && allowO2AtMOD;
        if (!isPureO2) {
          var limit = getPPO2LimitFn ? getPPO2LimitFn(fO2) : 1.6;
          var ppO2AtCur = (altSurfaceP + curDepthM * BAR_PER_METRE) * fO2;
          if (ppO2AtCur > limit + 0.001) continue;
        }
        if (fO2 > bestFO2) {
          best = dg;
          bestFO2 = fO2;
        }
      }
    } catch (err) {
      _iterator2.e(err);
    } finally {
      _iterator2.f();
    }
    return best || {
      fN2: bottomFN2,
      fHe: fHeBottom,
      label: bottomLabel || 'Bottom'
    };
  }
  function ppO2Check(depthM, fN2, fHe, opts) {
    var fHeVal = fHe || 0;
    var fO2 = 1 - fN2 - fHeVal;
    var o2frac = Math.max(0, fO2);
    var pAmb = altSurfaceP + depthM * BAR_PER_METRE;
    if (opts && opts.onLoop && opts.ccr && isRebreatherCircuit(opts.ccr.circuit) && !opts.ccr.bailout) {
      var ccrFO2 = opts.fO2 != null ? opts.fO2 : o2frac;
      var surfP = opts.surfP != null ? opts.surfP : altSurfaceP;
      var sp = opts.setpoint != null ? opts.setpoint : getEffectiveSetpointAtDepth(depthM, opts.ccr, surfP);
      return getEffectivePpo2(pAmb, sp, ccrFO2, opts.ccr, depthM, fHeVal);
    }
    return pAmb * o2frac;
  }
  function n2FracFromCustomO2(o2pct) {
    var o2 = Number.isFinite(o2pct) ? o2pct : 21;
    return Math.max(0, (100 - o2) / 100);
  }
  function n2FracFromPercentages(o2pct, hepct) {
    if (Number.isFinite(o2pct) && Number.isFinite(hepct)) {
      var n2 = (100 - o2pct - hepct) / 100;
      if (n2 < 0 || n2 > 1) return null;
      return n2;
    }
    return null;
  }
  function validateHypoxicDecoGas(o2, he, field, circuit) {
    var heVal = he || 0;
    var label = String(field).replace(/^dg/, '');
    if (o2 + heVal > 100 + 1e-6) {
      return {
        ok: false,
        code: 'ERR_TOTAL_EXCEEDS_100',
        field: field,
        message: "Deco gas ".concat(label, ": O\u2082 + He exceeds 100%.")
      };
    }
    var isCCR = circuit === 'CCR' || circuit === 'pSCR';
    if (!isCCR && o2 < 18) {
      return {
        ok: false,
        code: 'HYPOXIC_DECO_GAS',
        field: field,
        message: "Deco gas ".concat(label, ": O\u2082 below 18% is hypoxic at stop depths.")
      };
    }
    return null;
  }
  function canonicalCircuit(circuit) {
    if (circuit == null || circuit === '') return 'OC';
    var u = String(circuit).trim().toUpperCase();
    if (u === 'CCR') return 'CCR';
    if (u === 'PSCR') return 'pSCR';
    return 'OC';
  }
  var PSCR_LOOP_VOLUME_MIN = 3;
  var PSCR_LOOP_VOLUME_MAX = 15;
  var PSCR_METABOLIC_O2_MIN = 0.5;
  var PSCR_METABOLIC_O2_MAX = 2.5;

  /** Closed field list — unknown keys on input are dropped; extend here when adding CCR settings. */
  function normalizeCCRSettings(s) {
    s = s || {};
    return {
      circuit: canonicalCircuit(s.circuit || 'OC'),
      setpoint: s.setpoint != null ? s.setpoint : s.decoSetpoint,
      decoSetpoint: s.decoSetpoint != null ? s.decoSetpoint : s.setpoint,
      bottomSetpoint: s.bottomSetpoint,
      descentSetpoint: s.descentSetpoint,
      bailout: !!s.bailout,
      bailoutGfLow: s.bailoutGfLow,
      bailoutGfHigh: s.bailoutGfHigh,
      scrLoopVolume: s.scrLoopVolume,
      scrMetabolicO2: s.scrMetabolicO2,
      sacStress: s.sacStress,
      sacDecoCcr: s.sacDecoCcr,
      stressTimeMin: s.stressTimeMin,
      problemSolveMin: s.problemSolveMin,
      ccrPhase: s.ccrPhase || null,
      scrRuntimeMin: s.scrRuntimeMin || 0
    };
  }
  function mergeCCRSettings(ccr) {
    return normalizeCCRSettings(ccr);
  }
  function isRebreatherCircuit(circuit) {
    var c = canonicalCircuit(circuit);
    return c === 'CCR' || c === 'pSCR';
  }
  function loopMixLabelForCore(diluentLabel, ccr) {
    var cfg = normalizeCCRSettings(ccr);
    if (!isRebreatherCircuit(cfg.circuit) || cfg.bailout) return diluentLabel;
    if (typeof diluentLabel === 'string' && /^(CCR|pSCR)\s/i.test(diluentLabel)) return diluentLabel;
    var prefix = cfg.circuit === 'pSCR' ? 'pSCR' : 'CCR';
    return "".concat(prefix, " ").concat(diluentLabel);
  }
  function depthAtSetpointCrossing(setpoint, surfP) {
    if (!Number.isFinite(setpoint) || setpoint <= 0) return null;
    var sp = surfP != null ? surfP : altSurfaceP;
    if (!Number.isFinite(sp) || sp <= 0) return null;
    var d = (setpoint + WATER_VAPOR - sp) / BAR_PER_METRE;
    return Number.isFinite(d) && d > 0 ? d : null;
  }
  function getEffectiveSetpointAtDepth(depthM, ccr, surfP, phase) {
    var cfg = normalizeCCRSettings(ccr);
    if (!cfg || cfg.bailout || !isRebreatherCircuit(cfg.circuit)) return 0;
    if (cfg.circuit === 'pSCR') return 0;
    var descSP = cfg.descentSetpoint != null ? cfg.descentSetpoint : 0.7;
    var bottomSP = cfg.bottomSetpoint != null ? cfg.bottomSetpoint : 1.2;
    var decoSP = cfg.decoSetpoint != null ? cfg.decoSetpoint : cfg.setpoint != null ? cfg.setpoint : 1.3;
    if (phase === 'descent') return descSP;
    if (phase === 'bottom') return bottomSP;
    if (phase === 'deco' || phase === 'ascent') return decoSP;
    var spSurf = surfP != null ? surfP : altSurfaceP;
    var descCross = depthAtSetpointCrossing(descSP, spSurf);
    var bottomCross = depthAtSetpointCrossing(bottomSP, spSurf);
    var decoCross = depthAtSetpointCrossing(decoSP, spSurf);
    if (descCross == null && bottomCross == null && decoCross == null) {
      var _pDry = spSurf + depthM * BAR_PER_METRE - WATER_VAPOR;
      if (_pDry >= decoSP) return decoSP;
      if (_pDry >= bottomSP) return bottomSP;
      return descSP;
    }
    var pAmb = spSurf + depthM * BAR_PER_METRE;
    var pDry = pAmb - WATER_VAPOR;
    if (depthM < 0.5) {
      if (pDry >= decoSP) return decoSP;
      if (pDry >= bottomSP) return bottomSP;
      return descSP;
    }
    var crossDepths = [descCross, bottomCross, decoCross].filter(function (d) {
      return d != null;
    });
    if (crossDepths.length === 0) {
      if (pDry >= decoSP) return decoSP;
      if (pDry >= bottomSP) return bottomSP;
      return descSP;
    }
    if (descCross != null && depthM <= descCross) {
      if (pDry >= bottomSP + 0.005) return bottomSP;
      return descSP;
    }
    if (descCross == null && bottomCross != null && depthM < bottomCross) {
      if (pDry >= bottomSP) return bottomSP;
      return descSP;
    }
    if (decoCross != null && depthM <= decoCross) return bottomSP;
    return decoSP;
  }

  /** @param {object} ccr — CCR settings; scrMetabolicO2 in L/min (Baker steady-state). */
  function getCcrMetabolicO2Rate(ccr) {
    var cfg = normalizeCCRSettings(ccr);
    var v = parseFloat(cfg.scrMetabolicO2);
    return v > 0 ? v : 1.5;
  }
  function parsePSCRParameters(ccr) {
    var loopVol = parseFloat(ccr.scrLoopVolume);
    if (!Number.isFinite(loopVol) || loopVol < PSCR_LOOP_VOLUME_MIN || loopVol > PSCR_LOOP_VOLUME_MAX) {
      throw new RangeError("pSCR loop volume must be between ".concat(PSCR_LOOP_VOLUME_MIN, " and ").concat(PSCR_LOOP_VOLUME_MAX, " litres"));
    }
    var metO2 = parseFloat(ccr.scrMetabolicO2);
    if (!Number.isFinite(metO2) || metO2 < PSCR_METABOLIC_O2_MIN || metO2 > PSCR_METABOLIC_O2_MAX) {
      throw new RangeError("pSCR metabolic O2 must be between ".concat(PSCR_METABOLIC_O2_MIN, " and ").concat(PSCR_METABOLIC_O2_MAX, " L/min"));
    }
    return {
      loopVol: loopVol,
      metO2: metO2
    };
  }

  /** pSCR loop fractions; metO2/loopVol yields bar/bar ppO2 drop (L/min ÷ L). */
  function computePSCRFractions(pAmb, fO2, fHe, ccr) {
    fO2 = Math.max(0, Math.min(1, fO2 || 0));
    fHe = Math.max(0, Math.min(1 - fO2, fHe || 0));
    var fN2src = Math.max(0, 1 - fO2 - fHe);
    var sourceInert = Math.max(0.001, fHe + fN2src);
    if (sourceInert <= 0.001 && fO2 >= 0.999) return {
      fO2: 1,
      fHe: 0,
      fN2: 0
    };
    var _parsePSCRParameters = parsePSCRParameters(ccr),
      loopVol = _parsePSCRParameters.loopVol,
      metO2 = _parsePSCRParameters.metO2;
    // Steady-state pSCR model: ppO2_loop = ppO2_supply - VO2/loopVol (Baker drop formula).
    // Previous model subtracted cumulative dive runtime × VO2 from a fixed loop volume,
    // which drove loop O2 to near-zero after a few minutes, zeroing N2 loading for the
    // rest of the dive. The steady-state formula is time-independent and depth-correct.
    var ppO2Drop = metO2 / loopVol;
    var ppO2Supply = fO2 * pAmb;
    var cappedDrop = Math.min(ppO2Drop, Math.max(0, ppO2Supply - PSCR_MIN_PPO2));
    var newPpO2 = ppO2Supply - cappedDrop;
    var newFO2 = Math.min(0.999, newPpO2 / Math.max(0.001, pAmb));
    var inertTotal = Math.max(0, 1 - newFO2);
    var heShare = fHe / sourceInert;
    var n2Share = fN2src / sourceInert;
    return {
      fO2: newFO2,
      fHe: inertTotal * heShare,
      fN2: inertTotal * n2Share
    };
  }
  function ccrLoopGasBelowSetpoint(pAmb, fO2, fHe, setpoint) {
    var ppH2O = WATER_VAPOR;
    var pDry = Math.max(0, pAmb - ppH2O);
    if (pDry <= 0.001) {
      return {
        fO2: 1,
        fN2: 0,
        fHe: 0,
        pN2: 0,
        pHe: 0
      };
    }
    var spTarget = setpoint > 0 ? Math.min(setpoint, pDry) : pDry;
    var fO2dry = Math.min(1, spTarget / pDry);
    var loopInertDry = Math.max(0, 1 - fO2dry);
    var fN2d = Math.max(0, 1 - fO2 - fHe);
    var inertSrc = Math.max(0.001, fHe + fN2d);
    var fHeEffDry = loopInertDry * (fHe / inertSrc);
    var fN2effDry = loopInertDry * (fN2d / inertSrc);
    var wetScale = pDry / Math.max(0.001, pAmb);
    return {
      fO2: fO2dry * wetScale,
      fN2: fN2effDry * wetScale,
      fHe: fHeEffDry * wetScale,
      pN2: pDry * fN2effDry,
      pHe: pDry * fHeEffDry
    };
  }
  function getInspiredInertPressures(pAmb, setpoint, fO2, fHe, ccr) {
    var ppH2O = WATER_VAPOR;
    var cfg = normalizeCCRSettings(ccr);
    if (cfg.bailout || !isRebreatherCircuit(cfg.circuit)) {
      var fN2 = Math.max(0, 1 - fO2 - fHe);
      return {
        pN2: (pAmb - ppH2O) * fN2,
        pHe: (pAmb - ppH2O) * fHe,
        fO2: fO2,
        fHe: fHe,
        fN2: fN2
      };
    }
    if (cfg.circuit === 'pSCR') {
      var fr = computePSCRFractions(pAmb, fO2, fHe, cfg);
      return {
        pN2: (pAmb - ppH2O) * fr.fN2,
        pHe: (pAmb - ppH2O) * fr.fHe,
        fO2: fr.fO2,
        fHe: fr.fHe,
        fN2: fr.fN2
      };
    }
    if (!setpoint || setpoint <= 0) {
      var _fN2d = Math.max(0, 1 - fO2 - fHe);
      var _pInert = Math.max(0, pAmb - ppH2O);
      return {
        pN2: _pInert * _fN2d,
        pHe: _pInert * fHe,
        fO2: fO2,
        fHe: fHe,
        fN2: _fN2d
      };
    }
    if (pAmb <= setpoint + ppH2O) {
      var loop = ccrLoopGasBelowSetpoint(pAmb, fO2, fHe, setpoint);
      return {
        pN2: loop.pN2,
        pHe: loop.pHe,
        fO2: loop.fO2,
        fHe: loop.fHe,
        fN2: loop.fN2
      };
    }
    var pInert = pAmb - setpoint - ppH2O;
    var fN2d = Math.max(0, 1 - fO2 - fHe);
    var den = Math.max(0.001, fN2d + fHe);
    return {
      pN2: pInert * fN2d / den,
      pHe: pInert * fHe / den,
      fO2: fO2,
      fHe: fHe,
      fN2: fN2d
    };
  }
  function getCCRInertSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, ccr) {
    var cfg = normalizeCCRSettings(ccr);
    if (cfg.bailout || !isRebreatherCircuit(cfg.circuit)) {
      var fN2 = Math.max(0, 1 - fO2 - fHe);
      var ppH2O = WATER_VAPOR;
      return {
        inspN2Start: (pAmbStart - ppH2O) * fN2,
        inspHeStart: (pAmbStart - ppH2O) * fHe,
        rN2: fN2 * pressureRate,
        rHe: fHe * pressureRate
      };
    }
    if (cfg.circuit === 'pSCR') {
      var fr0 = computePSCRFractions(pAmbStart, fO2, fHe, cfg);
      var pEnd = pAmbStart + pressureRate * 1;
      var fr1 = computePSCRFractions(pEnd, fO2, fHe, cfg);
      var _ppH2O = WATER_VAPOR;
      var _inspN2Start = (pAmbStart - _ppH2O) * fr0.fN2;
      var _inspHeStart = (pAmbStart - _ppH2O) * fr0.fHe;
      return {
        inspN2Start: _inspN2Start,
        inspHeStart: _inspHeStart,
        rN2: (pEnd - _ppH2O) * fr1.fN2 - _inspN2Start,
        rHe: (pEnd - _ppH2O) * fr1.fHe - _inspHeStart
      };
    }
    if (!setpoint || setpoint <= 0) {
      var _fN2d2 = Math.max(0, 1 - fO2 - fHe);
      var _ppH2O2 = WATER_VAPOR;
      return {
        inspN2Start: (pAmbStart - _ppH2O2) * _fN2d2,
        inspHeStart: (pAmbStart - _ppH2O2) * fHe,
        rN2: _fN2d2 * pressureRate,
        rHe: fHe * pressureRate
      };
    }
    if (pAmbStart <= setpoint + WATER_VAPOR) {
      var loop0 = ccrLoopGasBelowSetpoint(pAmbStart, fO2, fHe, setpoint);
      var loop1 = ccrLoopGasBelowSetpoint(pAmbStart + pressureRate, fO2, fHe, setpoint);
      return {
        inspN2Start: loop0.pN2,
        inspHeStart: loop0.pHe,
        rN2: loop1.pN2 - loop0.pN2,
        rHe: loop1.pHe - loop0.pHe
      };
    }
    var fN2d = Math.max(0, 1 - fO2 - fHe);
    var den = Math.max(0.001, fN2d + fHe);
    var coeffN2 = fN2d / den;
    var coeffHe = fHe / den;
    var inspN2Start = Math.max(0, pAmbStart - setpoint - WATER_VAPOR) * coeffN2;
    var inspHeStart = Math.max(0, pAmbStart - setpoint - WATER_VAPOR) * coeffHe;
    return {
      inspN2Start: inspN2Start,
      inspHeStart: inspHeStart,
      rN2: coeffN2 * pressureRate,
      rHe: coeffHe * pressureRate
    };
  }
  function getSetpointBoundaryDepths(ccr, surfP) {
    var cfg = normalizeCCRSettings(ccr);
    if (!isRebreatherCircuit(cfg.circuit) || cfg.bailout || cfg.circuit === 'pSCR') return [];
    var descSP = cfg.descentSetpoint != null ? cfg.descentSetpoint : 0.7;
    var bottomSP = cfg.bottomSetpoint != null ? cfg.bottomSetpoint : 1.2;
    var decoSP = cfg.decoSetpoint != null ? cfg.decoSetpoint : cfg.setpoint != null ? cfg.setpoint : 1.3;
    return [descSP, bottomSP, decoSP].map(function (sp) {
      return depthAtSetpointCrossing(sp, surfP);
    }).filter(function (d) {
      return d != null;
    });
  }
  function splitLinearDepthAtBoundaries(fromDepth, toDepth, boundaryDepths) {
    var lo = Math.min(fromDepth, toDepth);
    var hi = Math.max(fromDepth, toDepth);
    var ascending = toDepth >= fromDepth;
    var interior = boundaryDepths.filter(function (d) {
      return d > lo + 1e-6 && d < hi - 1e-6;
    }).sort(function (a, b) {
      return ascending ? a - b : b - a;
    });
    var pts = [fromDepth].concat(_toConsumableArray(interior), [toDepth]);
    var segs = [];
    for (var i = 0; i < pts.length - 1; i++) {
      if (Math.abs(pts[i] - pts[i + 1]) > 1e-6) segs.push({
        fromDepth: pts[i],
        toDepth: pts[i + 1]
      });
    }
    return segs.length ? segs : [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
  }
  function splitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP) {
    if (!setpoint || setpoint <= 0) return [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
    var cross = depthAtSetpointCrossing(setpoint, surfP);
    if (cross == null) return [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
    var lo = Math.min(fromDepth, toDepth);
    var hi = Math.max(fromDepth, toDepth);
    if (cross <= lo + 1e-6 || cross >= hi - 1e-6) return [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
    if (cross > lo && cross < hi) {
      return [{
        fromDepth: fromDepth,
        toDepth: cross
      }, {
        fromDepth: cross,
        toDepth: toDepth
      }];
    }
    return [{
      fromDepth: fromDepth,
      toDepth: toDepth
    }];
  }
  function schreinerLinearCCR(p0, ht, t, p0Amb, R, setpoint, fO2, fHe, ccr, isHe) {
    var params = getCCRInertSchreinerParams(p0Amb, setpoint, fO2, fHe, R, ccr);
    var pStart = isHe ? params.inspHeStart : params.inspN2Start;
    var rate = isHe ? params.rHe : params.rN2;
    var k = Math.LN2 / ht;
    return pStart + rate * (t - 1 / k) - (pStart - p0 - rate / k) * Math.exp(-k * t);
  }
  function saturateLinearCCR(tissues, fromDepth, toDepth, t, fO2, fHe, ccr) {
    if (t <= 0) return tissues;
    var cfg = normalizeCCRSettings(ccr);
    var surfP = altSurfaceP;
    var phase = cfg.ccrPhase || null;
    if (Math.abs(fromDepth - toDepth) < 1e-9) {
      return saturateCCR(tissues, fromDepth, t, fO2, fHe, cfg);
    }
    var segments = splitLinearDepthAtBoundaries(fromDepth, toDepth, getSetpointBoundaryDepths(cfg, surfP));
    var out = tissues;
    var totalTime = t;
    var totalDist = Math.abs(toDepth - fromDepth) || 1e-9;
    var _iterator3 = _createForOfIteratorHelper(segments),
      _step3;
    try {
      var _loop = function _loop() {
        var seg = _step3.value;
        var segTime = Math.abs(seg.toDepth - seg.fromDepth) / totalDist * totalTime;
        if (!(segTime > 0)) return 1; // continue
        var p0Amb = depthBar(seg.fromDepth);
        var pEndAmb = depthBar(seg.toDepth);
        var R = (pEndAmb - p0Amb) / segTime;
        // [AUDIT-REG-07] setpoint sampled at deep segment endpoint (ascent uses fromDepth)
        var endpointDepth = seg.fromDepth < seg.toDepth ? seg.toDepth : seg.fromDepth;
        var segSP = getEffectiveSetpointAtDepth(endpointDepth, cfg, surfP, phase);
        var segCcr = _objectSpread(_objectSpread({}, cfg), {}, {
          setpoint: segSP
        });
        out = out.map(function (t0, i) {
          return {
            pN2: schreinerLinearCCR(t0.pN2, ZHL16C[i][0], segTime, p0Amb, R, segSP, fO2, fHe, segCcr, false),
            pHe: fHe > 0 ? schreinerLinearCCR(t0.pHe, ZHL16C_HE_HT[i], segTime, p0Amb, R, segSP, fO2, fHe, segCcr, true) : t0.pHe
          };
        });
      };
      for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
        if (_loop()) continue;
      }
    } catch (err) {
      _iterator3.e(err);
    } finally {
      _iterator3.f();
    }
    return out;
  }
  function saturateCCR(tissues, depthM, t, fO2, fHe, ccr) {
    if (t <= 0) return tissues;
    var cfg = normalizeCCRSettings(ccr);
    var pAmb = depthBar(depthM);
    var phase = cfg.ccrPhase || null;
    var sp = getEffectiveSetpointAtDepth(depthM, cfg, altSurfaceP, phase);
    var segCcr = _objectSpread(_objectSpread({}, cfg), {}, {
      setpoint: sp
    });
    var insp = getInspiredInertPressures(pAmb, sp, fO2, fHe, segCcr);
    return tissues.map(function (t0, i) {
      return {
        pN2: schreiner(t0.pN2, insp.pN2, ZHL16C[i][0], t),
        pHe: schreiner(t0.pHe, insp.pHe, ZHL16C_HE_HT[i], t)
      };
    });
  }
  function loadTissuesWithCCR(tissues, fromDepth, toDepth, time, fO2, fHe, ccr, constantDepth) {
    var cfg = normalizeCCRSettings(ccr);
    if (cfg.setpoint === 0 && !cfg.bailout && isRebreatherCircuit(cfg.circuit)) {
      var fN2 = Math.max(0, 1 - fO2 - fHe);
      if (constantDepth || Math.abs(fromDepth - toDepth) < 1e-6) {
        return saturate(tissues, fromDepth, time, fN2, fHe);
      }
      return saturateLinear(tissues, fromDepth, toDepth, time, fN2, fHe);
    }
    if (!isRebreatherCircuit(cfg.circuit) || cfg.bailout) {
      var _fN = Math.max(0, 1 - fO2 - fHe);
      if (constantDepth || Math.abs(fromDepth - toDepth) < 1e-6) {
        return saturate(tissues, fromDepth, time, _fN, fHe);
      }
      return saturateLinear(tissues, fromDepth, toDepth, time, _fN, fHe);
    }
    if (constantDepth || Math.abs(fromDepth - toDepth) < 1e-6) {
      return saturateCCR(tissues, fromDepth, time, fO2, fHe, cfg);
    }
    return saturateLinearCCR(tissues, fromDepth, toDepth, time, fO2, fHe, cfg);
  }
  function getEffectivePpo2(pAmb, setpoint, fO2, ccr, depthM, fHe) {
    var cfg = normalizeCCRSettings(ccr);
    if (cfg.bailout || !isRebreatherCircuit(cfg.circuit)) return fO2 * pAmb;
    if (cfg.circuit === 'pSCR') {
      var fHeVal = fHe != null ? fHe : 0;
      var fr = computePSCRFractions(pAmb, fO2, fHeVal, cfg);
      return fr.fO2 * pAmb;
    }
    var surfPRef = typeof altSurfaceP !== 'undefined' ? altSurfaceP : 1.01325;
    var depthFromAmb = depthM != null ? depthM : (pAmb - surfPRef) / BAR_PER_METRE;
    var sp = setpoint != null ? setpoint : getEffectiveSetpointAtDepth(depthFromAmb, cfg, surfPRef);
    var pDry = Math.max(0, pAmb - WATER_VAPOR);
    var dilPpo2 = fO2 * pAmb;
    return Math.min(pDry, Math.max(sp, dilPpo2));
  }
  function getGasLabel(fO2, fHe) {
    if (fO2 === null || fO2 === undefined) return null;
    if (fO2 >= 0.995) return '100%';
    var o2pct = Math.round(fO2 * 100);
    var hePct = Math.round((fHe || 0) * 100);
    if (o2pct === 21 && hePct === 0) return 'Air';
    return "".concat(String(o2pct).padStart(2, '0'), "/").concat(String(hePct).padStart(2, '0'));
  }
  function insertMdpStopDepths(stopDepths, enabled, min9m, min6m) {
    if (!enabled) return stopDepths;
    var out = stopDepths.slice();
    var _loop2 = function _loop2() {
        var _arr$_i = _slicedToArray(_arr[_i2], 2),
          targetM = _arr$_i[0],
          minDur = _arr$_i[1];
        if (minDur <= 0) return 0; // continue
        if (out.some(function (d) {
          return Math.abs(d - targetM) < 0.25;
        })) return 0; // continue
        var idx = out.findIndex(function (d) {
          return d < targetM;
        });
        if (idx < 0) idx = out.length;
        out.splice(idx, 0, targetM);
      },
      _ret;
    for (var _i2 = 0, _arr = [[9, min9m], [6, min6m]]; _i2 < _arr.length; _i2++) {
      _ret = _loop2();
      if (_ret === 0) continue;
    }
    return out;
  }
  function mdpMinRequiredAt(depthM, min9m, min6m) {
    if (Math.abs(depthM - 9) < 0.25 && min9m > 0) return min9m;
    if (Math.abs(depthM - 6) < 0.25 && min6m > 0) return min6m;
    return 0;
  }
  function runZhlScheduleCore(params) {
    var _params$decoAscentRat, _params$surfaceAscent, _params$minDecoProfil, _params$minDecoProfil2, _params$minDecoProfil3, _params$minDecoProfil4, _params$minDecoProfil5;
    applyEnvironment(params.environment || defaultEnvironment());
    var depthM = params.depthM;
    var bt = params.bt;
    var rate = params.ascentRate;
    var decoRate = (_params$decoAscentRat = params.decoAscentRate) !== null && _params$decoAscentRat !== void 0 ? _params$decoAscentRat : 3;
    var surfaceRate = (_params$surfaceAscent = params.surfaceAscentRate) !== null && _params$surfaceAscent !== void 0 ? _params$surfaceAscent : 3;
    var descentRate = params.descentRate;
    var gfL = params.gfL;
    var gfH = params.gfH;
    var ppo2Bottom = params.ppo2Bottom;
    var ppo2Deco = params.ppo2Deco;
    var minStopT = params.minStopTime;
    var switchPauseT = params.switchPauseT || 0;
    var mdCompatMode = params.mdCompatMode !== false;
    var wholeMinStops = params.wholeMinStops !== false;
    function stopRoundGrid() {
      return params.wholeMinStops === true ? 1 : minStopT;
    }
    function snapStopToGrid(stopT) {
      if (!wholeMinStops) return stopT;
      var grid = stopRoundGrid();
      return Math.max(minStopT, Math.ceil((stopT - 1e-9) / grid) * grid);
    }
    var lastStop = params.lastStop;
    var decoStep = params.decoStep;
    var ppo2High = ppo2Deco;
    var ppo2Mid = 1.5;
    var ppo2Low = params.ppo2Bottom;
    var bottomFN2 = params.bottomFN2;
    var bottomFHe = params.bottomFHe;
    var bottomFO2 = params.bottomFO2;
    var bottomMixLabel = params.bottomMixLabel;
    var decoGases = params.decoGases;
    var _mdpEnabled = !!((_params$minDecoProfil = params.minDecoProfile) !== null && _params$minDecoProfil !== void 0 && _params$minDecoProfil.enabled);
    var _mdp9m = (_params$minDecoProfil2 = (_params$minDecoProfil3 = params.minDecoProfile) === null || _params$minDecoProfil3 === void 0 ? void 0 : _params$minDecoProfil3.m9) !== null && _params$minDecoProfil2 !== void 0 ? _params$minDecoProfil2 : 1;
    var _mdp6m = (_params$minDecoProfil4 = (_params$minDecoProfil5 = params.minDecoProfile) === null || _params$minDecoProfil5 === void 0 ? void 0 : _params$minDecoProfil5.m6) !== null && _params$minDecoProfil4 !== void 0 ? _params$minDecoProfil4 : 3;
    var ccrSettings = params.ccr ? normalizeCCRSettings(params.ccr) : null;
    var _zhlOnLoop = !!(params.onLoop && ccrSettings && isRebreatherCircuit(ccrSettings.circuit) && !ccrSettings.bailout);
    var loopMixLabel = params.loopMixLabel || (ccrSettings ? loopMixLabelForCore(bottomMixLabel, ccrSettings) : bottomMixLabel);
    // CCR-loop elapsed time only (OC segments do not increment).
    var _ccrLoopElapsedMin = 0;
    var hitSafetyGuard = false;
    var CEILING_LOOP_GUARD_MIN = 1440;
    function zhlLoadLinear(tissues, from, to, t, fO2, fHe, onLoop, phase) {
      if (onLoop && ccrSettings) {
        var out = loadTissuesWithCCR(tissues, from, to, t, fO2, fHe, _objectSpread(_objectSpread({}, ccrSettings), {}, {
          scrRuntimeMin: _ccrLoopElapsedMin,
          ccrPhase: phase
        }));
        _ccrLoopElapsedMin += t;
        return out;
      }
      return saturateLinear(tissues, from, to, t, Math.max(0, 1 - fO2 - (fHe || 0)), fHe || 0);
    }
    function zhlLoadConst(tissues, depth, t, fO2, fHe, onLoop, phase) {
      if (onLoop && ccrSettings) {
        var out = loadTissuesWithCCR(tissues, depth, depth, t, fO2, fHe, _objectSpread(_objectSpread({}, ccrSettings), {}, {
          scrRuntimeMin: _ccrLoopElapsedMin,
          ccrPhase: phase
        }));
        _ccrLoopElapsedMin += t;
        return out;
      }
      return saturate(tissues, depth, t, Math.max(0, 1 - fO2 - (fHe || 0)), fHe || 0);
    }
    function zhlOnLoopAt() {
      return !!_zhlOnLoop;
    }
    function zhlStepPpo2(depthM, fN2, fHe, phase) {
      if (_zhlOnLoop && ccrSettings) {
        var sp = getEffectiveSetpointAtDepth(depthM, ccrSettings, altSurfaceP, phase || (decoZoneEntered ? 'deco' : 'bottom'));
        if (sp > 0) return sp;
      }
      return ppO2Check(depthM, fN2, fHe);
    }
    function zhlGasAt(depthM) {
      if (_zhlOnLoop) {
        return {
          fN2: bottomFN2,
          fHe: bottomFHe,
          fO2: bottomFO2,
          label: bottomMixLabel
        };
      }
      var baseN2 = _zhlActiveGas ? _zhlActiveGas.fN2 : bottomFN2;
      var baseHe = _zhlActiveGas ? _zhlActiveGas.fHe : bottomFHe;
      var baseLabel = _zhlActiveGas ? _zhlActiveGas.label : bottomMixLabel;
      return getActiveGas(depthM, baseN2, baseHe, decoGases, getPPO2Limit, baseLabel);
    }
    function getPPO2Limit(fO2) {
      var fO2pct = fO2 * 100;
      if (fO2pct >= 45) return ppo2High;
      if (fO2pct >= 28) return ppo2Mid;
      return ppo2Low;
    }
    var travelInfo = params.travelInfo || null;
    var travelSwitchM = travelInfo ? Math.min(travelInfo.switchDepthM, depthM) : 0;

    // Saturate tissues at depth for bottom time
    var tissues = initTissues();

    // ── Repetitive dive tissue carry (ZHL) ───────────────────────────────────
    // DOM schedule writes window._zhlRepState after each Bühlmann run; repState is
    // injected when the user enables the repetitive-dive checkbox.
    if (params.repState && Array.isArray(params.repState.tissues)) {
      var rep = params.repState;
      for (var i = 0; i < tissues.length && i < rep.tissues.length; i++) {
        tissues[i].pN2 = rep.tissues[i].pN2;
        tissues[i].pHe = rep.tissues[i].pHe || 0;
      }
      if (rep.surfaceIntervalMin > 0) {
        var siMin = rep.surfaceIntervalMin;
        var wv = WATER_VAPOR || 0.0627;
        var repSurfP = rep.surfaceP != null ? rep.surfaceP : altAcclimatized !== false ? altSurfaceP || SEA_LEVEL_P : SEA_LEVEL_P;
        var inspN2 = 0.7902 * (repSurfP - wv);
        for (var _i3 = 0; _i3 < tissues.length; _i3++) {
          var kN2 = Math.LN2 / ZHL16C[_i3][0];
          var htHe = ZHL16C_HE_HT[_i3];
          if (!(htHe > 0)) throw new Error('ZHL16C_HE_HT missing compartment ' + _i3);
          var kHe = Math.LN2 / htHe;
          tissues[_i3].pN2 = inspN2 + (tissues[_i3].pN2 - inspN2) * Math.exp(-kN2 * siMin);
          tissues[_i3].pHe = (tissues[_i3].pHe || 0) * Math.exp(-kHe * siMin);
        }
      }
    }

    // Descent phase — split by travel gas switch depth if travel gas is active
    var descentTime = depthM / descentRate;
    if (travelInfo && travelSwitchM > 0 && travelSwitchM < depthM) {
      var travelFHe = travelInfo.fHe || 0;
      var travelFO2;
      if (travelInfo.fO2 != null) {
        travelFO2 = travelInfo.fO2;
      } else {
        var inferred = 1 - travelInfo.fN2 - travelFHe;
        if (inferred < -1e-9) throw new Error('travelInfo gas fractions invalid: fN2 + fHe > 1');
        travelFO2 = Math.max(0, inferred);
      }
      // Phase 1: surface → travel switch depth on travel gas
      var travelDescentTime = travelSwitchM / descentRate;
      tissues = zhlLoadLinear(tissues, 0, travelSwitchM, travelDescentTime, travelFO2, travelFHe, _zhlOnLoop, 'descent');
      // Phase 2: travel switch depth → bottom on bottom gas
      var bottomDescentTime = (depthM - travelSwitchM) / descentRate;
      tissues = zhlLoadLinear(tissues, travelSwitchM, depthM, bottomDescentTime, bottomFO2, bottomFHe, _zhlOnLoop, 'descent');
    } else {
      // No travel gas or switch depth >= bottom: entire descent on bottom gas
      tissues = zhlLoadLinear(tissues, 0, depthM, descentTime, bottomFO2, bottomFHe, _zhlOnLoop, 'descent');
    }

    // Bottom time input = total time from leaving surface (industry standard).
    // Subtract descent time to get actual time spent at depth.
    var btAtDepth = Math.max(0, bt - descentTime);
    tissues = zhlLoadConst(tissues, depthM, btAtDepth, bottomFO2, bottomFHe, _zhlOnLoop, 'bottom');
    var tissuesAtBottom = _toConsumableArray(tissues); // snapshot for ceiling graph overlay (deepest level only; ML phases not included)

    // ── Decozone start (GF-INDEPENDENT) ──────────────────────────────────────
    // Evaluated at end-of-bottom tissue state, matching DiveKit's convention:
    // the depth where the leading compartment's raw inert-gas tension first
    // exceeds ambient pressure, with NO GF/M-value involved. Must NOT vary with
    // gfLo/gfHi for the same physical dive (see ambientCrossingDepth() above).
    var trueDecoZoneStart = ambientCrossingDepth(tissuesAtBottom);
    var steps = [];
    var cur = depthM;
    var rt = bt; // run time = full BT input (descent already counted in BT)

    // ── Multi-level headless: monotonic-shallower continuation after deepest level ──
    var _zhlContLevels = Array.isArray(params.continuationLevels) ? params.continuationLevels : [];
    var _zhlAscentFloors = _zhlContLevels.length ? _zhlContLevels.map(function (c) {
      return c.depth;
    }).concat([0]) : [0];
    var firstStopDepth = 0;
    var carriedFirstStopDepth = 0;
    var _zhlActiveGas = null; // continuation-level mix carried until deco-gas switch

    // gfAt must live outside the phase loop — block-scoped function declarations are
    // not visible after the loop in strict mode (Tier 3 bundle uses 'use strict').
    // [AUDIT-REG-06] pre-anchor gfAt returns gfL for Baker first-stop search
    function gfAt(depthM) {
      if (!firstStopDepth || firstStopDepth <= 0) return gfL;
      return gfAtDepth(depthM, gfL, gfH, firstStopDepth, lastStop, !!params.shallowGradient);
    }
    var _loop3 = function _loop3() {
      var _zhlAscentFloor = _zhlAscentFloors[_zhlPhaseIdx];
      firstStopDepth = carriedFirstStopDepth;

      // ── GF anchor: candidate stop list built from ceiling(bottom_tissues, gfL) ──
      // firstStopDepth is NOT pre-computed here — it is anchored dynamically at the
      // FIRST depth where mustStop actually fires. This matches MultiDeco/Baker:
      // GF line is pinned at the actual first required stop, not at a pre-computed
      // ceiling that may be one step above the real first stop.
      var bottomCeil = ceiling(tissues, gfL);
      var candidateFirstStop = bottomCeil > 0 ? Math.max(lastStop, Math.ceil((bottomCeil + 1e-9) / decoStep) * decoStep) : 0;

      // firstStopDepth: mutable — set when the first ceiling-forced stop is reached.
      // Until it is set, gfAt() returns gfL (GF Low determines the search for the
      // first stop, per Baker; GF line is not yet anchored/interpolated).

      // ── Stop-based ascent engine ──
      // Start stop iteration from candidateFirstStop — ascent from bottom to first stop
      // is a single linear segment. Gas switch happens at the first stop where it's available.
      // This matches ApexDeco: ascend to first stop, then iterate stops down to lastStop.
      var startStop = candidateFirstStop > 0 ? candidateFirstStop : lastStop;
      var stopDepths = [];
      var floorStopMin = _zhlAscentFloor > 0 ? Math.max(lastStop, _zhlAscentFloor) : lastStop;
      for (var d = startStop; d >= floorStopMin; d -= decoStep) {
        stopDepths.push(d);
      }
      if (_zhlAscentFloor > 0) {
        if (stopDepths.length === 0 || stopDepths[stopDepths.length - 1] !== floorStopMin) stopDepths.push(floorStopMin);
      } else if (stopDepths.length === 0 || stopDepths[stopDepths.length - 1] !== lastStop) {
        stopDepths.push(lastStop);
      }
      stopDepths = insertMdpStopDepths(stopDepths, _mdpEnabled, _mdp9m, _mdp6m);
      function applyMdpMinimumStop(cur, stopT, transitForDisplay, isFirstDecoStop) {
        var minReq = mdpMinRequiredAt(cur, _mdp9m, _mdp6m);
        if (!_mdpEnabled || minReq <= 0) return stopT;
        var displayT = mdCompatMode && !isFirstDecoStop ? stopT + transitForDisplay : stopT;
        if (displayT >= minReq) return stopT;
        return stopT + (minReq - displayT);
      }
      function loadMdpStopExtension(cur, stopT, transitForDisplay, isFirstDecoStop, stopFO2, stopFHe, onLoop) {
        var extended = applyMdpMinimumStop(cur, stopT, transitForDisplay, isFirstDecoStop);
        if (extended <= stopT) return stopT;
        var extra = extended - stopT;
        tissues = zhlLoadConst(tissues, cur, extra, stopFO2, stopFHe, onLoop, 'deco');
        rt += extra;
        return extended;
      }
      var prevEngineGas = _zhlActiveGas ? _zhlActiveGas.label : bottomMixLabel; // track gas for switch pause
      var decoZoneEntered = false; // set when ceiling-forced or min-stop deco actually begins

      // firstSwitchDepth — find first deco gas switch depth
      var firstDecoDepth = null;
      var firstSwitchDepth = null;
      {
        var simCur = cur;
        var simPrevGas = bottomMixLabel;
        var _iterator4 = _createForOfIteratorHelper(stopDepths),
          _step4;
        try {
          for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
            var sd = _step4.value;
            if (simCur > sd) simCur = sd;
            var gas2 = zhlGasAt(simCur);
            if (gas2.label !== simPrevGas) {
              firstSwitchDepth = simCur;
              break;
            }
            simPrevGas = gas2.label;
          }
        } catch (err) {
          _iterator4.e(err);
        } finally {
          _iterator4.f();
        }
      }

      // minStop zone: only enforce minimum stops within the GF-anchored deco zone.
      // Dynamically updated when firstStopDepth is set — starts null (no min-stop
      // enforcement until the first required stop depth is known).
      var minStopZoneDepth = null;
      for (var si = 0; si < stopDepths.length; si++) {
        var stopDepth = stopDepths[si];
        var nextStop = si + 1 < stopDepths.length ? stopDepths[si + 1] : 0;

        // Travel from cur to stopDepth — use appropriate ascent rate:
        // - Before first deco stop: use main ascent rate
        // - Between deco stops: use decoRate
        var legTransitDur = 0;
        if (cur > stopDepth) {
          var travelGas = zhlGasAt(cur);
          var travelRate = decoZoneEntered ? decoRate : rate;
          var travelDur = (cur - stopDepth) / travelRate;
          legTransitDur = travelDur;
          var travelOnLoop = zhlOnLoopAt();
          var travelPhase = decoZoneEntered ? 'deco' : 'bottom';
          if (decoZoneEntered && mdCompatMode) {
            // MultiDeco-compatible mode: treat deco-zone transit as instant for tissue loading.
            // Transit time is still counted in RT and added to the displayed stop duration below.
            // (Schreiner mode: tissues off-gas normally during transit — more accurate.)
            if (travelOnLoop && ccrSettings && isRebreatherCircuit(ccrSettings.circuit)) _ccrLoopElapsedMin += travelDur;
          } else {
            var tFO2 = travelOnLoop ? bottomFO2 : travelGas.fO2 != null ? travelGas.fO2 : Math.max(0, 1 - travelGas.fN2 - (travelGas.fHe || 0));
            var tFHe = travelOnLoop ? bottomFHe : travelGas.fHe || 0;
            tissues = zhlLoadLinear(tissues, cur, stopDepth, travelDur, tFO2, tFHe, travelOnLoop, travelPhase);
          }
          steps.push({
            type: 'ascent',
            from: cur,
            to: stopDepth,
            dur: travelDur,
            gas: travelOnLoop ? loopMixLabel : travelGas.label,
            pO2: zhlStepPpo2(cur, travelGas.fN2, travelGas.fHe || 0, travelPhase),
            fN2: travelGas.fN2,
            fHe: travelGas.fHe || 0,
            decoTransit: decoZoneEntered && mdCompatMode && firstDecoDepth !== null
          });
          rt += travelDur;
          cur = stopDepth;
        }

        // Transit time for minimum stop rounding (ApexDeco style):
        // si=0: actual ascent time to this stop (fast rate before deco zone)
        // si>0: travelled at decoRate between stops
        var transitDur = si === 0 ? legTransitDur : (stopDepths[si - 1] - stopDepth) / decoRate;

        // Select best gas available at this stop depth
        var stopGas = zhlGasAt(cur);
        var onLoop = zhlOnLoopAt();
        var stopFN2 = onLoop ? bottomFN2 : stopGas.fN2;
        var stopFHe = onLoop ? bottomFHe : stopGas.fHe || 0;
        var stopFO2 = onLoop ? bottomFO2 : stopGas.fO2 != null ? stopGas.fO2 : Math.max(0, 1 - stopFN2 - stopFHe);
        var gasLabel = onLoop ? loopMixLabel : stopGas.label;

        // Gas switch pause — saturate tissues at this depth during the switch
        if (gasLabel !== prevEngineGas && switchPauseT > 0) {
          tissues = zhlLoadConst(tissues, cur, switchPauseT, stopFO2, stopFHe, onLoop, 'deco');
          rt += switchPauseT;
        }
        prevEngineGas = gasLabel;

        // Ceiling clearance: evaluate GF at the TARGET (next stop or surface),
        // not the current stop. Baker/ApexDeco: "can I ascend TO the next stop?"
        // At last stop: target=0 → uses gfH (surface GF). This is correct.
        // Use nextStop exactly — ceiling must be strictly below the next stop.
        var phaseNextStop = _zhlAscentFloor > 0 ? Math.max(_zhlAscentFloor, nextStop) : nextStop;
        var ceilTarget = phaseNextStop < lastStop ? 0 : phaseNextStop;
        var gfForClear = gfAt(phaseNextStop < lastStop ? 0 : phaseNextStop);
        var isFirstDecoStop = firstDecoDepth === null;
        // Step resolution: first deco stop uses fine (10-sec) resolution, matching
        // ApexDeco/MultiDeco's fractional first-stop snap. Subsequent stops use
        // 1-min steps for ceiling resolution (MultiDeco behaviour), regardless of
        // minStopT, ensuring stops don't under-count.
        // BUGFIX (v2.10.44): headless mode previously forced holdStep=1 even for
        // the first stop (a test-speed shortcut), which coarsened the while-loop's
        // own resolution and inflated the first-stop time relative to what the
        // real app and headless test harnesses both expect — this is the only
        // _zhlHeadless branch in the file that changes a computed RESULT rather
        // than skipping DOM rendering, so it was silently producing different
        // RT/TTS numbers in headless tests than the real app would for the same
        // inputs. Subsequent (non-first) stops still use the coarser 1-min step
        // in headless mode for speed, since that resolution already matches the
        // real app's own non-first-stop behavior.
        var holdStep = isFirstDecoStop ? 1 / 6 : 1;
        var ceil = ceiling(tissues, gfForClear);
        var mustStop = ceil > ceilTarget;
        if (mustStop) {
          // Record the first depth where ceiling forces a stop.
          // CRITICAL: anchor firstStopDepth here (not pre-computed from bottom tissues).
          // This matches MultiDeco/Baker: GF line is anchored at the ACTUAL first stop depth.
          if (firstDecoDepth === null) {
            firstDecoDepth = cur;
            firstStopDepth = cur; // anchor GF line at real first stop
            carriedFirstStopDepth = cur;
            minStopZoneDepth = cur; // enable min-stop enforcement from here down
            // When the first required stop is already the final stop, clearance
            // is to the surface. Do not reuse the pre-anchor GF Low value: the
            // newly anchored line reaches GF High at the surface.
            if (phaseNextStop < lastStop) gfForClear = gfAt(0);
          }
          decoZoneEntered = true;
          // Capture RT before ceiling loop — ApexDeco snaps the arrival RT to next minute
          var rtOnArrival = rt;
          var stopT = 0;
          while (ceiling(tissues, gfForClear) > ceilTarget && stopT < CEILING_LOOP_GUARD_MIN) {
            tissues = zhlLoadConst(tissues, cur, holdStep, stopFO2, stopFHe, onLoop, 'deco');
            stopT += holdStep;
            rt += holdStep;
          }
          if (stopT >= CEILING_LOOP_GUARD_MIN && ceiling(tissues, gfForClear) > ceilTarget) hitSafetyGuard = true;
          if (isFirstDecoStop) {
            // First stop: always use RT-snap (fractional) — both ApexDeco and MultiDeco
            // keep the exact first-stop time (e.g. 0:33, 0:27) regardless of rounding mode.
            var rawRounded = Math.round(stopT * 60) / 60;
            var minFirstStop = Math.round((Math.ceil(rtOnArrival / minStopT) * minStopT - rtOnArrival) * 60) / 60;
            var actualStop = Math.max(rawRounded, minFirstStop);
            if (actualStop > stopT) {
              var extra = actualStop - stopT;
              tissues = zhlLoadConst(tissues, cur, extra, stopFO2, stopFHe, onLoop, 'deco');
              rt += extra;
              stopT = actualStop;
            }
            if (stopT < 1 / 60) {
              tissues = zhlLoadConst(tissues, cur, 1 / 60 - stopT, stopFO2, stopFHe, onLoop, 'deco');
              rt += 1 / 60 - stopT;
              stopT = 1 / 60;
            }
          } else {
            var roundedStop = void 0;
            if (wholeMinStops) {
              var grid = stopRoundGrid();
              var totalAtLevel = Math.max(minStopT, Math.ceil((transitDur + stopT - 1e-9) / grid) * grid);
              roundedStop = totalAtLevel - transitDur;
            } else {
              roundedStop = stopT;
            }
            if (roundedStop > stopT) {
              var _extra = roundedStop - stopT;
              tissues = zhlLoadConst(tissues, cur, _extra, stopFO2, stopFHe, onLoop, 'deco');
              rt += _extra;
              stopT = roundedStop;
            }
            // Enforce minimum stop time — every non-first deco stop gets at least minStopT
            if (stopT < minStopT) {
              var _extra2 = minStopT - stopT;
              tissues = zhlLoadConst(tissues, cur, _extra2, stopFO2, stopFHe, onLoop, 'deco');
              rt += _extra2;
              stopT = minStopT;
            }
          }
          stopT = loadMdpStopExtension(cur, stopT, transitDur, isFirstDecoStop, stopFO2, stopFHe, onLoop);
          var mustStopDisplay = mdCompatMode && !isFirstDecoStop ? stopT + transitDur : stopT;
          steps.push({
            type: 'deco',
            depth: cur,
            dur: mustStopDisplay,
            nativeDuration: stopT,
            gas: gasLabel,
            pO2: zhlStepPpo2(cur, stopFN2, stopFHe, 'deco'),
            fN2: stopFN2,
            fHe: stopFHe,
            hitSafetyGuard: hitSafetyGuard || undefined,
            _tissues: tissues.map(function (t) {
              return {
                pN2: t.pN2,
                pHe: t.pHe
              };
            })
          });
        } else if (minStopT > 0 && minStopZoneDepth !== null && cur <= minStopZoneDepth && cur !== lastStop) {
          decoZoneEntered = true;
          var _stopT = 0;
          if (isFirstDecoStop) {
            if (firstDecoDepth === null) {
              firstDecoDepth = cur;
              firstStopDepth = cur;
              carriedFirstStopDepth = cur;
              minStopZoneDepth = cur;
            }
            var _minFirstStop = Math.ceil(rt / minStopT) * minStopT - rt;
            var snapped = Math.max(0, Math.round(_minFirstStop * 60) / 60);
            // Always keep meaningful fractional first stop (RT-snap). MultiDeco behaviour.
            // Never pad first stop to full minStopT — only enforce 1-sec minimum.
            _stopT = Math.max(snapped, 1 / 60);
          } else {
            var needed = Math.max(0, minStopT - transitDur);
            _stopT = needed;
          }
          if (_stopT > 0) {
            tissues = zhlLoadConst(tissues, cur, _stopT, stopFO2, stopFHe, onLoop, 'deco');
            rt += _stopT;
          }
          while (ceiling(tissues, gfForClear) > ceilTarget && _stopT < CEILING_LOOP_GUARD_MIN) {
            tissues = zhlLoadConst(tissues, cur, minStopT, stopFO2, stopFHe, onLoop, 'deco');
            _stopT += minStopT;
            rt += minStopT;
          }
          if (_stopT >= CEILING_LOOP_GUARD_MIN && ceiling(tissues, gfForClear) > ceilTarget) hitSafetyGuard = true;
          if (!isFirstDecoStop) {
            // Round up and enforce minimum — only for non-first stops
            if (wholeMinStops) {
              var _grid = stopRoundGrid();
              var _totalAtLevel = Math.max(minStopT, Math.ceil((transitDur + _stopT - 1e-9) / _grid) * _grid);
              var _roundedStop = _totalAtLevel - transitDur;
              if (_roundedStop > _stopT) {
                var _extra3 = _roundedStop - _stopT;
                tissues = zhlLoadConst(tissues, cur, _extra3, stopFO2, stopFHe, onLoop, 'deco');
                rt += _extra3;
                _stopT = _roundedStop;
              }
            }
            if (_stopT < minStopT) {
              var _extra4 = minStopT - _stopT;
              tissues = zhlLoadConst(tissues, cur, _extra4, stopFO2, stopFHe, onLoop, 'deco');
              rt += _extra4;
              _stopT = minStopT;
            }
          }
          if (_stopT > 0) {
            _stopT = loadMdpStopExtension(cur, _stopT, transitDur, isFirstDecoStop, stopFO2, stopFHe, onLoop);
            var minStopDisplay = mdCompatMode && !isFirstDecoStop ? _stopT + transitDur : _stopT;
            steps.push({
              type: 'deco',
              depth: cur,
              dur: minStopDisplay,
              nativeDuration: _stopT,
              gas: gasLabel,
              pO2: zhlStepPpo2(cur, stopFN2, stopFHe, 'deco'),
              fN2: stopFN2,
              fHe: stopFHe,
              hitSafetyGuard: hitSafetyGuard || undefined,
              _tissues: tissues.map(function (t) {
                return {
                  pN2: t.pN2,
                  pHe: t.pHe
                };
              })
            });
          }
        } else if (cur === lastStop) {
          var isDecoNeeded = steps.some(function (s) {
            return s.type === 'deco';
          });
          var stopType = isDecoNeeded ? 'deco' : 'safety';
          var isFinalAscentPhase = _zhlPhaseIdx >= _zhlAscentFloors.length - 1;
          var lastClearGf = isFinalAscentPhase ? gfAt(0) : gfAt(floorStopMin);
          var lastCeilTarget = isFinalAscentPhase ? 0 : floorStopMin;
          var _stopT2 = 0;
          var transitToLastStop = 0;
          if (isDecoNeeded) {
            transitToLastStop = stopDepths.length > 1 ? (stopDepths[stopDepths.length - 2] - lastStop) / decoRate : 0;
            while (ceiling(tissues, lastClearGf) > lastCeilTarget + 0.01 && _stopT2 < CEILING_LOOP_GUARD_MIN) {
              tissues = zhlLoadConst(tissues, cur, minStopT, stopFO2, stopFHe, onLoop, 'deco');
              _stopT2 += minStopT;
              rt += minStopT;
            }
            if (_stopT2 >= CEILING_LOOP_GUARD_MIN && ceiling(tissues, lastClearGf) > lastCeilTarget + 0.01) hitSafetyGuard = true;
            var roundedLastStop = void 0;
            if (wholeMinStops) {
              var _grid2 = stopRoundGrid();
              var _totalAtLevel2 = Math.max(minStopT, Math.ceil((transitToLastStop + _stopT2 - 1e-9) / _grid2) * _grid2);
              roundedLastStop = _totalAtLevel2 - transitToLastStop;
            } else {
              roundedLastStop = _stopT2;
            }
            if (roundedLastStop > _stopT2) {
              var _extra5 = roundedLastStop - _stopT2;
              tissues = zhlLoadConst(tissues, cur, _extra5, stopFO2, stopFHe, onLoop, 'deco');
              _stopT2 += _extra5;
              rt += _extra5;
            }
            if (_stopT2 < minStopT) {
              var _extra6 = minStopT - _stopT2;
              tissues = zhlLoadConst(tissues, cur, _extra6, stopFO2, stopFHe, onLoop, 'deco');
              _stopT2 += _extra6;
              rt += _extra6;
            }
          } else {
            _stopT2 = Math.max(3, minStopT);
            tissues = zhlLoadConst(tissues, cur, _stopT2, stopFO2, stopFHe, onLoop, 'deco');
            rt += _stopT2;
          }
          _stopT2 = loadMdpStopExtension(cur, _stopT2, transitToLastStop, false, stopFO2, stopFHe, onLoop);
          var lastStopDisplay = mdCompatMode ? _stopT2 + transitToLastStop : _stopT2;
          steps.push({
            type: stopType,
            depth: cur,
            dur: lastStopDisplay,
            nativeDuration: _stopT2,
            gas: gasLabel,
            pO2: zhlStepPpo2(cur, stopFN2, stopFHe, 'deco'),
            fN2: stopFN2,
            fHe: stopFHe,
            hitSafetyGuard: hitSafetyGuard || undefined,
            _tissues: tissues.map(function (t) {
              return {
                pN2: t.pN2,
                pHe: t.pHe
              };
            })
          });
        } else if (_mdpEnabled && mdpMinRequiredAt(cur, _mdp9m, _mdp6m) > 0) {
          var minReq = mdpMinRequiredAt(cur, _mdp9m, _mdp6m);
          var _stopT3 = mdCompatMode ? Math.max(0, minReq - transitDur) : minReq;
          if (_stopT3 < 1 / 60) _stopT3 = 1 / 60;
          tissues = zhlLoadConst(tissues, cur, _stopT3, stopFO2, stopFHe, onLoop, 'deco');
          rt += _stopT3;
          var mdpDisplay = mdCompatMode ? _stopT3 + transitDur : _stopT3;
          steps.push({
            type: 'deco',
            depth: cur,
            dur: mdpDisplay,
            nativeDuration: _stopT3,
            gas: gasLabel,
            pO2: zhlStepPpo2(cur, stopFN2, stopFHe, 'deco'),
            fN2: stopFN2,
            fHe: stopFHe,
            hitSafetyGuard: hitSafetyGuard || undefined,
            _tissues: tissues.map(function (t) {
              return {
                pN2: t.pN2,
                pHe: t.pHe
              };
            })
          });
        }
        // No stop needed and not lastStop — continue ascending
        if (_zhlAscentFloor > 0 && cur <= _zhlAscentFloor && stopDepth <= _zhlAscentFloor) break;
      }
      if (_zhlAscentFloor > 0 && cur > _zhlAscentFloor) {
        var _travelRate = decoZoneEntered ? decoRate : rate;
        var _travelDur = (cur - _zhlAscentFloor) / _travelRate;
        var _travelGas = zhlGasAt(cur);
        var _travelOnLoop = zhlOnLoopAt();
        var _tFO = _travelOnLoop ? bottomFO2 : _travelGas.fO2 != null ? _travelGas.fO2 : Math.max(0, 1 - _travelGas.fN2 - (_travelGas.fHe || 0));
        var _tFHe = _travelOnLoop ? bottomFHe : _travelGas.fHe || 0;
        tissues = zhlLoadLinear(tissues, cur, _zhlAscentFloor, _travelDur, _tFO, _tFHe, _travelOnLoop, 'deco');
        steps.push({
          type: 'ascent',
          from: cur,
          to: _zhlAscentFloor,
          dur: _travelDur,
          gas: _travelOnLoop ? loopMixLabel : _travelGas.label,
          pO2: zhlStepPpo2(cur, _travelGas.fN2, _travelGas.fHe || 0, 'deco'),
          fN2: _travelGas.fN2,
          fHe: _travelGas.fHe || 0
        });
        rt += _travelDur;
        cur = _zhlAscentFloor;
      } else if (_zhlAscentFloor === 0 && cur > 0) {
        var finalAscentDur = cur / surfaceRate;
        var finalGas = zhlGasAt(cur);
        var finalOnLoop = zhlOnLoopAt();
        var fFO2 = finalOnLoop ? bottomFO2 : finalGas.fO2 != null ? finalGas.fO2 : Math.max(0, 1 - finalGas.fN2 - (finalGas.fHe || 0));
        var fFHe = finalOnLoop ? bottomFHe : finalGas.fHe || 0;
        tissues = zhlLoadLinear(tissues, cur, 0, finalAscentDur, fFO2, fFHe, finalOnLoop, 'deco');
        steps.push({
          type: 'ascent',
          from: cur,
          to: 0,
          dur: finalAscentDur,
          gas: finalOnLoop ? loopMixLabel : finalGas.label,
          pO2: zhlStepPpo2(cur, finalGas.fN2, finalGas.fHe || 0, 'deco'),
          fN2: finalGas.fN2,
          fHe: finalGas.fHe || 0
        });
        rt += finalAscentDur;
        cur = 0;
      }
      if (_zhlPhaseIdx < _zhlContLevels.length) {
        var cont = _zhlContLevels[_zhlPhaseIdx];
        if (cont.depth > cur) {
          throw new Error('continuationLevel must be shallower than current depth');
        }
        cur = cont.depth;
        var cO2 = cont.o2 / 100;
        var cHe = (cont.he || 0) / 100;
        var cN2 = Math.max(0, 1 - cO2 - cHe);
        tissues = zhlLoadConst(tissues, cur, cont.time, cO2, cHe, _zhlOnLoop, 'bottom');
        rt += cont.time;
        steps.push({
          type: 'bottom',
          depth: cur,
          dur: cont.time,
          gas: getGasLabel(cO2, cHe),
          pO2: (+ppO2Check(cur, cN2, cHe)).toFixed(2),
          fN2: cN2,
          fHe: cHe
        });
        _zhlActiveGas = {
          fN2: cN2,
          fHe: cHe,
          fO2: cO2,
          label: getGasLabel(cO2, cHe)
        };
      }
    };
    for (var _zhlPhaseIdx = 0; _zhlPhaseIdx < _zhlAscentFloors.length; _zhlPhaseIdx++) {
      _loop3();
    } // end multi-level ascent phase

    // Collapse consecutive ascent steps with the same gas into single rows
    var collapsed = [];
    for (var _i4 = 0, _steps = steps; _i4 < _steps.length; _i4++) {
      var s = _steps[_i4];
      var prev = collapsed[collapsed.length - 1];
      if (s.type === 'ascent' && prev && prev.type === 'ascent' && prev.gas === s.gas) {
        prev.to = s.to;
        prev.dur += s.dur;
        prev.pO2 = s.pO2; // keep last ppO2
        prev.decoTransit = !!(prev.decoTransit || s.decoTransit);
      } else {
        collapsed.push(_objectSpread({}, s));
      }
    }
    var collapsedMDP = collapsed;
    var decoStops = collapsedMDP.filter(function (s) {
      return s.type === 'deco';
    });
    var decoTime = Math.round(decoStops.reduce(function (a, s) {
      return a + s.dur;
    }, 0) * 60) / 60;
    var hasDeco = decoStops.length > 0;
    var gasUsed = _toConsumableArray(new Set(collapsedMDP.map(function (s) {
      return s.gas;
    })));

    // ── Headless hook: store plan for Node testing ──
    var runTimeMin = Math.round(rt);
    // TTS (time-to-surface): ascent+deco only. rt was initialized to bt (the full
    // descent-inclusive bottom-time input) and only grows via ascent/deco `rt +=`
    // additions in the loop above, so `rt - bt` is exactly the ascent+deco portion —
    // matches MultiDeco/DiveKit's published TTS definition. Computed here (before
    // the headless early-return below) so it's available in both headless tests
    // and the live DOM-rendered footer.
    var ttsMin = Math.max(0, rt - bt);
    _neoNativeTrace = {
      runtimeMinutes: rt,
      steps: steps.map(function (step) {
        return _objectSpread({}, step);
      }),
      tissuesAtBottom: tissuesAtBottom.map(function (t) {
        return _objectSpread({}, t);
      }),
      finalTissues: tissues.map(function (t) {
        return _objectSpread({}, t);
      }),
      hitSafetyGuard: !!hitSafetyGuard,
      bottomRuntimeMinutes: bt,
      firstStopDepth: firstStopDepth || 0
    };
    var lastPlan = {
      rt: runTimeMin,
      tts: Math.round(ttsMin * 10) / 10,
      decoTime: Math.round(decoTime),
      stops: decoStops.map(function (s) {
        return {
          depth: s.depth,
          dur: s.dur,
          gas: s.gas
        };
      }),
      steps: collapsedMDP,
      decoZoneStart: trueDecoZoneStart,
      firstStopDepth: firstStopDepth || 0,
      finalTissues: tissues.map(function (t) {
        return {
          pN2: t.pN2,
          pHe: t.pHe || 0
        };
      }),
      // for ZHL repetitive dive carry
      surfaceGF: computeSurfaceGF(tissues),
      hitSafetyGuard: hitSafetyGuard || undefined
    };
    return {
      hitSafetyGuard: hitSafetyGuard,
      lastPlan: lastPlan,
      collapsed: collapsed,
      collapsedMDP: collapsedMDP,
      tissuesAtBottom: tissuesAtBottom,
      decoStops: decoStops,
      decoTime: decoTime,
      hasDeco: hasDeco,
      gasUsed: gasUsed,
      descentTime: descentTime,
      trueDecoZoneStart: trueDecoZoneStart,
      firstStopDepth: firstStopDepth,
      gfAt: gfAt,
      depthM: depthM,
      bt: bt,
      rate: rate,
      decoRate: decoRate,
      surfaceRate: surfaceRate,
      descentRate: descentRate,
      bottomFN2: bottomFN2,
      bottomFHe: bottomFHe,
      bottomFO2: bottomFO2,
      bottomMixLabel: bottomMixLabel,
      rawD: params.rawD,
      dU: params.metric
    };
  }
  var _scheduleCoreGetGasLabel = getGasLabel;
  function zhlOptimalSwitchDepth(fO2, ctx) {
    var ppo2High = ctx.ppo2Deco;
    var ppo2Mid = 1.5;
    var ppo2Low = ctx.ppo2Bottom;
    function getPPO2Limit(fo2) {
      var pct = fo2 * 100;
      if (pct >= 45) return ppo2High;
      if (pct >= 28) return ppo2Mid;
      return ppo2Low;
    }
    if (fO2 <= 0) return 0;
    if (fO2 >= 0.995) return Math.max(ctx.lastStop, 6);
    var limit = getPPO2Limit(fO2);
    var exactMOD = (limit / fO2 - altSurfaceP) / BAR_PER_METRE;
    var snapped = Math.floor(exactMOD / ctx.decoStep) * ctx.decoStep;
    return Math.max(ctx.lastStop, Math.max(0, snapped));
  }
  function buildZhlDecoGasesFromEngine(decoGases, ctx) {
    var list = (decoGases || []).map(function (g) {
      var o2pct = g.o2;
      var hePct = g.he || 0;
      var fO2 = o2pct / 100;
      var fHe = hePct / 100;
      var fN2 = Math.max(0, 1 - fO2 - fHe);
      var label = getGasLabel(fO2, fHe);
      var depth = zhlOptimalSwitchDepth(fO2, ctx);
      return {
        depth: depth,
        fN2: fN2,
        fHe: fHe,
        fO2: fO2,
        label: label
      };
    });
    list.sort(function (a, b) {
      return b.depth - a.depth;
    });
    return list;
  }
  function splitZhlProfileLevels(levels) {
    if (!levels || levels.length <= 1) return {
      primary: levels || [],
      continuation: []
    };
    var deepest = 0;
    for (var i = 1; i < levels.length; i++) {
      if (levels[i].depth > levels[deepest].depth) deepest = i;
    }
    var continuation = [];
    var primary = levels;
    if (deepest < levels.length - 1) {
      var monotonic = true;
      for (var _i5 = deepest + 1; _i5 < levels.length; _i5++) {
        if (levels[_i5].depth > levels[_i5 - 1].depth) {
          monotonic = false;
          break;
        }
      }
      if (monotonic) {
        continuation = levels.slice(deepest + 1);
        primary = levels.slice(0, deepest + 1);
      }
    }
    return {
      primary: primary,
      continuation: continuation
    };
  }
  function buildZhlScheduleParamsFromEngine(levels, decoGases, settings, profileSplit, environment) {
    var s = settings || {};
    var level = levels[0];
    var metric = s.metric !== false;
    var fO2bot = level.o2 / 100;
    var fHeBot = (level.he || 0) / 100;
    var fN2bot = Math.max(0, 1 - fO2bot - fHeBot);
    var switchCtx = {
      ppo2Bottom: s.ppO2Bottom || 1.4,
      ppo2Deco: s.ppO2Deco || 1.6,
      lastStop: s.lastStop || 3,
      decoStep: s.stepSize || 3,
      metric: metric
    };
    var gases = buildZhlDecoGasesFromEngine(decoGases, switchCtx);
    return {
      depthM: level.depth,
      bt: level.time,
      rawD: metric ? level.depth : Math.round(level.depth * 3.28084),
      metric: metric,
      ascentRate: Math.max(1, s.ascentRate || 10),
      decoAscentRate: Math.max(1, s.decoAscentRate || 3),
      surfaceAscentRate: Math.max(1, s.surfaceAscentRate || s.decoAscentRate || 3),
      descentRate: Math.max(1, s.descentRate || 20),
      gfL: (s.gfLo || s.gfLow || 30) / 100,
      gfH: (s.gfHi || s.gfHigh || 85) / 100,
      ppo2Bottom: switchCtx.ppo2Bottom,
      ppo2Deco: switchCtx.ppo2Deco,
      minStopTime: s.minStopTime || 1,
      wholeMinStops: s.wholeMinStops !== false,
      switchPauseT: 0,
      mdCompatMode: s.mdCompatMode !== false,
      lastStop: switchCtx.lastStop,
      decoStep: switchCtx.decoStep,
      shallowGradient: !!s.shallowGradient,
      bottomFN2: fN2bot,
      bottomFHe: fHeBot,
      bottomFO2: fO2bot,
      bottomMixLabel: getGasLabel(fO2bot, fHeBot),
      travelInfo: s.travelInfo || null,
      repState: s._preTissues && s._preTissues.length ? {
        tissues: s._preTissues,
        surfaceIntervalMin: s._surfaceInterval || 0,
        surfaceP: (environment || defaultEnvironment()).altSurfaceP
      } : null,
      continuationLevels: profileSplit && profileSplit.continuation || [],
      minDecoProfile: s.minDecoProfile || {
        enabled: false,
        m9: 1,
        m6: 3,
        isMetric: metric !== false
      },
      decoGases: gases,
      environment: environment || defaultEnvironment(),
      ccr: {
        circuit: s.circuit || 'OC',
        setpoint: s.setpoint,
        descentSetpoint: s.descentSetpoint,
        bottomSetpoint: s.bottomSetpoint,
        decoSetpoint: s.decoSetpoint != null ? s.decoSetpoint : s.setpoint,
        bailout: !!s.bailout,
        bailoutGfLow: s.bailoutGfLow,
        bailoutGfHigh: s.bailoutGfHigh,
        scrLoopVolume: s.scrLoopVolume,
        scrMetabolicO2: s.scrMetabolicO2,
        sacStress: s.sacStress,
        sacDecoCcr: s.sacDecoCcr,
        stressTimeMin: s.stressTimeMin,
        problemSolveMin: s.problemSolveMin
      },
      onLoop: isRebreatherCircuit(s.circuit || 'OC') && !s.bailout
    };
  }
  function addHeadlessExposure(hCNSfrac, hOTU, ppO2, dur) {
    if (ppO2 > 0.5 && dur > 0) {
      hOTU.v += dur * Math.pow((ppO2 - 0.5) / 0.5, OTU_EXPONENT);
      if (ppO2 >= 1.6) {
        hCNSfrac.v += dur / 45;
        return;
      }
      var lims = {
        6: 720,
        7: 570,
        8: 450,
        9: 360,
        10: 300,
        11: 240,
        12: 210,
        13: 180,
        14: 150,
        15: 120,
        16: 45
      };
      var lo = Math.floor(ppO2 * 10),
        hi = lo + 1;
      var lim = (lims[lo] || 0) + ((lims[hi] || 0) - (lims[lo] || 0)) * (ppO2 * 10 - lo);
      var safeLim = lim > 0 ? lim : 45;
      hCNSfrac.v += dur / safeLim;
    }
  }
  function headlessSegPpo2(depthM, fO2, fHe, s) {
    var pAmb = altSurfaceP + depthM * BAR_PER_METRE;
    var cfg = normalizeCCRSettings(s);
    if (!isRebreatherCircuit(cfg.circuit) || cfg.bailout) return fO2 * pAmb;
    if (cfg.circuit === 'pSCR') {
      var fr = computePSCRFractions(pAmb, fO2, fHe, cfg);
      return fr.fO2 * pAmb;
    }
    var sp = getEffectiveSetpointAtDepth(depthM, cfg, altSurfaceP);
    return Math.min(pAmb, Math.max(sp, fO2 * pAmb));
  }
  function computeHeadlessCnsOtu(lp, level, s) {
    if (!lp || lp.totalCNS != null) return;
    var fO2bot = level.o2 / 100;
    var fHebot = (level.he || 0) / 100;
    var onLoop = isRebreatherCircuit(s.circuit || 'OC') && !s.bailout;
    var hCNSfrac = {
      v: 0
    };
    var hOTU = {
      v: 0
    };
    var hDescentRate = s.descentRate || 20;
    var hDescentTime = level.depth / hDescentRate;
    var ppO2DescMid = onLoop ? headlessSegPpo2(level.depth / 2, fO2bot, fHebot, s) : (altSurfaceP + level.depth / 2 * BAR_PER_METRE) * fO2bot;
    var ppO2Bottom = onLoop ? headlessSegPpo2(level.depth, fO2bot, fHebot, s) : (altSurfaceP + level.depth * BAR_PER_METRE) * fO2bot;
    addHeadlessExposure(hCNSfrac, hOTU, ppO2DescMid, hDescentTime);
    var btAtDepthMin = Math.max(0, level.time - hDescentTime);
    addHeadlessExposure(hCNSfrac, hOTU, ppO2Bottom, btAtDepthMin);
    (lp.steps || []).forEach(function (seg) {
      if (seg.decoTransit) return; // mdCompatMode: transit folded into stop display
      var d = seg.depth != null ? seg.depth : seg.type === 'ascent' ? (seg.from + seg.to) / 2 : 0;
      var fHeS = seg.fHe !== undefined ? seg.fHe : fHebot;
      var fO2s = seg.fN2 !== undefined ? Math.max(0, 1 - seg.fN2 - fHeS) : fO2bot;
      var ppO2 = onLoop ? headlessSegPpo2(d, fO2s, fHeS, s) : fO2s * (altSurfaceP + d * BAR_PER_METRE);
      addHeadlessExposure(hCNSfrac, hOTU, ppO2, seg.dur || 0);
    });
    lp.totalCNS = parseFloat((hCNSfrac.v * 100).toFixed(1));
    lp.totalOTU = Math.round(hOTU.v);
  }
  function mapToEngineReturn(lp, level, s, isMetric) {
    var fO2bot = level.o2 / 100;
    var stops = (lp.stops || []).map(function (st) {
      return {
        depth: st.depth,
        time: st.dur,
        gas: st.gas,
        type: 'stop'
      };
    });
    var plan = (lp.steps || []).filter(function (st) {
      return !(st.type === 'ascent' && st.decoTransit);
    }).map(function (st) {
      return {
        type: st.type === 'deco' ? 'stop' : st.type === 'safety' ? 'stop' : st.type,
        depth: st.type === 'ascent' ? st.to : st.depth,
        time: st.dur,
        run: null,
        gas: st.gas,
        o2: Math.round((st.fN2 !== undefined ? 1 - st.fN2 - (st.fHe || 0) : fO2bot) * 100),
        he: Math.round((st.fHe || 0) * 100)
      };
    });
    if (plan.length === 0 || plan[0].type !== 'descent') {
      var descentTime = level.depth / (s.descentRate || 20);
      var btAtDepthMin = Math.max(0, level.time - descentTime);
      var bottomGasLabel = getGasLabel(fO2bot, (level.he || 0) / 100);
      plan.unshift({
        type: 'bottom',
        depth: level.depth,
        time: btAtDepthMin,
        run: descentTime + btAtDepthMin,
        gas: bottomGasLabel,
        o2: level.o2,
        he: level.he || 0
      });
      plan.unshift({
        type: 'descent',
        depth: level.depth,
        time: descentTime,
        run: descentTime,
        gas: bottomGasLabel,
        o2: level.o2,
        he: level.he || 0
      });
    }
    var runAccum = 0;
    plan.forEach(function (seg) {
      if (seg.run == null) {
        runAccum += seg.time || 0;
        seg.run = Math.round(runAccum * 10) / 10;
      } else {
        runAccum = seg.run;
      }
    });
    return {
      plan: plan,
      stops: stops,
      totalRuntime: lp.rt || 0,
      tts: lp.tts || 0,
      totalOTU: lp.totalOTU || 0,
      totalCNS: lp.totalCNS || 0,
      finalTissues: lp.finalTissues || null,
      depthUnit: isMetric ? 'm' : 'ft',
      error: null
    };
  }
  function calculate(levels, decoGases, settings, profileSplit, environment) {
    applyEnvironment(environment || defaultEnvironment());
    var s = settings || {};
    var isMetric = s.metric !== false;
    var level = levels[0];
    var params = buildZhlScheduleParamsFromEngine(levels, decoGases, s, profileSplit, environment);
    var coreResult;
    try {
      coreResult = runZhlScheduleCore(params);
    } catch (e) {
      return {
        error: e.message,
        stops: [],
        plan: [],
        totalRuntime: 0
      };
    }
    var lp = coreResult.lastPlan;
    if (!lp) return {
      error: 'No plan generated',
      stops: [],
      plan: [],
      totalRuntime: 0
    };
    computeHeadlessCnsOtu(lp, level, s);
    return mapToEngineReturn(lp, level, s, isMetric);
  }
  var api = {
    neoNativeTrace: function neoNativeTrace() {
      return _neoNativeTrace;
    },
    runZhlScheduleCore: runZhlScheduleCore,
    buildZhlScheduleParamsFromEngine: buildZhlScheduleParamsFromEngine,
    splitZhlProfileLevels: splitZhlProfileLevels,
    zhlOptimalSwitchDepth: zhlOptimalSwitchDepth,
    calculate: calculate,
    defaultEnvironment: defaultEnvironment,
    applyEnvironment: applyEnvironment,
    setHeHalfTimeMode: setHeHalfTimeMode,
    OTU_EXPONENT: OTU_EXPONENT,
    PSCR_MIN_PPO2: PSCR_MIN_PPO2,
    PSCR_LOOP_VOLUME_MIN: PSCR_LOOP_VOLUME_MIN,
    PSCR_LOOP_VOLUME_MAX: PSCR_LOOP_VOLUME_MAX,
    PSCR_METABOLIC_O2_MIN: PSCR_METABOLIC_O2_MIN,
    PSCR_METABOLIC_O2_MAX: PSCR_METABOLIC_O2_MAX,
    ZHL16C: ZHL16C,
    ZHL16C_HE_HT: ZHL16C_HE_HT,
    ZHL16C_HE_HT_BAKER: ZHL16C_HE_HT_BAKER,
    ZHL16C_HE_HT_BUHL2003: ZHL16C_HE_HT_BUHL2003,
    ZHL16C_HE_AB: ZHL16C_HE_AB,
    initTissues: initTissues,
    depthBar: depthBar,
    schreiner: schreiner,
    schreinerLinear: schreinerLinear,
    saturateLinear: saturateLinear,
    saturate: saturate,
    ceiling: ceiling,
    computeSurfaceGF: computeSurfaceGF,
    ambientCrossingDepth: ambientCrossingDepth,
    gfAtDepth: gfAtDepth,
    ndlClearAtDepth: ndlClearAtDepth,
    buhNDL: buhNDL,
    getActiveGas: getActiveGas,
    enforceMinDecoProfile: enforceMinDecoProfile,
    ppO2Check: ppO2Check,
    n2FracFromCustomO2: n2FracFromCustomO2,
    n2FracFromPercentages: n2FracFromPercentages,
    validateHypoxicDecoGas: validateHypoxicDecoGas,
    canonicalCircuit: canonicalCircuit,
    normalizeCCRSettings: normalizeCCRSettings,
    isRebreatherCircuit: isRebreatherCircuit,
    loopMixLabelForCore: loopMixLabelForCore,
    depthAtSetpointCrossing: depthAtSetpointCrossing,
    getEffectiveSetpointAtDepth: getEffectiveSetpointAtDepth,
    getCcrMetabolicO2Rate: getCcrMetabolicO2Rate,
    computePSCRFractions: computePSCRFractions,
    ccrLoopGasBelowSetpoint: ccrLoopGasBelowSetpoint,
    getInspiredInertPressures: getInspiredInertPressures,
    getCCRInertSchreinerParams: getCCRInertSchreinerParams,
    getSetpointBoundaryDepths: getSetpointBoundaryDepths,
    splitLinearDepthAtBoundaries: splitLinearDepthAtBoundaries,
    splitSegmentAtSetpoint: splitSegmentAtSetpoint,
    schreinerLinearCCR: schreinerLinearCCR,
    saturateLinearCCR: saturateLinearCCR,
    saturateCCR: saturateCCR,
    loadTissuesWithCCR: loadTissuesWithCCR,
    getEffectivePpo2: getEffectivePpo2,
    getGasLabel: _scheduleCoreGetGasLabel
  };
  global.ZhlEngineBundle = api;
  if (typeof module !== 'undefined' && module.exports) module.exports = api;
})(typeof self !== 'undefined' ? self : globalThis);

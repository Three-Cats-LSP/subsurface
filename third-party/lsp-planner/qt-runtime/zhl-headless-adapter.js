// Generated from LSP+ 05fef18bade6354cebcc60df01f83e0ac4cc3b27. See SOURCE.json.
/**
 * ZHL headless adapter.
 * Browser-facing test/API wrapper around zhl-engine-bundle.js and zhl-worker-bridge.js.
 * Loaded after schedule-runner-core and before planner-shell.
 */

'use strict';

function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
function _regenerator() { /*! regenerator-runtime -- Copyright (c) 2014-present, Facebook, Inc. -- license (MIT): https://github.com/babel/babel/blob/main/packages/babel-helpers/LICENSE */ var e, t, r = "function" == typeof Symbol ? Symbol : {}, n = r.iterator || "@@iterator", o = r.toStringTag || "@@toStringTag"; function i(r, n, o, i) { var c = n && n.prototype instanceof Generator ? n : Generator, u = Object.create(c.prototype); return _regeneratorDefine2(u, "_invoke", function (r, n, o) { var i, c, u, f = 0, p = o || [], y = !1, G = { p: 0, n: 0, v: e, a: d, f: d.bind(e, 4), d: function d(t, r) { return i = t, c = 0, u = e, G.n = r, a; } }; function d(r, n) { for (c = r, u = n, t = 0; !y && f && !o && t < p.length; t++) { var o, i = p[t], d = G.p, l = i[2]; r > 3 ? (o = l === n) && (u = i[(c = i[4]) ? 5 : (c = 3, 3)], i[4] = i[5] = e) : i[0] <= d && ((o = r < 2 && d < i[1]) ? (c = 0, G.v = n, G.n = i[1]) : d < l && (o = r < 3 || i[0] > n || n > l) && (i[4] = r, i[5] = n, G.n = l, c = 0)); } if (o || r > 1) return a; throw y = !0, n; } return function (o, p, l) { if (f > 1) throw TypeError("Generator is already running"); for (y && 1 === p && d(p, l), c = p, u = l; (t = c < 2 ? e : u) || !y;) { i || (c ? c < 3 ? (c > 1 && (G.n = -1), d(c, u)) : G.n = u : G.v = u); try { if (f = 2, i) { if (c || (o = "next"), t = i[o]) { if (!(t = t.call(i, u))) throw TypeError("iterator result is not an object"); if (!t.done) return t; u = t.value, c < 2 && (c = 0); } else 1 === c && (t = i.return) && t.call(i), c < 2 && (u = TypeError("The iterator does not provide a '" + o + "' method"), c = 1); i = e; } else if ((t = (y = G.n < 0) ? u : r.call(n, G)) !== a) break; } catch (t) { i = e, c = 1, u = t; } finally { f = 1; } } return { value: t, done: y }; }; }(r, o, i), !0), u; } var a = {}; function Generator() {} function GeneratorFunction() {} function GeneratorFunctionPrototype() {} t = Object.getPrototypeOf; var c = [][n] ? t(t([][n]())) : (_regeneratorDefine2(t = {}, n, function () { return this; }), t), u = GeneratorFunctionPrototype.prototype = Generator.prototype = Object.create(c); function f(e) { return Object.setPrototypeOf ? Object.setPrototypeOf(e, GeneratorFunctionPrototype) : (e.__proto__ = GeneratorFunctionPrototype, _regeneratorDefine2(e, o, "GeneratorFunction")), e.prototype = Object.create(u), e; } return GeneratorFunction.prototype = GeneratorFunctionPrototype, _regeneratorDefine2(u, "constructor", GeneratorFunctionPrototype), _regeneratorDefine2(GeneratorFunctionPrototype, "constructor", GeneratorFunction), GeneratorFunction.displayName = "GeneratorFunction", _regeneratorDefine2(GeneratorFunctionPrototype, o, "GeneratorFunction"), _regeneratorDefine2(u), _regeneratorDefine2(u, o, "Generator"), _regeneratorDefine2(u, n, function () { return this; }), _regeneratorDefine2(u, "toString", function () { return "[object Generator]"; }), (_regenerator = function _regenerator() { return { w: i, m: f }; })(); }
function _regeneratorDefine2(e, r, n, t) { var i = Object.defineProperty; try { i({}, "", {}); } catch (e) { i = 0; } _regeneratorDefine2 = function _regeneratorDefine(e, r, n, t) { function o(r, n) { _regeneratorDefine2(e, r, function (e) { return this._invoke(r, n, e); }); } r ? i ? i(e, r, { value: n, enumerable: !t, configurable: !t, writable: !t }) : e[r] = n : (o("next", 0), o("throw", 1), o("return", 2)); }, _regeneratorDefine2(e, r, n, t); }
function asyncGeneratorStep(n, t, e, r, o, a, c) { try { var i = n[a](c), u = i.value; } catch (n) { return void e(n); } i.done ? t(u) : Promise.resolve(u).then(r, o); }
function _asyncToGenerator(n) { return function () { var t = this, e = arguments; return new Promise(function (r, o) { var a = n.apply(t, e); function _next(n) { asyncGeneratorStep(a, r, o, _next, _throw, "next", n); } function _throw(n) { asyncGeneratorStep(a, r, o, _next, _throw, "throw", n); } _next(void 0); }); }; }
function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), !0).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: !0, configurable: !0, writable: !0 }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == _typeof(i) ? i : i + ""; }
function _toPrimitive(t, e) { if ("object" != _typeof(t) || !t) return t; var r; if ("undefined" != typeof Symbol && void 0 !== (r = t[Symbol.toPrimitive])) { var i = r.call(t, e || "default"); if ("object" != _typeof(i)) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === e ? String : Number)(t); }
function _slicedToArray(r, e) { return _arrayWithHoles(r) || _iterableToArrayLimit(r, e) || _unsupportedIterableToArray(r, e) || _nonIterableRest(); }
function _nonIterableRest() { throw new TypeError("Invalid attempt to destructure non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArrayLimit(r, l) { var t = null == r ? null : "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (null != t) { var e, n, i, u, a = [], f = !0, o = !1; try { if (i = (t = t.call(r)).next, 0 === l) { if (Object(t) !== t) return; f = !1; } else for (; !(f = (e = i.call(t)).done) && (a.push(e.value), a.length !== l); f = !0); } catch (r) { o = !0, n = r; } finally { try { if (!f && null != t.return && (u = t.return(), Object(u) !== u)) return; } finally { if (o) throw n; } } return a; } }
function _arrayWithHoles(r) { if (Array.isArray(r)) return r; }
function _toConsumableArray(r) { return _arrayWithoutHoles(r) || _iterableToArray(r) || _unsupportedIterableToArray(r) || _nonIterableSpread(); }
function _nonIterableSpread() { throw new TypeError("Invalid attempt to spread non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); }
function _iterableToArray(r) { if ("undefined" != typeof Symbol && null != r[Symbol.iterator] || null != r["@@iterator"]) return Array.from(r); }
function _arrayWithoutHoles(r) { if (Array.isArray(r)) return _arrayLikeToArray(r); }
function _createForOfIteratorHelper(r, e) { var t = "undefined" != typeof Symbol && r[Symbol.iterator] || r["@@iterator"]; if (!t) { if (Array.isArray(r) || (t = _unsupportedIterableToArray(r)) || e && r && "number" == typeof r.length) { t && (r = t); var _n = 0, F = function F() {}; return { s: F, n: function n() { return _n >= r.length ? { done: !0 } : { done: !1, value: r[_n++] }; }, e: function e(r) { throw r; }, f: F }; } throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method."); } var o, a = !0, u = !1; return { s: function s() { t = t.call(r); }, n: function n() { var r = t.next(); return a = r.done, r; }, e: function e(r) { u = !0, o = r; }, f: function f() { try { a || null == t.return || t.return(); } finally { if (u) throw o; } } }; }
function _unsupportedIterableToArray(r, a) { if (r) { if ("string" == typeof r) return _arrayLikeToArray(r, a); var t = {}.toString.call(r).slice(8, -1); return "Object" === t && r.constructor && (t = r.constructor.name), "Map" === t || "Set" === t ? Array.from(r) : "Arguments" === t || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(t) ? _arrayLikeToArray(r, a) : void 0; } }
function _arrayLikeToArray(r, a) { (null == a || a > r.length) && (a = r.length); for (var e = 0, n = Array(a); e < a; e++) n[e] = r[e]; return n; }
function validateGasFractionsPct(o2, he, field) {
  var o = Number(o2);
  var h = he == null || he === '' ? 0 : Number(he);
  if (!Number.isFinite(o) || !Number.isFinite(h) || o <= 0 || o > 100 || h < 0 || h > 100 || o + h > 100) {
    return {
      ok: false,
      code: 'INVALID_GAS_FRACTIONS',
      field: field || 'gas',
      message: 'O2 must be greater than 0 and at most 100 percent; He must be between 0 and 100 percent; total no more than 100 percent.'
    };
  }
  return {
    ok: true,
    o2: o,
    he: h
  };
}

// AUDIT-UNIT:UI-ZHL-HEADLESS-HELPERS
function gasFractionsFromPct(o2, he) {
  var o = Number(o2);
  var h = he == null || he === '' ? 0 : Number(he);
  return {
    o2Frac: o / 100,
    heFrac: h / 100,
    o2Pct: o,
    hePct: h
  };
}
if (typeof window !== 'undefined') window.validateGasFractionsPct = validateGasFractionsPct;
function validateEngineInputs(levels, decoGases) {
  var errors = [];
  if (!Array.isArray(levels) || levels.length === 0) {
    errors.push({
      code: 'INVALID_PROFILE',
      field: 'levels',
      message: 'At least one dive level is required.'
    });
  } else {
    levels.forEach(function (level, index) {
      if (level == null) {
        errors.push({
          code: 'INVALID_PROFILE',
          field: "levels[".concat(index, "]"),
          message: 'Level entry is required.'
        });
        return;
      }
      if (!Number.isFinite(level.depth) || level.depth <= 0) {
        errors.push({
          code: 'INVALID_DEPTH',
          field: "levels[".concat(index, "].depth"),
          message: 'Depth must be a positive number.'
        });
      }
      if (!Number.isFinite(level.time) || level.time <= 0) {
        errors.push({
          code: 'INVALID_TIME',
          field: "levels[".concat(index, "].time"),
          message: 'Bottom time must be a positive number.'
        });
      }
      var gasCheck = validateGasFractionsPct(level.o2, level.he, "levels[".concat(index, "]"));
      if (!gasCheck.ok) errors.push(gasCheck);
    });
  }
  if (Array.isArray(decoGases)) {
    decoGases.forEach(function (g, index) {
      if (g == null) {
        errors.push({
          code: 'INVALID_GAS_FRACTIONS',
          field: "decoGases[".concat(index, "]"),
          message: 'Deco gas entry is required.'
        });
        return;
      }
      if (g.o2 == null && g.he == null) return;
      var gasCheck = validateGasFractionsPct(g.o2, g.he, "decoGases[".concat(index, "]"));
      if (!gasCheck.ok) errors.push(gasCheck);
    });
  }
  return {
    ok: errors.length === 0,
    errors: errors
  };
}
if (typeof window !== 'undefined') window.validateEngineInputs = validateEngineInputs;
function engineValidationError(validation) {
  var first = validation.errors[0];
  return {
    error: first.message || 'Invalid input.',
    code: first.code,
    field: first.field,
    errors: validation.errors,
    stops: [],
    plan: [],
    totalRuntime: 0,
    totalTime: 0
  };
}
function collectDecoGasesPctFromDom() {
  var out = [];
  var _iterator = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step;
  try {
    for (_iterator.s(); !(_step = _iterator.n()).done;) {
      var idx = _step.value;
      var gas = getDomDecoGasPct(idx);
      if (!gas) continue;
      out.push(gas);
    }
  } catch (err) {
    _iterator.e(err);
  } finally {
    _iterator.f();
  }
  return out;
}
function validatePSCRParameterBounds(settings) {
  var loopVol = parseFloat(settings.scrLoopVolume);
  var metO2 = parseFloat(settings.scrMetabolicO2);
  var errors = [];
  if (!Number.isFinite(loopVol) || loopVol < PSCR_LOOP_VOLUME_MIN || loopVol > PSCR_LOOP_VOLUME_MAX) {
    errors.push({
      code: 'INVALID_LOOP_VOLUME',
      field: 'scrLoopVolume',
      message: "pSCR loop volume must be between ".concat(PSCR_LOOP_VOLUME_MIN, " and ").concat(PSCR_LOOP_VOLUME_MAX, " litres.")
    });
  }
  if (!Number.isFinite(metO2) || metO2 < PSCR_METABOLIC_O2_MIN || metO2 > PSCR_METABOLIC_O2_MAX) {
    errors.push({
      code: 'INVALID_METABOLIC_O2',
      field: 'scrMetabolicO2',
      message: "pSCR metabolic O2 must be between ".concat(PSCR_METABOLIC_O2_MIN, " and ").concat(PSCR_METABOLIC_O2_MAX, " L/min.")
    });
  }
  return errors;
}
function validateCcrCalculationInputs(levels, settings, decoGases) {
  var errors = [];
  var s = settings || {};
  var circuit = canonicalCircuit(s.circuit || 'OC');
  if (!isRebreatherCircuit(circuit)) return {
    ok: true,
    errors: []
  };
  if (!Array.isArray(levels) || levels.length === 0) {
    errors.push({
      code: 'INVALID_PROFILE',
      message: 'At least one profile level is required.'
    });
  }
  for (var li = 0; li < (levels || []).length; li++) {
    var level = levels[li];
    var depth = Number(level.depth);
    var time = Number(level.time);
    if (!Number.isFinite(depth) || depth <= 0) {
      errors.push({
        code: 'INVALID_PROFILE',
        field: 'depth',
        message: 'Depth must be a positive finite number.'
      });
    }
    if (!Number.isFinite(time) || time <= 0) {
      errors.push({
        code: 'INVALID_PROFILE',
        field: 'time',
        message: 'Bottom time must be a positive finite number.'
      });
    }
    var gasCheck = validateGasFractionsPct(level.o2, level.he, "levels[".concat(li, "]"));
    if (!gasCheck.ok) {
      errors.push({
        code: gasCheck.code,
        field: gasCheck.field,
        message: gasCheck.message
      });
    }
  }
  if (Array.isArray(decoGases)) {
    decoGases.forEach(function (g, index) {
      if (g.o2 == null && g.he == null) return;
      var gasCheck = validateGasFractionsPct(g.o2, g.he, "decoGases[".concat(index, "]"));
      if (!gasCheck.ok) {
        errors.push({
          code: gasCheck.code,
          field: gasCheck.field,
          message: gasCheck.message
        });
      }
    });
  }
  var descentSP = s.descentSetpoint != null ? s.descentSetpoint : 0.7;
  var bottomSP = s.bottomSetpoint != null ? s.bottomSetpoint : s.setpoint != null ? s.setpoint : 1.2;
  var decoSP = s.decoSetpoint != null ? s.decoSetpoint : s.setpoint != null ? s.setpoint : 1.3;
  if (circuit === 'pSCR') {
    errors.push.apply(errors, _toConsumableArray(validatePSCRParameterBounds(s)));
    for (var _i = 0, _arr = [['bottom', bottomSP, 0.7, 1.6], ['deco', decoSP, 0.7, 1.6]]; _i < _arr.length; _i++) {
      var _arr$_i = _slicedToArray(_arr[_i], 4),
        name = _arr$_i[0],
        value = _arr$_i[1],
        min = _arr$_i[2],
        max = _arr$_i[3];
      if (value === 0) continue;
      if (!Number.isFinite(value) || value < min || value > max) {
        errors.push({
          code: 'INVALID_SETPOINT',
          field: name + 'Setpoint',
          message: "".concat(name, " setpoint must be between ").concat(min, " and ").concat(max, " bar.")
        });
      }
    }
    return {
      ok: errors.length === 0,
      errors: errors
    };
  }
  for (var _i2 = 0, _arr2 = [['descent', descentSP, 0.5, 1.0], ['bottom', bottomSP, 0.7, 1.6], ['deco', decoSP, 0.7, 1.6]]; _i2 < _arr2.length; _i2++) {
    var _arr2$_i = _slicedToArray(_arr2[_i2], 4),
      _name = _arr2$_i[0],
      _value = _arr2$_i[1],
      _min = _arr2$_i[2],
      _max = _arr2$_i[3];
    if (!Number.isFinite(_value) || _value < _min || _value > _max) {
      errors.push({
        code: 'INVALID_SETPOINT',
        field: _name + 'Setpoint',
        message: "".concat(_name, " setpoint must be between ").concat(_min, " and ").concat(_max, " bar.")
      });
    }
  }
  return {
    ok: errors.length === 0,
    errors: errors
  };
}
if (typeof window !== 'undefined') window.validateCcrCalculationInputs = validateCcrCalculationInputs;
function splitZhlProfileLevels(levels) {
  return ZhlEngineBundle.splitZhlProfileLevels(levels);
}
function validateZhlHeadlessProfile(levels) {
  if (!levels || levels.length <= 1) return {
    ok: true
  };
  var deepestIdx = 0;
  for (var i = 1; i < levels.length; i++) {
    if (levels[i].depth > levels[deepestIdx].depth) deepestIdx = i;
  }
  if (deepestIdx !== 0) {
    return {
      ok: false,
      code: 'INVALID_PROFILE',
      field: 'levels',
      message: 'ZHL profiles must start at the deepest level; add deeper segments before shallower ones.'
    };
  }
  for (var _i3 = 1; _i3 < levels.length; _i3++) {
    if (levels[_i3].depth > levels[_i3 - 1].depth) {
      return {
        ok: false,
        code: 'INVALID_PROFILE',
        field: "levels[".concat(_i3, "].depth"),
        message: 'ZHL profiles cannot re-descend after a shallower level.'
      };
    }
  }
  return {
    ok: true
  };
}
if (typeof window !== 'undefined') window.validateZhlHeadlessProfile = validateZhlHeadlessProfile;

// AUDIT-UNIT:UI-ZHL-HEADLESS-ENGINE
var ZHLEngine = function () {
  function mergeRepSettings(settings) {
    var s = settings || {};
    if (s._preTissues && s._preTissues.length) {
      var _repSnap = peekZhlRepState();
      if (_repSnap && s.totalCNSCarry == null && _repSnap.totalCNS != null) {
        return _objectSpread(_objectSpread({}, s), {}, {
          totalCNSCarry: _repSnap.totalCNS,
          totalOTUCarry: _repSnap.totalOTU
        });
      }
      return s;
    }
    var repSnap = peekZhlRepState();
    if (repSnap) {
      return _objectSpread(_objectSpread({}, s), {}, {
        _preTissues: repSnap.tissues,
        _surfaceInterval: repSnap.surfaceIntervalMin || 0,
        totalCNSCarry: repSnap.totalCNS,
        totalOTUCarry: repSnap.totalOTU
      });
    }
    return s;
  }
  function applyPlanExposureTotals(result, levels, settings) {
    if (!result || result.error || !levels || !levels.length) return result;
    var level = levels[0];
    var s = settings || {};
    var fO2bot = level.o2 / 100;
    var fHeBot = (level.he || 0) / 100;
    var exposure = computePlanExposureTotals(result.plan, s, fO2bot, fHeBot, altSurfaceP || 1.01325, BAR_PER_METRE || 0.1);
    var _pdCarry = window._priorDiveCarry;
    var otuCarry = _pdCarry ? _pdCarry.otuCarry || 0 : s.totalOTUCarry != null ? s.totalOTUCarry : 0;
    var cnsCarry = _pdCarry ? _pdCarry.cnsCarry || 0 : s.totalCNSCarry != null ? s.totalCNSCarry : 0;
    return Object.assign({}, result, {
      totalOTU: Math.round(exposure.totalOTU + otuCarry),
      totalCNS: parseFloat((exposure.totalCNS + cnsCarry).toFixed(1))
    });
  }
  function calculate(levels, decoGases, settings) {
    _syncZhlBundleEnv();
    var s = mergeRepSettings(settings);
    var ccrVal = validateCcrCalculationInputs(levels, s, decoGases);
    if (!ccrVal.ok) return engineValidationError(ccrVal);
    var validation = validateEngineInputs(levels, decoGases);
    if (!validation.ok) return engineValidationError(validation);
    var profileVal = validateZhlHeadlessProfile(levels);
    if (!profileVal.ok) {
      return engineValidationError({
        ok: false,
        errors: [profileVal]
      });
    }
    var profileSplit = ZhlEngineBundle.splitZhlProfileLevels(levels);
    return applyPlanExposureTotals(ZhlEngineBundle.calculate(levels, decoGases, s, profileSplit, getZhlEnvironment(s)), levels, s);
  }
  function calculateInWorker(_x, _x2, _x3) {
    return _calculateInWorker.apply(this, arguments);
  }
  function _calculateInWorker() {
    _calculateInWorker = _asyncToGenerator(/*#__PURE__*/_regenerator().m(function _callee(levels, decoGases, settings) {
      var s, ccrVal, validation, profileVal, profileSplit, result, _t;
      return _regenerator().w(function (_context) {
        while (1) switch (_context.p = _context.n) {
          case 0:
            _syncZhlBundleEnv();
            s = mergeRepSettings(settings);
            ccrVal = validateCcrCalculationInputs(levels, s, decoGases);
            if (ccrVal.ok) {
              _context.n = 1;
              break;
            }
            return _context.a(2, engineValidationError(ccrVal));
          case 1:
            validation = validateEngineInputs(levels, decoGases);
            if (validation.ok) {
              _context.n = 2;
              break;
            }
            return _context.a(2, engineValidationError(validation));
          case 2:
            profileVal = validateZhlHeadlessProfile(levels);
            if (profileVal.ok) {
              _context.n = 3;
              break;
            }
            return _context.a(2, engineValidationError({
              ok: false,
              errors: [profileVal]
            }));
          case 3:
            profileSplit = ZhlEngineBundle.splitZhlProfileLevels(levels);
            _context.p = 4;
            _context.n = 5;
            return ZhlWorkerBridge.calculateInWorker(levels, decoGases, s, profileSplit, getZhlEnvironment(s));
          case 5:
            result = _context.v;
            return _context.a(2, applyPlanExposureTotals(result, levels, s));
          case 6:
            _context.p = 6;
            _t = _context.v;
            return _context.a(2, {
              error: _t.message,
              stops: [],
              plan: [],
              totalRuntime: 0
            });
        }
      }, _callee, null, [[4, 6]]);
    }));
    return _calculateInWorker.apply(this, arguments);
  }
  return {
    calculate: calculate,
    calculateInWorker: calculateInWorker,
    MODEL: 'ZHLC_GF'
  };
}();
if (typeof window !== 'undefined') window.ZHLEngine = ZHLEngine;

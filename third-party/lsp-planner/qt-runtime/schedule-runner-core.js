// Generated from LSP+ 05fef18bade6354cebcc60df01f83e0ac4cc3b27. See SOURCE.json.
/**
 * Schedule runner core — VPM/ZHL UI orchestration, visible schedule table contract,
 * and plan exposure totals. Loaded after results-render-core and before planner-shell.
 */
'use strict';

function ownKeys(e, r) { var t = Object.keys(e); if (Object.getOwnPropertySymbols) { var o = Object.getOwnPropertySymbols(e); r && (o = o.filter(function (r) { return Object.getOwnPropertyDescriptor(e, r).enumerable; })), t.push.apply(t, o); } return t; }
function _objectSpread(e) { for (var r = 1; r < arguments.length; r++) { var t = null != arguments[r] ? arguments[r] : {}; r % 2 ? ownKeys(Object(t), !0).forEach(function (r) { _defineProperty(e, r, t[r]); }) : Object.getOwnPropertyDescriptors ? Object.defineProperties(e, Object.getOwnPropertyDescriptors(t)) : ownKeys(Object(t)).forEach(function (r) { Object.defineProperty(e, r, Object.getOwnPropertyDescriptor(t, r)); }); } return e; }
function _defineProperty(e, r, t) { return (r = _toPropertyKey(r)) in e ? Object.defineProperty(e, r, { value: t, enumerable: !0, configurable: !0, writable: !0 }) : e[r] = t, e; }
function _toPropertyKey(t) { var i = _toPrimitive(t, "string"); return "symbol" == _typeof(i) ? i : i + ""; }
function _toPrimitive(t, e) { if ("object" != _typeof(t) || !t) return t; var r; if ("undefined" != typeof Symbol && void 0 !== (r = t[Symbol.toPrimitive])) { var i = r.call(t, e || "default"); if ("object" != _typeof(i)) return i; throw new TypeError("@@toPrimitive must return a primitive value."); } return ("string" === e ? String : Number)(t); }
function _typeof(o) { "@babel/helpers - typeof"; return _typeof = "function" == typeof Symbol && "symbol" == typeof Symbol.iterator ? function (o) { return typeof o; } : function (o) { return o && "function" == typeof Symbol && o.constructor === Symbol && o !== Symbol.prototype ? "symbol" : typeof o; }, _typeof(o); }
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
var SCHEDULE_TABLE_COLUMNS = Object.freeze(['Phase', 'Depth', 'Stop', 'Run', 'Mix', 'PPO2', 'CNS', 'EAD']);
function scheduleColumnCount() {
  return SCHEDULE_TABLE_COLUMNS.length;
}
function scheduleCell(label, value, attrs) {
  var attrText = attrs ? ' ' + String(attrs).trim() : '';
  var dataLabel = label && label !== 'Phase' ? " data-label=\"".concat(escHtmlAttr(label), "\"") : '';
  var safeValue = typeof escapeHtmlText === 'function' ? escapeHtmlText(value !== null && value !== void 0 ? value : '') : String(value !== null && value !== void 0 ? value : '').replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  return "<td".concat(dataLabel).concat(attrText, ">").concat(safeValue, "</td>");
}
function scheduleErrorRowHtml(message) {
  return "<tr data-phase=\"error\">".concat(scheduleCell('', message, "colspan=\"".concat(scheduleColumnCount(), "\" class=\"schedule-error-cell\" style=\"color:var(--red);padding:12px;\"")), "</tr>");
}
function renderScheduleErrorRow(message, tbodyId) {
  var tbody = document.getElementById(tbodyId || 'decoTableBody');
  if (tbody) tbody.innerHTML = scheduleErrorRowHtml(message);
  return !!tbody;
}
function fmtRowTTS(rowRunMin, rtAtBottomEnd, finalRunMin) {
  if (rowRunMin == null || isNaN(rowRunMin) || rowRunMin < rtAtBottomEnd - 0.001) return '—';
  return toMMSS(Math.max(0, finalRunMin - rowRunMin));
}
function parsePlanSummaryText(t) {
  var _;
  var s = (t || '').replace(/\s+/g, ' ');
  return {
    runTime: (s.match(/Run time:\s*([\d'":]+)/i) || [])[1] || '-',
    tts: (s.match(/TTS:\s*([\d'":]+)/i) || [])[1] || '-',
    decoTime: (s.match(/Deco time:\s*([\d'":]+)/i) || [])[1] || '-',
    cns: (s.match(/CNS:\s*([\d.]+%|-)/i) || [])[1] || '-',
    otu: (s.match(/OTU:\s*(\d+|-)/i) || [])[1] || '-',
    prt: (s.match(/PrT:\s*([\d.]+|-)/i) || [])[1] || '-',
    decozone: ((s.match(/Decozone:\s*([\d.]+ (?:m|ft)|—|-)/i) || s.match(/Deco zone:\s*([\d.]+ (?:m|ft)|—|-)/i) || [])[1] || '').trim() || '-',
    decoStop: ((_ = (s.match(/First deco:\s*([\d.]+ (?:m|ft)|—|-)/i) || [])[1]) === null || _ === void 0 ? void 0 : _.trim()) || '-'
  };
}
function getContingencySummaryExport() {
  var c = window._lastContingency;
  var totRow = document.querySelector('#contingencyResult .deco-table tbody tr[data-phase="totals"] td');
  if (totRow) {
    var p = totRow.dataset.run ? {
      runTime: normalizeMMSSDisplay(totRow.dataset.run) || '-',
      tts: normalizeMMSSDisplay(totRow.dataset.tts) || '-',
      decoTime: normalizeMMSSDisplay(totRow.dataset.deco) || '-',
      cns: totRow.dataset.cns || '-',
      otu: totRow.dataset.otu || '-',
      prt: totRow.dataset.prt || '-',
      decozone: totRow.dataset.decozone || '-',
      decoStop: totRow.dataset.decostop || '-',
      surfGF: totRow.dataset.surfgf || '-'
    } : parsePlanSummaryText(totRow.textContent);
    if (c) {
      if (p.runTime === '-') p.runTime = c.lastRunFmt || "".concat(c.lastRun, "'00\"");
      if (p.decoTime === '-') p.decoTime = c.decoTimeFmt || "".concat(c.decoTime, "'00\"");
      if (p.cns === '-') p.cns = c.totalCNS || '-';
      if (p.otu === '-') p.otu = c.totalOTU || '-';
      if (p.prt === '-') p.prt = c.totalPrT || '-';
      if (p.tts === '-') p.tts = c.tts || '-';
      if (p.decozone === '-') p.decozone = c.decozoneDisp || formatDecoZoneStart(c.decoZoneStart);
      if (p.decoStop === '-') p.decoStop = c.decoStop || '-';
      if (!p.surfGF || p.surfGF === '-') p.surfGF = c.surfGF || '-';
    }
    p.runTime = normalizeMMSSDisplay(p.runTime);
    p.tts = normalizeMMSSDisplay(p.tts);
    p.decoTime = normalizeMMSSDisplay(p.decoTime);
    return p;
  }
  if (!c) return getPlanSummaryExport();
  return {
    runTime: normalizeMMSSDisplay(c.lastRunFmt || "".concat(c.lastRun, "'00\"")),
    tts: normalizeMMSSDisplay(c.tts || '-'),
    decoTime: normalizeMMSSDisplay(c.decoTimeFmt || "".concat(c.decoTime, "'00\"")),
    cns: c.totalCNS || '-',
    otu: c.totalOTU || '-',
    prt: c.totalPrT || '-',
    decozone: c.decozoneDisp || formatDecoZoneStart(c.decoZoneStart),
    decoStop: c.decoStop || '-',
    surfGF: c.surfGF || '-'
  };
}
function getPlanSummaryExport(fromTotalsEl) {
  var el = fromTotalsEl || document.querySelector('#decoTableBody tr[data-phase="totals"] td');
  var lp = window._lastPlan || {};
  if (el) {
    // Read from data-* attributes (text content is empty to avoid inflating table width)
    var p = el.dataset.run ? {
      runTime: normalizeMMSSDisplay(el.dataset.run) || '-',
      tts: normalizeMMSSDisplay(el.dataset.tts) || '-',
      decoTime: normalizeMMSSDisplay(el.dataset.deco) || '-',
      cns: el.dataset.cns || '-',
      otu: el.dataset.otu || '-',
      prt: el.dataset.prt || '-',
      decozone: el.dataset.decozone || '-',
      decoStop: el.dataset.decostop || '-',
      surfGF: el.dataset.surfgf || '-'
    } : parsePlanSummaryText(el.textContent);
    p.runTime = normalizeMMSSDisplay(p.runTime);
    p.tts = normalizeMMSSDisplay(p.tts);
    p.decoTime = normalizeMMSSDisplay(p.decoTime);
    if (p.decozone === '-') p.decozone = formatDecoZoneStart(lp.decoZoneStart);
    if (p.decoStop === '-') p.decoStop = formatDecoStopDepth(lp.firstStopDepth);
    if (p.tts === '-' && lp.tts != null) p.tts = toMMSS(lp.tts);
    if (!p.surfGF || p.surfGF === '-') p.surfGF = lp.surfaceGF != null ? Math.round(lp.surfaceGF) + '%' : '-';
    return p;
  }
  return {
    runTime: '-',
    decoTime: '-',
    tts: lp.tts != null ? toMMSS(lp.tts) : '-',
    cns: '-',
    otu: '-',
    prt: '-',
    decozone: formatDecoZoneStart(lp.decoZoneStart),
    decoStop: formatDecoStopDepth(lp.firstStopDepth)
  };
}
function getDecoZoneExportVal(fromTotalsEl) {
  return getPlanSummaryExport(fromTotalsEl).decozone;
}
function _escHtmlPre(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

/** VPM deco gas switch depth (metric m or imperial ft per unit mode). */
function vpmDecoSwitchDepthVal(dgO2pct, ppO2Limit) {
  var dU = units === 'metric';
  var fO2 = dgO2pct / 100;
  if (fO2 >= 0.995) {
    var _modM = calcGasMODm(fO2, ppO2Limit || 1.6);
    return dU ? _modM : Math.round(_modM * 3.28084);
  }
  var modM = Math.max(0, Math.floor(((ppO2Limit || 1.6) / fO2 - altSurfaceP) / BAR_PER_METRE));
  return dU ? modM : Math.floor(modM * 3.28084);
}

/** Parse gas mix label for export/banner (50/00, 100%, Air, …). */
function shortMixLabel(m) {
  var s = (m || '').trim();
  if (!s) return '-';
  if (/^\d+\/\d+$/.test(s)) return s;
  if (s === '100%') return '100%';
  if (/^air$/i.test(s)) return 'Air';
  if (/^100/i.test(s)) return '100%';
  var ean = s.match(/[Ee][Aa][Nn]\s*(\d+)/);
  if (ean) return ean[1] + '/00';
  var pct = s.match(/(\d+)%/);
  if (pct) return pct[1] + '/00';
  return s;
}

/** Deco gas switches — reads computed table rows or falls back to gas cards. */
function getDecoGasSwitches() {
  var _document$getElementB, _document$getElementB2, _document$getElementB3, _document$getElementB4, _document$getElementB5;
  var gswRows = Array.from(document.querySelectorAll('#decoTableBody tr[data-phase="switch"]'));
  if (gswRows.length) {
    return gswRows.map(function (tr) {
      var _ref, _ref2;
      var depthTxt = ((_ref = tr.querySelector('td[data-label="Depth"]') || tr.querySelectorAll('td')[1]) === null || _ref === void 0 || (_ref = _ref.textContent) === null || _ref === void 0 ? void 0 : _ref.trim()) || '';
      var mixTxt = ((_ref2 = tr.querySelector('td[data-label="Mix"]') || tr.querySelectorAll('td')[4]) === null || _ref2 === void 0 || (_ref2 = _ref2.textContent) === null || _ref2 === void 0 ? void 0 : _ref2.trim()) || '';
      var legacy = depthTxt.match(/^(.+?)\s*@\s*(.+)$/i);
      if (legacy) {
        return {
          gas: shortMixLabel(legacy[1]),
          depth: legacy[2].replace(/\s+/g, '')
        };
      }
      return {
        gas: shortMixLabel(mixTxt || depthTxt.split('@')[0] || depthTxt),
        depth: depthTxt.replace(/\s+/g, '') || '-'
      };
    });
  }
  var switches = [];
  var decoModelSel = ((_document$getElementB = document.getElementById('algorithmSelect')) === null || _document$getElementB === void 0 ? void 0 : _document$getElementB.value) || 'ZHLC_GF';
  var isVPM = decoModelSel === 'VPMB' || decoModelSel === 'VPMB_GFS';
  var ppO2Deco = parseFloat((_document$getElementB2 = document.getElementById('ppo2Deco')) === null || _document$getElementB2 === void 0 ? void 0 : _document$getElementB2.value) || 1.6;
  var ppO2Bot = parseFloat((_document$getElementB3 = document.getElementById('ppo2Bottom')) === null || _document$getElementB3 === void 0 ? void 0 : _document$getElementB3.value) || 1.4;
  var _lastStopM = parseFloat((_document$getElementB4 = document.getElementById('lastDecoStop')) === null || _document$getElementB4 === void 0 ? void 0 : _document$getElementB4.value) || 3;
  var _decoStepM = parseFloat((_document$getElementB5 = document.getElementById('decoStep')) === null || _document$getElementB5 === void 0 ? void 0 : _document$getElementB5.value) || 3;
  var dU = units === 'metric';
  var _iterator = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step;
  try {
    for (_iterator.s(); !(_step = _iterator.n()).done;) {
      var idx = _step.value;
      var dgf = getDecoCardFractions(idx);
      if (!dgf || dgf.fN2 === null) continue;
      var gas = shortMixLabel(getGasLabel(dgf.fO2, dgf.fHe));
      var sw = void 0;
      if (isVPM) {
        var fO2 = dgf.fO2 != null ? dgf.fO2 : Math.max(0, 1 - dgf.fN2 - (dgf.fHe || 0));
        var ppLimit = fO2 > 0.60 ? ppO2Deco : ppO2Bot;
        var switchD = vpmDecoSwitchDepthVal(Math.round(fO2 * 100), ppLimit);
        sw = dU ? Math.max(3, Math.floor(switchD / 3) * 3) : Math.max(10, Math.floor(switchD / 10) * 10);
      } else {
        // Global switch-depth fallback (closure-scoped version not available here):
        var _fO = dgf.fO2 != null ? dgf.fO2 : Math.max(0, 1 - dgf.fN2 - (dgf.fHe || 0));
        if (_fO <= 0) continue;else {
          var limit = _fO > 0.60 ? ppO2Deco : ppO2Bot;
          var modM = calcGasMODm(_fO, limit);
          sw = Math.max(_lastStopM, Math.max(0, Math.floor(modM / _decoStepM) * _decoStepM));
        }
      }
      if (!sw || sw <= 0) continue;
      var depth = dU ? sw + 'm' : Math.round(sw * 3.28084) + 'ft';
      switches.push({
        gas: gas,
        depth: depth
      });
    }
  } catch (err) {
    _iterator.e(err);
  } finally {
    _iterator.f();
  }
  return switches;
}
function getDecoGasSwitchLines() {
  var ccrUi = isCcrGasUiMode();
  var prefix = ccrUi ? 'Bailout mix' : 'Deco Gas';
  return getDecoGasSwitches().map(function (g, i) {
    return "".concat(prefix, " ").concat(i + 1, "  : ").concat(g.gas, " (switch @ ").concat(g.depth, ")");
  });
}
function measureDecoScheduleColumnWidth(table) {
  var left = Infinity;
  var right = -Infinity;
  table.querySelectorAll('tr:not(.deco-totals-row)').forEach(function (row) {
    Array.from(row.cells || []).forEach(function (cell) {
      var rect = cell.getBoundingClientRect();
      if (rect.width <= 0) return;
      left = Math.min(left, rect.left);
      right = Math.max(right, rect.right);
    });
  });
  return right > left ? right - left : table.getBoundingClientRect().width || table.offsetWidth;
}
function syncDecoScheduleStackWidths() {
  document.querySelectorAll('.deco-table-wrap').forEach(function (wrap) {
    var _table$tBodies$;
    if (wrap.closest('#contingencyResult') && wrap.closest('#resultsPanel')) {
      wrap.style.width = '';
      wrap.style.removeProperty('--deco-table-width');
      return;
    }
    var table = wrap.querySelector('.deco-table');
    if (!(table !== null && table !== void 0 && (_table$tBodies$ = table.tBodies[0]) !== null && _table$tBodies$ !== void 0 && _table$tBodies$.rows.length)) {
      wrap.style.width = '';
      wrap.style.removeProperty('--deco-table-width');
      return;
    }
    wrap.style.width = '';
    wrap.style.removeProperty('--deco-table-width');
    void wrap.offsetHeight;
    var w = measureDecoScheduleColumnWidth(table);
    if (w > 0) {
      var px = w + 'px';
      wrap.style.setProperty('--deco-table-width', px);
      wrap.style.width = px;
    }
  });
  var stack = document.getElementById('contingencyScheduleStack');
  var btn = stack === null || stack === void 0 ? void 0 : stack.querySelector('.deco-contingency-calc');
  var emergWrap = document.querySelector('#contingencyResult .deco-table-wrap');
  var emergAlerts = document.getElementById('decoAlertsEmergency');
  if (stack !== null && stack !== void 0 && stack.closest('#resultsPanel')) {
    stack.style.width = '100%';
    if (btn) btn.style.width = '100%';
    if (emergWrap) {
      emergWrap.style.width = '';
      emergWrap.style.removeProperty('--deco-table-width');
    }
    if (emergWrap && emergAlerts) {
      emergAlerts.style.width = '';
      emergAlerts.style.maxWidth = '';
      emergAlerts.style.boxSizing = '';
    }
  } else if (stack && btn) {
    var wrap = emergWrap;
    if (wrap !== null && wrap !== void 0 && wrap.style.width) {
      stack.style.width = wrap.style.width;
      btn.style.width = '100%';
    } else {
      stack.style.width = '';
    }
    if (emergWrap && emergAlerts) {
      if (emergWrap.style.width) {
        emergAlerts.style.width = emergWrap.style.width;
        emergAlerts.style.maxWidth = '100%';
        emergAlerts.style.boxSizing = 'border-box';
      } else {
        emergAlerts.style.width = '';
        emergAlerts.style.maxWidth = '';
        emergAlerts.style.boxSizing = '';
      }
    }
  }
  var mainWrap = document.querySelector('#decoResult .deco-table-wrap');
  var decoAlerts = document.getElementById('decoAlerts');
  var decoAlertsNarcotic = document.getElementById('decoAlertsNarcotic');
  [decoAlerts, decoAlertsNarcotic].forEach(function (alertsEl) {
    if (!alertsEl) return;
    if (mainWrap && mainWrap.style.width) {
      alertsEl.style.width = mainWrap.style.width;
      alertsEl.style.maxWidth = '100%';
      alertsEl.style.boxSizing = 'border-box';
    } else {
      alertsEl.style.width = '';
      alertsEl.style.maxWidth = '';
      alertsEl.style.boxSizing = '';
    }
  });
}
function scheduleDecoScheduleStackSync() {
  requestAnimationFrame(function () {
    syncDecoScheduleStackWidths();
    requestAnimationFrame(syncDecoScheduleStackWidths);
  });
}
function planInfoTipIconHtml() {
  return "<span class=\"tip-icon\" onclick=\"event.stopPropagation();showTip('Plan summary',PLAN_INFO_TIP)\" style=\"vertical-align:middle;margin-right:12px;cursor:pointer;\"><svg fill=\"none\" height=\"11\" viewBox=\"0 0 11 11\" width=\"11\" xmlns=\"http://www.w3.org/2000/svg\"><circle cx=\"5.5\" cy=\"5.5\" r=\"5\" stroke=\"currentColor\" stroke-width=\"1.1\"></circle><path d=\"M4.1 3.8 Q4.1 2.4 5.5 2.4 Q6.9 2.4 6.9 3.7 Q6.9 4.6 5.5 5.3 L5.5 6.2\" fill=\"none\" stroke=\"currentColor\" stroke-linecap=\"round\" stroke-width=\"1.1\"></path><circle cx=\"5.5\" cy=\"7.4\" fill=\"currentColor\" r=\"0.55\"></circle></svg></span>";
}
function buildPlanInfoRowHtml(o, phase) {
  var decoColor = o.hasDeco ? 'var(--red)' : 'var(--green)';
  var surfGFVal = o.surfaceGF != null ? Math.round(o.surfaceGF) : null;
  var surfGFDisp = surfGFVal != null ? surfGFVal + '%' : '—';
  var surfGFColor = surfGFVal == null ? 'var(--muted)' : surfGFVal >= 100 ? 'var(--red)' : surfGFVal >= 85 ? 'var(--orange)' : surfGFVal >= 70 ? 'var(--yellow)' : 'var(--green)';
  var firstStopColor = o.decoStop && o.decoStop !== '—' && o.decoStop !== '-' ? 'var(--red)' : 'var(--text)';
  var innerHtml = "<div class=\"deco-totals-inner\">\n      ".concat(planInfoTipIconHtml(), "\n      <span class=\"summary-stat\"><span>RT:</span> <span style=\"color:#2563eb\">").concat(o.runTime, "</span></span> &nbsp;\n      <span class=\"summary-stat\">Deco: <span style=\"color:").concat(decoColor, "\">").concat(o.decoTime, "</span></span> &nbsp;\n      <span class=\"summary-stat\">CNS: <span style=\"color:").concat(o.cnsColor, "\">").concat(o.cns, "</span></span> &nbsp;\n      <span class=\"summary-stat\">OTU: <span style=\"color:").concat(o.otuColor || 'var(--text)', "\">").concat(o.otu, "</span></span> &nbsp;\n      <span class=\"summary-stat\">PrT: <span style=\"color:").concat(o.prtColor, "\" title=\"").concat(o.prtTitle || '', "\">").concat(o.prt, "</span></span> &nbsp;\n      <span class=\"summary-stat\">Surf GF: <span style=\"color:").concat(surfGFColor, "\" title=\"Surface gradient factor \u2014 tissue load \xF7 M-value at surface\">").concat(surfGFDisp, "</span></span> &nbsp;\n      <span class=\"summary-stat\">Decozone: <span style=\"color:").concat(firstStopColor, "\">").concat(o.decozone, "</span></span> &nbsp;\n      <span class=\"summary-stat\">First deco: <span style=\"color:").concat(firstStopColor, "\">").concat(o.decoStop, "</span></span>\n    </div>");
  var totalsEl = document.getElementById('decoTotals');
  if (totalsEl) {
    totalsEl.innerHTML = '';
    totalsEl.style.display = 'none';
  }
  var rowHtml = "<tr class=\"deco-totals-row\" data-phase=\"".concat(phase || 'totals', "\"><td colspan=\"20\" data-run=\"").concat(escHtmlAttr(o.runTime), "\" data-tts=\"").concat(escHtmlAttr(o.tts), "\" data-deco=\"").concat(escHtmlAttr(o.decoTime), "\" data-cns=\"").concat(escHtmlAttr(o.cns), "\" data-otu=\"").concat(escHtmlAttr(o.otu), "\" data-prt=\"").concat(escHtmlAttr(o.prt), "\" data-decozone=\"").concat(escHtmlAttr(o.decozone), "\" data-decostop=\"").concat(escHtmlAttr(o.decoStop), "\" data-surfgf=\"").concat(escHtmlAttr(surfGFDisp), "\">").concat(innerHtml, "</td></tr>");
  scheduleDecoScheduleStackSync();
  return rowHtml;
}
function compactExportDepth(v) {
  if (!v || v === '-' || v === '—') return v;
  return String(v).replace(/(\d+(?:\.\d+)?)\s+(m|ft)\b/gi, '$1$2').trim();
}
function formatPlanSummaryBlock(p, compact) {
  var sg = p.surfGF || '-';
  if (compact) {
    var dz = compactExportDepth(p.decozone);
    var ds = compactExportDepth(p.decoStop);
    return ["RT: ".concat(p.runTime, " TTS: ").concat(p.tts, " Deco: ").concat(p.decoTime), "CNS: ".concat(p.cns, " OTU: ").concat(p.otu, " PrT: ").concat(p.prt), "Surf GF: ".concat(sg, " Decozone: ").concat(dz), "First deco: ".concat(ds)];
  }
  return ["Deco Time: ".concat(p.decoTime, "  Run Time: ").concat(p.runTime, "  TTS: ").concat(p.tts), "CNS: ".concat(p.cns, "  OTU: ").concat(p.otu, "  PrT: ").concat(p.prt), "Surf GF: ".concat(sg, "  Decozone: ").concat(p.decozone, "  First deco: ").concat(p.decoStop)];
}

/** Compact 3-line totals footer for .txt export (no inch marks on times). */
function formatExportSummaryBlock(p) {
  var stripInch = function stripInch(s) {
    return s && s !== '-' ? String(s).replace(/"/g, '') : s;
  };
  var dz = compactExportDepth(p.decozone);
  var ds = compactExportDepth(p.decoStop);
  var sg = p.surfGF || '-';
  return ["RT: ".concat(stripInch(p.runTime), " TTS: ").concat(stripInch(p.tts), " Deco: ").concat(stripInch(p.decoTime)), "CNS: ".concat(p.cns, " OTU: ").concat(p.otu, " PrT: ").concat(p.prt), "Surf GF: ".concat(sg, " Decozone: ").concat(dz, " First deco: ").concat(ds)];
}

// AUDIT-UNIT:UI-UNIT-SWITCHING
function setUnits(u, opts) {
  var _syncBestMixDepthCons, _syncEndDepthConstrai, _syncSurfIntDepthCons, _syncCylinderSizeCons, _document$getElementB6, _syncAltitudeCustomIn, _document$getElementB7, _syncContDepthLabels, _buildUnifiedPlan, _document$getElementB8, _document$getElementB9, _updateGasMODDisplays, _updateTravelGasMOD, _renderEADTable, _renderGasTable, _calcBestMix, _calcEND_tool, _calcSurfInt, _calcAvgDepth, _calcMODTool, _renderModRefTable;
  opts = opts || {};
  var relabelOnly = !!opts.relabelOnly;
  var prev = units;
  units = u;
  updateMinDecoLabels(u !== 'imperial');
  var FT_PER_M = 3.28084;
  var PSI_PER_BAR = 14.5038;
  var CUFT_PER_L = 0.0353147;
  var isToImperial = !relabelOnly && u === 'imperial' && prev === 'metric';
  var isToMetric = !relabelOnly && u === 'metric' && prev === 'imperial';
  var setTxt = function setTxt(id, txt) {
    var el = document.getElementById(id);
    if (el) el.textContent = txt;
  };
  var convertNumericInput = function convertNumericInput(id, toImperial, toMetric, metricMax, imperialMax) {
    var el = document.getElementById(id);
    if (!el) return;
    var v = parseFloat(el.value);
    if (Number.isNaN(v)) return;
    if (isToImperial) {
      if (imperialMax != null) el.max = imperialMax;
      el.value = toImperial(v);
    } else if (isToMetric) {
      if (metricMax != null) el.max = metricMax;
      el.value = toMetric(v);
    }
  };
  var relabelSelectOptions = function relabelSelectOptions(id, formatter) {
    var sel = document.getElementById(id);
    if (!sel) return;
    Array.from(sel.options).forEach(function (opt) {
      var val = parseFloat(opt.value);
      if (!Number.isNaN(val)) opt.text = formatter(val);
    });
  };
  var relabelFieldLabelsByText = function relabelFieldLabelsByText() {
    document.querySelectorAll('label').forEach(function (label) {
      var raw = (label.textContent || '').trim();
      if (/\bBT SAC\b/i.test(raw)) label.textContent = u === 'imperial' ? 'BT SAC (ft³/min)' : 'BT SAC (L/min)';
      if (/\bDeco SAC\b/i.test(raw)) label.textContent = u === 'imperial' ? 'Deco SAC (ft³/min)' : 'Deco SAC (L/min)';
      if (/\bCyl\. Pressure\b/i.test(raw)) label.textContent = u === 'imperial' ? 'Cyl. Pressure (psi)' : 'Cyl. Pressure (bar)';
      if (/\bBottom Gas\b/i.test(raw) && /\(.*\)/.test(raw) && /bar|psi|L|cu ft|ft³/i.test(raw)) {
        if (/bar|psi/i.test(raw)) label.textContent = u === 'imperial' ? 'Bottom Gas (psi)' : 'Bottom Gas (bar)';
        if (/\bL\b|cu ft|ft³/i.test(raw)) label.textContent = u === 'imperial' ? 'Bottom Gas Size (ft³)' : 'Bottom Gas Size (L)';
      }
      if (/\bDeco Gas 1\b/i.test(raw) && /\(.*\)/.test(raw) && /bar|psi|L|cu ft|ft³/i.test(raw)) {
        if (/bar|psi/i.test(raw)) label.textContent = u === 'imperial' ? 'Deco Gas 1 (psi)' : 'Deco Gas 1 (bar)';
        if (/\bL\b|cu ft|ft³/i.test(raw)) label.textContent = u === 'imperial' ? 'Deco Gas 1 Size (ft³)' : 'Deco Gas 1 Size (L)';
      }
      if (/\bDeco Gas 2\b/i.test(raw) && /\(.*\)/.test(raw) && /bar|psi|L|cu ft|ft³/i.test(raw)) {
        if (/bar|psi/i.test(raw)) label.textContent = u === 'imperial' ? 'Deco Gas 2 (psi)' : 'Deco Gas 2 (bar)';
        if (/\bL\b|cu ft|ft³/i.test(raw)) label.textContent = u === 'imperial' ? 'Deco Gas 2 Size (ft³)' : 'Deco Gas 2 Size (L)';
      }
    });
  };
  var refreshMultiDiveDepthDisplays = function refreshMultiDiveDepthDisplays() {
    document.querySelectorAll('.mDLbl').forEach(function (el) {
      el.textContent = "Depth (".concat(u === 'imperial' ? 'ft' : 'm', ")");
    });
    document.querySelectorAll('.mDepth').forEach(function (el) {
      var v = parseFloat(el.value);
      if (Number.isNaN(v)) return;
      if (isToImperial) {
        el.value = Math.round(v * FT_PER_M);
        el.max = 330;
      } else if (isToMetric) {
        el.value = (v / FT_PER_M).toFixed(1).replace(/\.0$/, '');
        el.max = 100;
      }
    });
  };
  var refreshInlineStaticText = function refreshInlineStaticText() {
    var versionRow = document.getElementById('mainVersionLabel');
    if (versionRow) versionRow.textContent = versionRow.textContent;
  };
  var syncSelect = function syncSelect(id) {
    var el = document.getElementById(id);
    if (el) el.value = u;
  };
  syncSelect('unitsSelect');
  ['uImp', 'uMet'].forEach(function (id) {
    var el = document.getElementById(id);
    if (el) el.classList.toggle('active', id === (u === 'imperial' ? 'uImp' : 'uMet'));
  });
  ['ndlUImp', 'ndlUMet', 'mUImp', 'mUMet'].forEach(function (id) {
    var el = document.getElementById(id);
    if (!el) return;
    var isImp = id.includes('Imp');
    el.classList.toggle('active', u === 'imperial' ? isImp : !isImp);
  });
  var applyDepthUnitSwitch = function applyDepthUnitSwitch(id, metricMax, imperialMax) {
    var el = document.getElementById(id);
    if (!el || !isToImperial && !isToMetric) return;
    var metres = parseFloat(el.dataset.depthM);
    if (!Number.isFinite(metres)) {
      var v = parseFloat(el.value);
      if (Number.isNaN(v)) return;
      metres = prev === 'imperial' ? v / FT_PER_M : v;
      el.dataset.depthM = String(metres);
    }
    if (isToImperial) {
      if (imperialMax != null) el.max = imperialMax;
      el.value = String(Math.round(metres * FT_PER_M));
    } else if (isToMetric) {
      if (metricMax != null) el.max = metricMax;
      el.value = Number.isInteger(metres) ? String(Math.round(metres)) : metres.toFixed(1).replace(/\.0$/, '');
    }
    el.dataset.depthM = String(metres);
  };
  applyDepthUnitSwitch('recDepth', 100, 330);
  applyDepthUnitSwitch('tecDepth', 120, 394);
  applyDepthUnitSwitch('bestMixDepth', 100, 330);
  (_syncBestMixDepthCons = syncBestMixDepthConstraints) === null || _syncBestMixDepthCons === void 0 || _syncBestMixDepthCons();
  applyDepthUnitSwitch('cnsDepth', 60, 197);
  setTxt('cnsDepthLbl', "Depth (".concat(u === 'imperial' ? 'ft' : 'm', ")"));
  applyDepthUnitSwitch('endDepth', 100, 330);
  (_syncEndDepthConstrai = syncEndDepthConstraints) === null || _syncEndDepthConstrai === void 0 || _syncEndDepthConstrai();
  applyDepthUnitSwitch('siD1Depth', 60, 197);
  applyDepthUnitSwitch('siD2Depth', 60, 197);
  (_syncSurfIntDepthCons = syncSurfIntDepthConstraints) === null || _syncSurfIntDepthCons === void 0 || _syncSurfIntDepthCons();
  var CYL_SIZE_MAX_L = 50;
  var CYL_SIZE_MIN_L = 0.5;
  var cylMaxCuFt = function cylMaxCuFt() {
    return +(CYL_SIZE_MAX_L * CUFT_PER_L).toFixed(2);
  };
  var cylMinCuFt = function cylMinCuFt() {
    return +(CYL_SIZE_MIN_L * CUFT_PER_L).toFixed(2);
  };
  var applyCylinderSizeUnitSwitch = function applyCylinderSizeUnitSwitch(id) {
    var el = document.getElementById(id);
    if (!el || !isToImperial && !isToMetric) return;
    var litres = parseFloat(el.dataset.volumeL);
    if (!Number.isFinite(litres)) {
      var v = parseFloat(el.value);
      if (Number.isNaN(v)) return;
      litres = prev === 'imperial' ? v / CUFT_PER_L : v;
      el.dataset.volumeL = String(litres);
    }
    if (isToImperial) {
      el.min = String(cylMinCuFt());
      el.max = String(cylMaxCuFt());
      el.step = String(cylMinCuFt());
      el.value = (litres * CUFT_PER_L).toFixed(1).replace(/\.0$/, '');
    } else if (isToMetric) {
      el.min = String(CYL_SIZE_MIN_L);
      el.max = String(CYL_SIZE_MAX_L);
      el.step = String(CYL_SIZE_MIN_L);
      el.value = Number.isInteger(litres) ? String(Math.round(litres)) : litres.toFixed(1).replace(/\.0$/, '');
    }
    el.dataset.volumeL = String(litres);
  };

  // ── Cylinder pressure conversion (all gas cards including dynamic ones) ──
  var allCylPres = ['cylBot_pres', 'cylTravelGas_pres', 'cylDg1_pres', 'cylDg2_pres'].concat(_toConsumableArray(Array.from(document.querySelectorAll('[id^="cylDg"][id$="_pres"]')).map(function (el) {
    return el.id;
  }))).filter(function (v, i, a) {
    return a.indexOf(v) === i;
  });
  var allCylSize = ['cylBot_size', 'cylTravelGas_size', 'cylDg1_size', 'cylDg2_size'].concat(_toConsumableArray(Array.from(document.querySelectorAll('[id^="cylDg"][id$="_size"]')).map(function (el) {
    return el.id;
  }))).filter(function (v, i, a) {
    return a.indexOf(v) === i;
  });
  var allCylReserve = ['cylBot_reserve', 'cylTravelGas_reserve', 'cylDg1_reserve', 'cylDg2_reserve'].concat(_toConsumableArray(Array.from(document.querySelectorAll('[id^="cylDg"][id$="_reserve"]')).map(function (el) {
    return el.id;
  }))).filter(function (v, i, a) {
    return a.indexOf(v) === i;
  });
  allCylPres.forEach(function (id) {
    convertNumericInput(id, function (v) {
      return Math.round(v * PSI_PER_BAR);
    }, function (v) {
      return Math.round(v / PSI_PER_BAR);
    }, 300, 4351);
  });
  allCylSize.forEach(function (id) {
    applyCylinderSizeUnitSwitch(id);
  });
  (_syncCylinderSizeCons = syncCylinderSizeConstraints) === null || _syncCylinderSizeCons === void 0 || _syncCylinderSizeCons();
  allCylReserve.forEach(function (id) {
    convertNumericInput(id, function (v) {
      return Math.round(v * PSI_PER_BAR);
    }, function (v) {
      return Math.round(v / PSI_PER_BAR);
    }, 300, 4351);
  });

  // SAC conversion (L/min ↔ cu ft/min)
  convertNumericInput('sacBottom', function (v) {
    return (v * CUFT_PER_L).toFixed(2).replace(/\.0+$/, '');
  }, function (v) {
    return Math.round(v / CUFT_PER_L);
  }, 60, 2.12);
  convertNumericInput('sacDeco', function (v) {
    return (v * CUFT_PER_L).toFixed(2).replace(/\.0+$/, '');
  }, function (v) {
    return Math.round(v / CUFT_PER_L);
  }, 40, 1.41);

  // ── Gas Plan tab conversions ──
  ['gpBot_size', 'gpTravel_size', 'gpDg1_size', 'gpDg2_size'].forEach(function (id) {
    convertNumericInput(id, function (v) {
      return (v * CUFT_PER_L).toFixed(1).replace(/\.0$/, '');
    }, function (v) {
      return Math.round(v / CUFT_PER_L);
    }, 50, 1.77);
  });
  ['gpBot_fill', 'gpBot_reserve', 'gpTravel_fill', 'gpTravel_reserve', 'gpDg1_fill', 'gpDg1_reserve', 'gpDg2_fill', 'gpDg2_reserve'].forEach(function (id) {
    convertNumericInput(id, function (v) {
      return Math.round(v * PSI_PER_BAR);
    }, function (v) {
      return Math.round(v / PSI_PER_BAR);
    }, 300, 4351);
  });
  ['gpBot_size_lbl', 'gpTravel_size_lbl', 'gpDg1_size_lbl', 'gpDg2_size_lbl'].forEach(function (id) {
    return setTxt(id, u === 'imperial' ? 'Size (ft³)' : 'Size (L)');
  });
  ['gpBot_fill_lbl', 'gpTravel_fill_lbl', 'gpDg1_fill_lbl', 'gpDg2_fill_lbl'].forEach(function (id) {
    return setTxt(id, u === 'imperial' ? 'Fill psi' : 'Fill bar');
  });
  ['gpBot_reserve_lbl', 'gpTravel_reserve_lbl', 'gpDg1_reserve_lbl', 'gpDg2_reserve_lbl', 'cylBot_reserve_lbl', 'cylTravelGas_reserve_lbl', 'cylDg1_reserve_lbl', 'cylDg2_reserve_lbl'].forEach(function (id) {
    return setTxt(id, u === 'imperial' ? 'Reserve psi' : 'Reserve bar');
  });
  document.querySelectorAll('[id^="cylDg"][id$="_reserve_lbl"]').forEach(function (el) {
    el.textContent = u === 'imperial' ? 'Reserve psi' : 'Reserve bar';
  });
  if ((_document$getElementB6 = document.getElementById('gasplan')) !== null && _document$getElementB6 !== void 0 && _document$getElementB6.classList.contains('active')) calcGasPlan();
  var du = u === 'metric' ? 'm' : 'ft';
  setTxt('depthLbl', "Depth (".concat(du, ")"));
  setTxt('ndlDepthHdr', "Depth (".concat(du, ")"));
  _syncDepthBtSteppers();
  // Altitude custom input unit label
  setTxt('altitudeCustomUnit', du);
  (_syncAltitudeCustomIn = syncAltitudeCustomInputConstraints) === null || _syncAltitudeCustomIn === void 0 || _syncAltitudeCustomIn();
  if (((_document$getElementB7 = document.getElementById('altitudeSelect')) === null || _document$getElementB7 === void 0 ? void 0 : _document$getElementB7.value) === 'custom') {
    var altInp = document.getElementById('altitudeCustomInput');
    if (altInp) altInp.value = altitudeMToCustomDisplay(altitudeM);
  }
  var depthToggle = document.getElementById('stopDepthToggle');
  if (depthToggle) {
    var depthOpts = depthToggle.querySelectorAll('.option-pill-opt');
    if (depthOpts[0]) depthOpts[0].textContent = u === 'metric' ? '3 m' : '10 ft';
    if (depthOpts[1]) depthOpts[1].textContent = u === 'metric' ? '6 m' : '20 ft';
  }
  relabelSelectOptions('descentRate', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft/min' : v + ' m/min';
  });
  relabelSelectOptions('ascentRate', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft/min' : v + ' m/min';
  });
  relabelSelectOptions('decoAscentRate', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft/min' : v + ' m/min';
  });
  relabelSelectOptions('surfaceAscentRate', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft/min' : v + ' m/min';
  });
  relabelSelectOptions('decoStep', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft' : v + ' m';
  });
  relabelSelectOptions('lastDecoStop', function (v) {
    return u === 'imperial' ? Math.round(v * FT_PER_M) + ' ft' : v + ' m';
  });
  [['refDepthRec', 40, 130], ['refDepthOW', 18, 60], ['refDepthAdv', 30, 100]].forEach(function (_ref3) {
    var _ref4 = _slicedToArray(_ref3, 3),
      id = _ref4[0],
      m = _ref4[1],
      ft = _ref4[2];
    var el = document.getElementById(id);
    if (el) el.textContent = u === 'imperial' ? "".concat(ft, " ft") : "".concat(m, " m");
  });
  var ndlRangeEl = document.getElementById('ndlRange');
  if (ndlRangeEl) {
    var metricLabels = ['10–12 m', '15–18 m', '21–27 m', '30–40 m'];
    var imperialLabels = ['35–40 ft', '50–60 ft', '70–90 ft', '100–130 ft'];
    Array.from(ndlRangeEl.options).forEach(function (opt, i) {
      opt.text = u === 'imperial' ? imperialLabels[i] : metricLabels[i];
    });
  }

  // ── Cylinder label updates (fixed IDs with underscores) ──
  setTxt('cylBot_pres_lbl', u === 'imperial' ? 'PSI' : 'Bar');
  setTxt('cylTravelGas_pres_lbl', u === 'imperial' ? 'PSI' : 'Bar');
  setTxt('cylDg1_pres_lbl', u === 'imperial' ? 'PSI' : 'Bar');
  setTxt('cylDg2_pres_lbl', u === 'imperial' ? 'PSI' : 'Bar');
  // Dynamic deco gas card labels
  document.querySelectorAll('[id^="cylDg"][id$="_pres_lbl"]').forEach(function (el) {
    el.textContent = u === 'imperial' ? 'PSI' : 'Bar';
  });
  document.querySelectorAll('[id^="cylDg"][id$="_size"] ~ * label, label[id$="_size_lbl"]').forEach(function (el) {
    el.textContent = u === 'imperial' ? 'Size (ft³)' : 'Size (L)';
  });
  // Cyl size labels (static)
  document.querySelectorAll('.field label').forEach(function (lbl) {
    var t = lbl.textContent.trim();
    if (/^Cyl\. Size/i.test(t) || t === 'Size' || t === 'Cyl. Size (L)' || t === 'Cyl. Size (ft³)' || t === 'Cyl. Size (cu ft)' || /^Size \(/.test(t)) {
      lbl.textContent = u === 'imperial' ? 'Size (ft³)' : 'Size (L)';
    }
    if (/^Cyl\. bar/i.test(t) || /^Cyl\. psi/i.test(t) || t === 'Bar' || t === 'PSI') {
      lbl.textContent = u === 'imperial' ? 'PSI' : 'Bar';
    }
  });
  ['cylSizeUnit', 'cylinderSizeUnit', 'tankSizeUnit'].forEach(function (id) {
    return setTxt(id, u === 'imperial' ? 'ft³' : 'L');
  });
  ['cylPressureUnit', 'tankPressureUnit'].forEach(function (id) {
    return setTxt(id, u === 'imperial' ? 'psi' : 'bar');
  });
  relabelFieldLabelsByText();
  refreshMultiDiveDepthDisplays();
  refreshInlineStaticText();
  (_syncContDepthLabels = syncContDepthLabels) === null || _syncContDepthLabels === void 0 || _syncContDepthLabels();
  ndlUnits = u;
  multiUnits = u;

  // Skip all recalculations in headless test mode
  if (window._zhlHeadless) return;
  renderNDLTable === null || renderNDLTable === void 0 || renderNDLTable();
  (_buildUnifiedPlan = buildUnifiedPlan) === null || _buildUnifiedPlan === void 0 || _buildUnifiedPlan();
  if ((_document$getElementB8 = document.getElementById('plannerResult')) !== null && _document$getElementB8 !== void 0 && _document$getElementB8.innerHTML) runPlanner === null || runPlanner === void 0 || runPlanner();
  if ((_document$getElementB9 = document.getElementById('decoResult')) !== null && _document$getElementB9 !== void 0 && _document$getElementB9.innerHTML) {
    if (window._lastContingency) calcContingency();
  }

  // ── Refresh gas card MOD displays and travel gas switch depth ──
  (_updateGasMODDisplays = updateGasMODDisplays) === null || _updateGasMODDisplays === void 0 || _updateGasMODDisplays();
  (_updateTravelGasMOD = updateTravelGasMOD) === null || _updateTravelGasMOD === void 0 || _updateTravelGasMOD();

  // ── Refresh Tools panels that have depth/unit-sensitive displays ──
  // Update depth display labels on tools sliders when unit changes
  var bmDepthEl = document.getElementById('bestMixDepth');
  if (bmDepthEl) document.getElementById('bestMixDepthDisplay').textContent = bmDepthEl.value + (u === 'imperial' ? ' ft' : ' m');

  // Recalculate visible tools panels
  (_renderEADTable = renderEADTable) === null || _renderEADTable === void 0 || _renderEADTable();
  (_renderGasTable = renderGasTable) === null || _renderGasTable === void 0 || _renderGasTable();
  (_calcBestMix = calcBestMix) === null || _calcBestMix === void 0 || _calcBestMix();
  (_calcEND_tool = calcEND_tool) === null || _calcEND_tool === void 0 || _calcEND_tool();
  (_calcSurfInt = calcSurfInt) === null || _calcSurfInt === void 0 || _calcSurfInt();
  (_calcAvgDepth = calcAvgDepth) === null || _calcAvgDepth === void 0 || _calcAvgDepth();
  (_calcMODTool = calcMODTool) === null || _calcMODTool === void 0 || _calcMODTool();
  (_renderModRefTable = renderModRefTable) === null || _renderModRefTable === void 0 || _renderModRefTable();
}

// ═══════════════════════════════════════════════
// SAFETY STOP CONTROLS
// ═══════════════════════════════════════════════
function _syncStopDepthPill() {
  var toggle = document.getElementById('stopDepthToggle');
  if (toggle) toggle.dataset.side = stopDepthM === 6 ? 'right' : 'left';
}
function _syncStopDurPill() {
  var toggle = document.getElementById('stopDurToggle');
  if (toggle) toggle.dataset.side = stopDurMin === 5 ? 'right' : 'left';
}
function toggleStopDepth() {
  setStopDepth(stopDepthM === 3 ? 6 : 3);
}
function toggleStopDur() {
  setStopDur(stopDurMin === 3 ? 5 : 3);
}
function setStopDepth(m) {
  stopDepthM = m;
  _syncStopDepthPill();
}
function setStopDur(min) {
  stopDurMin = min;
  _syncStopDurPill();
}

// ═══════════════════════════════════════════════
// GF CONTROLS
// ═══════════════════════════════════════════════
function autoSlashGF(input) {
  var oldVal = input.value;
  var oldPos = input.selectionStart;

  // If backspace was pressed at or near the slash, handle specially
  if (oldVal.length > 0 && oldVal[2] === '/' && oldPos === 3) {
    // Cursor is right after slash — delete the slash and one Low digit
    input.value = oldVal.slice(0, 1) + oldVal.slice(3);
    input.selectionStart = input.selectionEnd = 1;
    return;
  }

  // Normal digit-only filtering and slash insertion
  var v = input.value.replace(/[^\d/]/g, '');
  var digits = v.replace('/', '');
  if (digits.length >= 2) {
    v = digits.slice(0, 2) + '/' + digits.slice(2, 4);
  } else {
    v = digits;
  }
  input.value = v;

  // Smart cursor positioning: jump over the slash
  if (oldPos === 2 && v[2] === '/') {
    // User just typed the 2nd digit, move cursor past the slash
    input.selectionStart = input.selectionEnd = 3;
  } else if (oldPos === 3 && v[2] === '/') {
    // Cursor at slash position, move past it
    input.selectionStart = input.selectionEnd = 3;
  }
}

// ═══════════════════════════════════════════════
// BÜHLMANN PHYSICS — delegates to ZhlEngineBundle (zhl-physics-core.js)
// ═══════════════════════════════════════════════
// AUDIT-UNIT:UI-ZHL-DELEGATES
function _syncZhlBundleEnv(extra) {
  if (!window.ZhlEngineBundle) return;
  ZhlEngineBundle.applyEnvironment(Object.assign({
    altSurfaceP: altSurfaceP,
    barPerMetre: BAR_PER_METRE,
    waterVapor: WATER_VAPOR,
    altAcclimatized: altAcclimatized,
    allowO2AtMOD: typeof allowO2AtMOD !== 'undefined' ? allowO2AtMOD : true
  }, extra || {}));
}
function initTissues() {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.initTissues();
}
function depthBar(m) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.depthBar(m);
}
function schreiner(p0, pGas, ht, t) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.schreiner(p0, pGas, ht, t);
}
function schreinerLinear(p0, fN2, ht, t, p0Amb, R) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.schreinerLinear(p0, fN2, ht, t, p0Amb, R);
}
function saturateLinear(tissues, fromDepth, toDepth, t, fN2, fHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.saturateLinear(tissues, fromDepth, toDepth, t, fN2, fHe);
}
function saturate(tissues, depthM, t, fN2, fHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.saturate(tissues, depthM, t, fN2, fHe);
}
function toggleCustomO2() {
  var gasMixEl = document.getElementById('gasMix');
  if (!gasMixEl) return;
  var mix = gasMixEl.value;
  if (typeof plannerAlgo !== 'undefined' && plannerAlgo === 'rec' && !['air', 'ean32', 'ean36'].includes(mix)) {
    gasMixEl.value = 'air';
    mix = 'air';
  }
  if (typeof algo !== 'undefined' && algo === 'padi') return;
  var customField = document.getElementById('customO2Field');
  if (customField) customField.style.display = mix === 'custom' ? 'block' : 'none';
  var showTrimix = mix === 'trimix';
  var trimixO2Field = document.getElementById('plannerTrimixO2Field');
  var trimixHeField = document.getElementById('plannerTrimixHeField');
  if (trimixO2Field) trimixO2Field.style.display = showTrimix ? 'block' : 'none';
  if (trimixHeField) trimixHeField.style.display = showTrimix ? 'block' : 'none';
  syncTecGasMixMemory();
}
function getN2Frac(mix) {
  if (mix === 'ean32') return FN2_EAN32;
  if (mix === 'ean36') return FN2_EAN36;
  if (mix === 'custom') {
    var _document$getElementB0;
    return ZhlEngineBundle.n2FracFromCustomO2(parseFloat((_document$getElementB0 = document.getElementById('customO2')) === null || _document$getElementB0 === void 0 ? void 0 : _document$getElementB0.value) || 21);
  }
  if (mix === 'trimix') {
    var o2 = readDomO2Pct('plannerTrimixO2');
    var he = readDomHePct('plannerTrimixHe');
    var n2 = ZhlEngineBundle.n2FracFromPercentages(o2, he);
    if (n2 != null) return n2;
    console.warn('[LSP] Invalid trimix N₂ fraction — check O₂/He percentages.');
    return null;
  }
  return FN2_AIR;
}
function getHeFrac(mix) {
  if (mix === 'trimix') {
    var he = readDomHePct('plannerTrimixHe');
    return Number.isFinite(he) ? Math.min(1, Math.max(0, he / 100)) : 0;
  }
  return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CCR / Rebreather shared physics (v2.30) — used by Bühlmann + VPM paths
// ═══════════════════════════════════════════════════════════════════════════════
function getPscrMinPpo2() {
  return typeof ZhlEngineBundle !== 'undefined' && ZhlEngineBundle.PSCR_MIN_PPO2 != null ? ZhlEngineBundle.PSCR_MIN_PPO2 : 0.16;
}
var PSCR_DILUENT_EPSILON = 1e-6;
var PSCR_DEFAULT_BYPASS_RATIO = 10; // fallback when loop/diluent delta vanishes (typical sport pSCR ~1:7–1:10)
var PSCR_LOOP_VOLUME_MIN = ZhlEngineBundle.PSCR_LOOP_VOLUME_MIN;
var PSCR_LOOP_VOLUME_MAX = ZhlEngineBundle.PSCR_LOOP_VOLUME_MAX;
var PSCR_METABOLIC_O2_MIN = ZhlEngineBundle.PSCR_METABOLIC_O2_MIN;
var PSCR_METABOLIC_O2_MAX = ZhlEngineBundle.PSCR_METABOLIC_O2_MAX;
function parseCcrSetpoint(id, fallback) {
  var _document$getElementB1;
  var v = parseFloat((_document$getElementB1 = document.getElementById(id)) === null || _document$getElementB1 === void 0 ? void 0 : _document$getElementB1.value);
  return Number.isFinite(v) ? v : fallback;
}
function getCCRSettingsFromDOM() {
  var _document$getElementB10, _document$getElementB11, _document$getElementB12, _document$getElementB13, _document$getElementB14, _document$getElementB15, _document$getElementB16, _document$getElementB17;
  var circuit = ((_document$getElementB10 = document.getElementById('circuitSelect')) === null || _document$getElementB10 === void 0 ? void 0 : _document$getElementB10.value) || 'OC';
  var bailoutEl = document.getElementById('ccrBailoutToggle');
  var bailout = bailoutEl ? bailoutEl.type === 'checkbox' ? bailoutEl.checked : bailoutEl.value === 'on' : false;
  var decoSP = parseCcrSetpoint('ccrDecoSetpoint', parseCcrSetpoint('ccrSetpoint', 1.3));
  var loopVol = parseCcrSetpoint('ccrLoopVolume', parseCcrSetpoint('scrLoopVolume', 10));
  var metO2 = parseCcrSetpoint('ccrMetabolicO2', parseCcrSetpoint('scrMetabolicO2', 1.5));
  // Keep hidden legacy fields in sync for saved presets / exports
  var legSp = document.getElementById('ccrSetpoint');
  var legVol = document.getElementById('scrLoopVolume');
  var legMet = document.getElementById('scrMetabolicO2');
  if (legSp) legSp.value = String(decoSP);
  if (legVol) legVol.value = String(loopVol);
  if (legMet) legMet.value = String(metO2);
  return {
    circuit: circuit,
    setpoint: decoSP,
    decoSetpoint: decoSP,
    bottomSetpoint: parseCcrSetpoint('ccrBottomSetpoint', 1.2),
    descentSetpoint: parseCcrSetpoint('ccrDescentSetpoint', 0.7),
    bailout: bailout,
    bailoutGfLow: parseInt((_document$getElementB11 = document.getElementById('ccrBailoutGfLow')) === null || _document$getElementB11 === void 0 ? void 0 : _document$getElementB11.value, 10) || 50,
    bailoutGfHigh: parseInt((_document$getElementB12 = document.getElementById('ccrBailoutGfHigh')) === null || _document$getElementB12 === void 0 ? void 0 : _document$getElementB12.value, 10) || 85,
    scrLoopVolume: loopVol,
    scrMetabolicO2: metO2,
    sacStress: parseFloat((_document$getElementB13 = document.getElementById('ccrSacStress')) === null || _document$getElementB13 === void 0 ? void 0 : _document$getElementB13.value) || 50,
    sacDecoCcr: parseFloat((_document$getElementB14 = document.getElementById('ccrSacDeco')) === null || _document$getElementB14 === void 0 ? void 0 : _document$getElementB14.value) || 25,
    stressTimeMin: parseFloat((_document$getElementB15 = document.getElementById('ccrStressTime')) === null || _document$getElementB15 === void 0 ? void 0 : _document$getElementB15.value) || 10,
    problemSolveMin: parseFloat((_document$getElementB16 = document.getElementById('ccrProblemSolve')) === null || _document$getElementB16 === void 0 ? void 0 : _document$getElementB16.value) || 3,
    diluentUseAsBailout: ((_document$getElementB17 = document.getElementById('diluentUseAsBailout')) === null || _document$getElementB17 === void 0 ? void 0 : _document$getElementB17.value) === 'on'
  };
}
function mergeCCRSettings(settings) {
  var s = settings || {};
  var hasSnap = s.circuit != null && s.circuit !== '';
  var dom = hasSnap ? {} : typeof document !== 'undefined' ? getCCRSettingsFromDOM() : {};
  var raw = {
    circuit: s.circuit || dom.circuit || 'OC',
    setpoint: s.setpoint != null ? s.setpoint : s.decoSetpoint != null ? s.decoSetpoint : dom.setpoint,
    decoSetpoint: s.decoSetpoint != null ? s.decoSetpoint : s.setpoint != null ? s.setpoint : dom.decoSetpoint,
    bottomSetpoint: s.bottomSetpoint != null ? s.bottomSetpoint : dom.bottomSetpoint,
    descentSetpoint: s.descentSetpoint != null ? s.descentSetpoint : dom.descentSetpoint,
    bailout: s.bailout != null ? s.bailout : dom.bailout,
    bailoutGfLow: s.bailoutGfLow != null ? s.bailoutGfLow : dom.bailoutGfLow,
    bailoutGfHigh: s.bailoutGfHigh != null ? s.bailoutGfHigh : dom.bailoutGfHigh,
    scrLoopVolume: s.scrLoopVolume != null ? s.scrLoopVolume : dom.scrLoopVolume,
    scrMetabolicO2: s.scrMetabolicO2 != null ? s.scrMetabolicO2 : dom.scrMetabolicO2,
    sacStress: s.sacStress != null ? s.sacStress : dom.sacStress,
    sacDecoCcr: s.sacDecoCcr != null ? s.sacDecoCcr : dom.sacDecoCcr,
    stressTimeMin: s.stressTimeMin != null ? s.stressTimeMin : dom.stressTimeMin,
    problemSolveMin: s.problemSolveMin != null ? s.problemSolveMin : dom.problemSolveMin,
    ccrPhase: s.ccrPhase || dom.ccrPhase || null,
    scrRuntimeMin: s.scrRuntimeMin || 0,
    diluentUseAsBailout: s.diluentUseAsBailout != null ? s.diluentUseAsBailout : dom.diluentUseAsBailout
  };
  if (window.ZhlEngineBundle && ZhlEngineBundle.normalizeCCRSettings) {
    var norm = ZhlEngineBundle.normalizeCCRSettings(raw);
    if (raw.diluentUseAsBailout != null) norm.diluentUseAsBailout = raw.diluentUseAsBailout;
    return norm;
  }
  return raw;
}

// AUDIT-UNIT:UI-CCR-DELEGATES
function canonicalCircuit(circuit) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.canonicalCircuit(circuit);
}
function isRebreatherCircuit(circuit) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.isRebreatherCircuit(circuit);
}
function depthAtSetpointCrossing(setpoint, surfP) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.depthAtSetpointCrossing(setpoint, surfP);
}
function getEffectiveSetpointAtDepth(depthM, ccr, surfP, phase) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getEffectiveSetpointAtDepth(depthM, mergeCCRSettings(ccr), surfP, phase);
}
function getCcrMetabolicO2Rate(ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getCcrMetabolicO2Rate(mergeCCRSettings(ccr));
}
function computePSCRFractions(pAmb, fO2, fHe, ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.computePSCRFractions(pAmb, fO2, fHe, mergeCCRSettings(ccr));
}
function ccrLoopGasBelowSetpoint(pAmb, fO2, fHe, setpoint) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.ccrLoopGasBelowSetpoint(pAmb, fO2, fHe, setpoint);
}
function getInspiredInertPressures(pAmb, setpoint, fO2, fHe, ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getInspiredInertPressures(pAmb, setpoint, fO2, fHe, mergeCCRSettings(ccr));
}
function getCCRInertSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getCCRInertSchreinerParams(pAmbStart, setpoint, fO2, fHe, pressureRate, mergeCCRSettings(ccr));
}
function getSetpointBoundaryDepths(ccr, surfP) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getSetpointBoundaryDepths(mergeCCRSettings(ccr), surfP);
}
function splitLinearDepthAtBoundaries(fromDepth, toDepth, boundaryDepths) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.splitLinearDepthAtBoundaries(fromDepth, toDepth, boundaryDepths);
}
function splitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.splitSegmentAtSetpoint(fromDepth, toDepth, setpoint, surfP);
}
function getEffectivePpo2(pAmb, setpoint, fO2, ccr, depthM, fHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getEffectivePpo2(pAmb, setpoint, fO2, mergeCCRSettings(ccr), depthM, fHe);
}
function schreinerLinearCCR(p0, ht, t, p0Amb, R, setpoint, fO2, fHe, ccr, isHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.schreinerLinearCCR(p0, ht, t, p0Amb, R, setpoint, fO2, fHe, mergeCCRSettings(ccr), isHe);
}
function saturateLinearCCR(tissues, fromDepth, toDepth, t, fO2, fHe, ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.saturateLinearCCR(tissues, fromDepth, toDepth, t, fO2, fHe, mergeCCRSettings(ccr));
}
function saturateCCR(tissues, depthM, t, fO2, fHe, ccr) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.saturateCCR(tissues, depthM, t, fO2, fHe, mergeCCRSettings(ccr));
}
function loadTissuesWithCCR(tissues, fromDepth, toDepth, time, fO2, fHe, ccr, constantDepth) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.loadTissuesWithCCR(tissues, fromDepth, toDepth, time, fO2, fHe, mergeCCRSettings(ccr), constantDepth);
}
function gfAdjustedMValue(a, b, pAmb, gf) {
  return a * gf + pAmb * (1 - gf + gf / b);
}
function isCcrOnLoopGasLabel(label) {
  if (typeof label !== 'string') return false;
  var u = label.toUpperCase();
  return u.startsWith('CCR ') || u.startsWith('PSCR ');
}
function ccrDiluentSurfaceLpm(depthM) {
  var ccr = getCCRSettingsFromDOM();
  var metRate = getCcrMetabolicO2Rate(ccr);
  var bot = getBottomGasFractions();
  if (!bot) return NaN;
  var fO2Dil = Math.max(0.01, bot.fO2 || 0.21);
  if (ccr.circuit === 'pSCR' && !ccr.bailout) {
    var pAmb = altSurfaceP + (depthM || 0) * BAR_PER_METRE;
    var pSurf = altSurfaceP || 1.01325;
    var fr = computePSCRFractions(pAmb, fO2Dil, bot.fHe || 0, ccr);
    var delta = Math.max(PSCR_DILUENT_EPSILON, fO2Dil - fr.fO2);
    var flowAtDepth = metRate / delta;
    return flowAtDepth * (pSurf / pAmb);
  }
  return metRate / fO2Dil;
}
function isCcrDiluentGasLabel(label) {
  if (!label) return false;
  if (isCcrOnLoopGasLabel(label)) return true;
  var ccr = getCCRSettingsFromDOM();
  if (!isRebreatherCircuit(ccr.circuit) || ccr.bailout) return false;
  var bot = getBottomGasFractions();
  if (!bot) return false;
  var norm = function norm(s) {
    return String(s).trim().replace(/^(CCR|PSCR)\s+/i, '').toUpperCase();
  };
  var a = norm(label);
  var b = norm(getGasLabel(bot.fO2, bot.fHe));
  if (a === b) return true;
  if (a === 'AIR' && b === '21/00') return true;
  if ((a === '100%' || a === '100') && b.startsWith('100')) return true;
  return false;
}
function isCcrOnLoopProfile(ccr) {
  var cfg = ccr || getCCRSettingsFromDOM();
  return isRebreatherCircuit(cfg.circuit) && !cfg.bailout;
}
/** Display label for on-loop rebreather gas (CCR Air / pSCR Air). OC/bailout → diluent unchanged. */
function loopMixLabelFor(diluentLabel, ccr) {
  var cfg = ccr || getCCRSettingsFromDOM();
  if (!isRebreatherCircuit(cfg.circuit) || cfg.bailout) return diluentLabel;
  if (typeof diluentLabel === 'string' && /^(CCR|pSCR)\s/i.test(diluentLabel)) return diluentLabel;
  var prefix = cfg.circuit === 'pSCR' ? 'pSCR' : 'CCR';
  return "".concat(prefix, " ").concat(diluentLabel);
}
/** Read SAC from DOM; always returns L/min (converts cu ft/min when units === 'imperial'). */
function sacDomToLpm(inputId, defaultLpm) {
  var _document$getElementB18;
  var raw = parseFloat((_document$getElementB18 = document.getElementById(inputId)) === null || _document$getElementB18 === void 0 ? void 0 : _document$getElementB18.value);
  if (!Number.isFinite(raw)) return defaultLpm;
  if (raw <= 0) return 0;
  return units === 'imperial' ? raw / CUFT_PER_LITRE : raw;
}
function ccrGasLitres(label, depthM, durMin, sac) {
  if (isCcrDiluentGasLabel(label)) {
    var pAmb = altSurfaceP + depthM * BAR_PER_METRE;
    var pSurf = altSurfaceP || 1.01325;
    var surfLpm = ccrDiluentSurfaceLpm(depthM);
    if (!Number.isFinite(surfLpm)) return NaN;
    return surfLpm * (pAmb / pSurf) * durMin;
  }
  var absP = altSurfaceP + depthM * BAR_PER_METRE;
  return sac * absP * durMin; // sac must be L/min → litres at ambient
}
function accumGasLitres(acc, label, litres) {
  if (!Number.isFinite(litres)) {
    acc[label] = NaN;
    return;
  }
  var prev = acc[label];
  if (prev === undefined || prev === null) {
    acc[label] = litres;
    return;
  }
  acc[label] = Number.isFinite(prev) ? prev + litres : NaN;
}
function setCircuitMode(mode, btn) {
  var _appSettings, _appSettings$save;
  var sel = document.getElementById('circuitSelect');
  if (sel) sel.value = mode;
  document.querySelectorAll('#circuitBtnGroup .circ-btn').forEach(function (b) {
    b.classList.toggle('active', b === btn || b.getAttribute('data-circuit') === mode);
  });
  toggleCircuitFields();
  if (window._lastContingency) calcContingency();
  calcCNS();
  (_appSettings = appSettings) === null || _appSettings === void 0 || (_appSettings$save = _appSettings.save) === null || _appSettings$save === void 0 || _appSettings$save.call(_appSettings, false);
}
function _syncCircuitBtns() {
  var _document$getElementB19;
  var mode = ((_document$getElementB19 = document.getElementById('circuitSelect')) === null || _document$getElementB19 === void 0 ? void 0 : _document$getElementB19.value) || 'OC';
  document.querySelectorAll('#circuitBtnGroup .circ-btn').forEach(function (b) {
    b.classList.toggle('active', b.getAttribute('data-circuit') === mode);
  });
}
function toggleCircuitFields() {
  var _document$getElementB20, _document$getElementB21;
  var circuit = ((_document$getElementB20 = document.getElementById('circuitSelect')) === null || _document$getElementB20 === void 0 ? void 0 : _document$getElementB20.value) || 'OC';
  var isRB = circuit !== 'OC';
  var isCCR = circuit === 'CCR';
  var isPSCR = circuit === 'pSCR';
  var advSec = document.getElementById('ccrAdvSettingsSection');
  if (advSec) advSec.style.display = isRB ? '' : 'none';
  var ccrGrp = document.getElementById('ccrSettingsGroup');
  if (ccrGrp) ccrGrp.style.display = isRB ? '' : 'none';
  var boGrp = document.getElementById('ccrBailoutSettingsGroup');
  var bailoutOn = ((_document$getElementB21 = document.getElementById('ccrBailoutToggle')) === null || _document$getElementB21 === void 0 ? void 0 : _document$getElementB21.value) === 'on';
  if (boGrp) boGrp.style.display = isRB && bailoutOn ? '' : 'none';
  ['ccrSpDescentRow', 'ccrSpBottomRow', 'ccrSpDecoRow'].forEach(function (id) {
    var el = document.getElementById(id);
    if (el) el.style.display = isCCR ? '' : 'none';
  });
  var bailoutEl = document.getElementById('ccrBailoutRow');
  if (bailoutEl) bailoutEl.style.display = isRB ? '' : 'none';
  if (isRB) _updateCcrAdvSummary();
  updateCcrGasCardLabels();
  updateCcrGasValidation();
}
function isCcrGasUiMode() {
  var _document$getElementB22;
  var c = ((_document$getElementB22 = document.getElementById('circuitSelect')) === null || _document$getElementB22 === void 0 ? void 0 : _document$getElementB22.value) || 'OC';
  return c === 'CCR' || c === 'pSCR';
}
function updateCcrGasCardLabels() {
  var _document$getElementB23;
  var ccrUi = isCcrGasUiMode();
  var isCCR = (((_document$getElementB23 = document.getElementById('circuitSelect')) === null || _document$getElementB23 === void 0 ? void 0 : _document$getElementB23.value) || 'OC') === 'CCR';
  var titleEl = document.getElementById('diluentCardTitle');
  if (titleEl) {
    titleEl.textContent = ccrUi ? 'Diluent' : 'Bottom Gas';
  }
  var dilBailoutRow = document.getElementById('diluentUseAsBailoutRow');
  if (dilBailoutRow) dilBailoutRow.style.display = isCCR ? '' : 'none';
  var mixLbl = document.getElementById('diluentMixLabel');
  if (mixLbl) mixLbl.textContent = ccrUi ? 'Diluent mix' : 'Gas Mix';
  var travelBtn = document.getElementById('addTravelGasBtn');
  var travelCard = document.getElementById('travelGasCard');
  if (ccrUi) {
    if (travelBtn) travelBtn.style.display = 'none';
    if (travelCard) travelCard.style.display = 'none';
  } else if (travelBtn && (!travelCard || travelCard.style.display === 'none')) {
    travelBtn.style.display = '';
  }
  var addBtn = document.getElementById('addDecoGasBtn');
  if (addBtn) addBtn.textContent = ccrUi ? 'ADD BAILOUT MIX' : 'ADD DECO GAS';
  var gpBotTitle = document.getElementById('gpBotRowTitle');
  if (gpBotTitle) {
    gpBotTitle.textContent = ccrUi ? '🔵 DILUENT CYLINDER' : '🔵 BOTTOM GAS CYLINDER';
  }
  renumberDecoGasCards();
}
function calcGasMODm(fO2, ppO2Limit) {
  var _document$getElementB24;
  var o2AtMOD = typeof allowO2AtMOD !== 'undefined' ? allowO2AtMOD : true;
  var lastStop = parseInt((_document$getElementB24 = document.getElementById('lastDecoStop')) === null || _document$getElementB24 === void 0 ? void 0 : _document$getElementB24.value, 10) || 3;
  var o2MODm = Math.max(lastStop, 6);
  if (fO2 <= 0) return 0;
  if (fO2 >= 0.995) {
    if (o2AtMOD) return o2MODm;
    var strictM = Math.max(0, Math.floor((ppO2Limit / fO2 - altSurfaceP) / BAR_PER_METRE));
    return Math.max(o2MODm, strictM);
  }
  return Math.max(0, Math.floor((ppO2Limit / fO2 - altSurfaceP) / BAR_PER_METRE));
}
function getBailoutPpo2Limit() {
  var _document$getElementB25;
  return parseFloat((_document$getElementB25 = document.getElementById('ppo2Bottom')) === null || _document$getElementB25 === void 0 ? void 0 : _document$getElementB25.value) || 1.4;
}
function getConfiguredBailoutMixes() {
  var ocPpo2 = getBailoutPpo2Limit();
  var mixes = [];
  var ids = getAllDecoGasIds();
  ids.forEach(function (cidx) {
    var sel = document.getElementById("dg".concat(cidx, "Mix"));
    if (!sel || sel.value === 'none' || !sel.value) return;
    var fracs = getDecoCardFractions(cidx);
    if (!fracs) return;
    mixes.push({
      cidx: cidx,
      label: getGasLabel(fracs.fO2, fracs.fHe),
      fO2: fracs.fO2,
      modM: calcGasMODm(fracs.fO2, ocPpo2)
    });
  });
  return mixes;
}

/** Richest breathable bailout mix for stress/problem-solve reserve at depth. */
function getBailoutReserveMixLabel(depthM, fO2Bot, fHeBot) {
  var _document$getElementB26;
  var mixes = getConfiguredBailoutMixes();
  var atDepth = mixes.filter(function (m) {
    return m.modM >= depthM - 0.01;
  });
  if (atDepth.length) {
    return atDepth.reduce(function (best, m) {
      return !best || m.modM < best.modM ? m : best;
    }).label;
  }
  var useDil = ((_document$getElementB26 = document.getElementById('diluentUseAsBailout')) === null || _document$getElementB26 === void 0 ? void 0 : _document$getElementB26.value) === 'on';
  if (useDil) {
    var bailoutPpo2 = getBailoutPpo2Limit();
    if (calcGasMODm(fO2Bot, bailoutPpo2) >= depthM - 0.01) {
      return getGasLabel(fO2Bot, fHeBot);
    }
  }
  return null;
}

/** Split stress/problem-solve reserve across bottom + deco stop depths with phase-appropriate bailout mix (BUG-70). */
function addBailoutStressReserve(addGasFn, reserveMin, bottomDepthM, stopDepthsM, fO2Bot, fHeBot, sacStress) {
  if (!reserveMin || reserveMin <= 0) return;
  var seen = new Set();
  var depths = [];
  [bottomDepthM].concat(_toConsumableArray(stopDepthsM || [])).forEach(function (d) {
    if (d == null || d < 0) return;
    var key = Math.round(d * 100);
    if (seen.has(key)) return;
    seen.add(key);
    depths.push(d);
  });
  if (!depths.length) return;
  var shareMin = reserveMin / depths.length;
  depths.forEach(function (d) {
    var label = getBailoutReserveMixLabel(d, fO2Bot, fHeBot);
    if (label) addGasFn(label, d, shareMin, sacStress);
  });
}
function validateCcrGasConfiguration() {
  var _getPlannerInputEl, _document$getElementB27, _document$getElementB28, _document$getElementB29;
  if (!isCcrGasUiMode()) return {
    ok: true,
    errors: [],
    warnings: []
  };
  var rawD = parseFloat((_getPlannerInputEl = getPlannerInputEl('decoDepth')) === null || _getPlannerInputEl === void 0 ? void 0 : _getPlannerInputEl.value) || 0;
  var depthM = units === 'metric' ? rawD : rawD / 3.28084;
  if (depthM <= 0) return {
    ok: true,
    errors: [],
    warnings: []
  };
  var circuit = ((_document$getElementB27 = document.getElementById('circuitSelect')) === null || _document$getElementB27 === void 0 ? void 0 : _document$getElementB27.value) || 'OC';
  var activePpo2 = circuit === 'pSCR' ? getBailoutPpo2Limit() : parseFloat((_document$getElementB28 = document.getElementById('ccrBottomSetpoint')) === null || _document$getElementB28 === void 0 ? void 0 : _document$getElementB28.value) || parseFloat((_document$getElementB29 = document.getElementById('ppo2Bottom')) === null || _document$getElementB29 === void 0 ? void 0 : _document$getElementB29.value) || 1.4;
  var bot = getBottomGasFractions();
  var du = units === 'imperial' ? 'ft' : 'm';
  var depthDisp = units === 'imperial' ? Math.round(depthM * 3.28084) : Math.round(depthM);
  var errors = [];
  var warnings = [];
  var gasFrac = validateDomDecoGases();
  if (!gasFrac.ok) {
    gasFrac.errors.forEach(function (e) {
      return errors.push(e.message || String(e));
    });
  }
  if (!bot) {
    errors.push('Invalid bottom gas configuration.');
  } else {
    var _document$getElementB30;
    var diluentMod = calcGasMODm(bot.fO2, activePpo2);
    var modDisp = units === 'imperial' ? Math.round(diluentMod * 3.28084) : diluentMod;
    if (depthM > diluentMod + 0.01) {
      errors.push("Diluent MOD (".concat(modDisp).concat(du, ") is shallower than dive depth (").concat(depthDisp).concat(du, "). Use a leaner diluent or reduce depth."));
    }
    var useDiluentBailout = ((_document$getElementB30 = document.getElementById('diluentUseAsBailout')) === null || _document$getElementB30 === void 0 ? void 0 : _document$getElementB30.value) === 'on';
    var bailoutMixes = getConfiguredBailoutMixes();
    var bailoutAtDepth = bailoutMixes.filter(function (m) {
      return m.modM >= depthM - 0.01;
    });
    if (useDiluentBailout && diluentMod >= depthM - 0.01) {
      // diluent cylinder counts as bailout at depth
    } else if (bailoutAtDepth.length === 0) {
      if (bailoutMixes.length === 0 && !useDiluentBailout) {
        errors.push('Add at least one bailout mix with sufficient MOD, or enable "Use diluent as bailout".');
      } else {
        errors.push("No bailout mix with enough MOD for ".concat(depthDisp).concat(du, ". Add or correct bailout mixtures."));
      }
    }
  }
  if (errors.length === 0) {
    var _document$getElementB31;
    var _useDiluentBailout = ((_document$getElementB31 = document.getElementById('diluentUseAsBailout')) === null || _document$getElementB31 === void 0 ? void 0 : _document$getElementB31.value) === 'on';
    var _bailoutMixes = getConfiguredBailoutMixes();
    if (_bailoutMixes.length === 0 && !_useDiluentBailout) {
      warnings.push('No bailout mixes configured — add OC bailout cylinders for emergency open-circuit use.');
    }
  }
  return {
    ok: errors.length === 0,
    errors: errors,
    warnings: warnings
  };
}
function updateCcrGasValidation() {
  var box = document.getElementById('ccrGasValidation');
  if (!box) return;
  if (!isCcrGasUiMode()) {
    box.style.display = 'none';
    box.innerHTML = '';
    return;
  }
  var v = validateCcrGasConfiguration();
  box.style.display = '';
  var html = '';
  v.errors.forEach(function (msg) {
    html += "<div class=\"alert dang\" style=\"margin-top:0;\"><span>\u26A0</span><div><strong>Validation error.</strong> ".concat(msg, "</div></div>");
  });
  v.warnings.forEach(function (msg) {
    html += "<div class=\"alert\" style=\"margin-top:6px;background:var(--card2);\"><span>\u2139</span><div>".concat(msg, "</div></div>");
  });
  if (v.ok && v.warnings.length === 0) {
    html = "<div class=\"alert\" style=\"margin-top:0;background:rgba(38,208,124,0.08);border-color:var(--green);\"><span>\u2713</span><div>Diluent and bailout MOD check passed for planned depth.</div></div>";
  }
  box.innerHTML = html;
}
function toggleCcrAdvSettings(forceOpen) {
  var body = document.getElementById('ccrAdvSettingsBody');
  var chevron = document.getElementById('ccrAdvSettingsChevron');
  var btn = document.getElementById('ccrAdvSettingsToggle');
  if (!body) return;
  var isOpen = body.style.display !== 'none';
  var open = forceOpen !== undefined ? forceOpen : !isOpen;
  body.style.display = open ? '' : 'none';
  if (chevron) chevron.style.transform = open ? 'rotate(90deg)' : '';
  if (btn) {
    btn.style.borderColor = open ? 'var(--accent)' : 'var(--border)';
    btn.style.color = open ? 'var(--accent)' : 'var(--muted)';
  }
  _updateCcrAdvSummary();
  try {
    localStorage.setItem('lspCcrAdvOpen', open ? '1' : '0');
  } catch (e) {}
}
function _updateCcrAdvSummary() {
  var _document$getElementB32, _document$getElementB36;
  var el = document.getElementById('ccrAdvSettingsSummary');
  if (!el) return;
  var body = document.getElementById('ccrAdvSettingsBody');
  if (body && body.style.display !== 'none') {
    el.textContent = '';
    return;
  }
  var circuit = ((_document$getElementB32 = document.getElementById('circuitSelect')) === null || _document$getElementB32 === void 0 ? void 0 : _document$getElementB32.value) || 'OC';
  if (circuit === 'OC') {
    el.textContent = '';
    return;
  }
  var parts = [];
  if (circuit === 'CCR') {
    var _document$getElementB33, _document$getElementB34, _document$getElementB35;
    var ds = (_document$getElementB33 = document.getElementById('ccrDescentSetpoint')) === null || _document$getElementB33 === void 0 ? void 0 : _document$getElementB33.value;
    var bs = (_document$getElementB34 = document.getElementById('ccrBottomSetpoint')) === null || _document$getElementB34 === void 0 ? void 0 : _document$getElementB34.value;
    var dec = (_document$getElementB35 = document.getElementById('ccrDecoSetpoint')) === null || _document$getElementB35 === void 0 ? void 0 : _document$getElementB35.value;
    if (ds && bs && dec) parts.push("SP ".concat(ds, "/").concat(bs, "/").concat(dec));
  }
  var bo = (_document$getElementB36 = document.getElementById('ccrBailoutToggle')) === null || _document$getElementB36 === void 0 ? void 0 : _document$getElementB36.value;
  if (bo === 'on') {
    var _document$getElementB37, _document$getElementB38;
    var glo = ((_document$getElementB37 = document.getElementById('ccrBailoutGfLow')) === null || _document$getElementB37 === void 0 ? void 0 : _document$getElementB37.value) || '50';
    var ghi = ((_document$getElementB38 = document.getElementById('ccrBailoutGfHigh')) === null || _document$getElementB38 === void 0 ? void 0 : _document$getElementB38.value) || '85';
    parts.push("BO GF ".concat(glo, "/").concat(ghi));
  }
  el.textContent = parts.length ? parts.join(' · ') : 'defaults';
}
if (typeof window !== 'undefined') {
  window.getInspiredInertPressures = getInspiredInertPressures;
  window.getCCRSettingsFromDOM = getCCRSettingsFromDOM;
  window.mergeCCRSettings = mergeCCRSettings;
  window.getEffectiveSetpointAtDepth = getEffectiveSetpointAtDepth;
  window.getEffectivePpo2 = getEffectivePpo2;
  window.loadTissuesWithCCR = loadTissuesWithCCR;
  window.splitSegmentAtSetpoint = splitSegmentAtSetpoint;
  window.computePSCRFractions = computePSCRFractions;
  window.isCcrOnLoopProfile = isCcrOnLoopProfile;
  window.loopMixLabelFor = loopMixLabelFor;
  window.toggleCcrAdvSettings = toggleCcrAdvSettings;
  window.updateCcrGasCardLabels = updateCcrGasCardLabels;
  window.updateCcrGasValidation = updateCcrGasValidation;
  window.validateCcrGasConfiguration = validateCcrGasConfiguration;
}

// AUDIT-UNIT:UI-DECO-PHYSICS
function ceiling(tissues, gfHigh) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.ceiling(tissues, gfHigh);
}

// ── Decozone start (GF-INDEPENDENT) ──────────────────────────────────────────
// This is NOT a Bühlmann M-value/ceiling calculation. It's the purely physical
// depth where any tissue's total inert-gas tension (pN2+pHe) first exceeds the
// surrounding ambient pressure — the point where the diver becomes supersaturated
// in the loosest sense, before any conservatism (GF) is applied. Gradient factors
// move the M-value line, not the ambient line, so this value must not vary with
// GF Lo/Hi — same dive at GF30/70 and GF50/80 must report the same decozone start.
// Matches MultiDeco/DiveKit's published "decozone" definition exactly.
// Surface GF — maximum tissue loading ratio vs M-value at surface pressure.
// Formula (tl5915 / Baker): surfGF_i = (pN2+pHe - P_surf) / (a_i + P_surf/b_i)
// where P_surf = altSurfaceP (actual surface ambient — altitude-aware). Returns 0–100+ as a percentage.
// Negative values (all tissues undersaturated at surface) are clamped to 0.
function computeSurfaceGF(tissues) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.computeSurfaceGF(tissues);
}
function ambientCrossingDepth(tissues) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.ambientCrossingDepth(tissues);
}
function parseDomInt(id, fallback) {
  var _document$getElementB39;
  var raw = parseInt((_document$getElementB39 = document.getElementById(id)) === null || _document$getElementB39 === void 0 ? void 0 : _document$getElementB39.value, 10);
  return Number.isNaN(raw) ? fallback : raw;
}
function isShallowGradientOn() {
  var _document$getElementB40;
  return ((_document$getElementB40 = document.getElementById('shallowGradient')) === null || _document$getElementB40 === void 0 ? void 0 : _document$getElementB40.value) === 'on';
}
function buhNDL(depthM, fN2, gfLow, gfHigh, fHe) {
  _syncZhlBundleEnv();
  var lastStop = parseDomInt('lastDecoStop', 3);
  var decoStep = parseDomInt('decoStep', 3);
  return ZhlEngineBundle.buhNDL(depthM, fN2, gfLow, gfHigh, fHe, lastStop, decoStep, isShallowGradientOn());
}
function maxSatPct(tissues, gfHigh) {
  var max = 0;
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
    var mv = gfAdjustedMValue(a, b, altSurfaceP, gfHigh / 100);
    var pct = Math.round(pTotal / mv * 100);
    if (pct > max) max = pct;
  });
  return max;
}

// ═══════════════════════════════════════════════
// PLANNER
// ═══════════════════════════════════════════════
function runPlanner() {
  var _document$getElementB41, _document$getElementB42, _document$getElementB43, _schedulePlannerProfi;
  if (!guardEngineBootForCalculate()) return;
  if (!window._zhlHeadless) {
    var inputVal = validatePlannerInputs();
    if (!inputVal.ok) {
      alert('Cannot calculate dive:\n\n' + inputVal.msg);
      return;
    }
  }
  var isMetric = units === 'metric';
  var rawD = parseFloat((_document$getElementB41 = document.getElementById('recDepth')) === null || _document$getElementB41 === void 0 ? void 0 : _document$getElementB41.value) || 30;
  var depthM = isMetric ? rawD : rawD / 3.28084;
  var bt = parseInt((_document$getElementB42 = document.getElementById('recBT')) === null || _document$getElementB42 === void 0 ? void 0 : _document$getElementB42.value) || 25;
  var gfL = mGF.low;
  var gfH = mGF.high;
  var mix = getPersistedGasMix();
  var modPpo2 = parseFloat((_document$getElementB43 = document.getElementById('ppo2Bottom')) === null || _document$getElementB43 === void 0 ? void 0 : _document$getElementB43.value) || 1.4;
  var fN2 = getN2Frac(mix);
  if (fN2 == null) {
    if (!window._zhlHeadless) alert('Invalid gas mix — check O₂ and He percentages.');
    return;
  }
  var fHe = getHeFrac(mix);
  var dDisp = isMetric ? rawD + ' m' : rawD + ' ft';
  var stopFt = stopDepthM === 3 ? 10 : 20;
  var html = '';
  var summarySurfGF = '—';
  var recFO2 = Math.max(0, 1 - fN2 - fHe);
  var recGasLabel = mix === 'ean32' ? '32/00' : mix === 'ean36' ? '36/00' : 'Air';
  var recModM = nitroxMOD(recFO2, modPpo2);
  var recModFt = recModM !== null ? Math.floor(recModM * 3.28084) : null;
  var resultHintHtml = recModM !== null ? "<div class=\"alert info result-inline-hint\"><span>\uD83D\uDCA1</span><div>MOD for ".concat(recGasLabel, " @ ").concat(modPpo2.toFixed(1), " bar ppO\u2082: <strong>").concat(isMetric ? recModM + ' m' : recModFt + ' ft', "</strong></div></div>") : '';
  if (algo === 'padi') {
    var fO2 = PadiEngine.recMixFO2(mix);
    var isNitrox = mix !== 'air';
    var gasLabel = recGasLabel;
    var pO2 = parseFloat((depthBar(depthM) * fO2).toFixed(2));
    var ndl = padiNDL(depthM, mix);
    var rem = ndl > 0 ? Math.max(0, ndl - bt) : 0;
    var group = padiGroup(depthM, bt, mix);
    var pct = ndl > 0 ? Math.min(100, Math.round(bt / ndl * 100)) : 100;
    summarySurfGF = "".concat(pct, "%");
    var bc = pct >= 100 ? 'var(--red)' : pct >= 80 ? 'var(--orange)' : pct >= 65 ? 'var(--yellow)' : 'var(--green)';
    var modM = recModM;
    var modFt = modM !== null ? Math.floor(modM * 3.28084) : null;
    var beyondMOD = depthM > modM;
    var ppO2Ok = pO2 <= modPpo2;
    var btOk = ndl > 0 && bt <= ndl && !beyondMOD;
    if (beyondMOD || ndl === 0 || !btOk) {
      var blockMessage = beyondMOD ? "<strong>BEYOND MOD.</strong> ".concat(dDisp, " exceeds ").concat(gasLabel, " MOD of ").concat(isMetric ? modM + ' m' : modFt + ' ft', " at ").concat(modPpo2.toFixed(1), " bar ppO\u2082. Use Air or a lower O\u2082 mix, or reduce depth.") : ndl === 0 ? "<strong>BEYOND TABLE.</strong> ".concat(dDisp, " exceeds the PADI recreational table maximum (").concat(isMetric ? PADI_TABLE_MAX_M + ' m' : PADI_TABLE_MAX_FT + ' ft', ").") : "<strong>NDL EXCEEDED.</strong> ".concat(bt, " min exceeds the ").concat(ndl, " min no-decompression limit. Reduce bottom time by ").concat(bt - ndl, " min.");
      var plannerResult = document.getElementById('plannerResult');
      if (plannerResult) {
        plannerResult.innerHTML = "<div class=\"card rec-block-card\">\n          <div class=\"card-title\" style=\"margin:0 0 10px;\">REC Plan Blocked</div>\n          <div class=\"alert dang\" style=\"margin:0;\"><span>\u26A0</span><div>".concat(blockMessage, "</div></div>\n        </div>");
        plannerResult.style.display = 'block';
      }
      _renderResultSummaryStrip({
        mode: 'rec',
        runTime: String(bt),
        firstStop: 'Blocked'
      });
      _onPlanResultsReady();
      renderSurfIntPanel('recSurfIntContainer', 'recSi', depthM, bt);
      return;
    }
    var gasStatHtml = "<div class=\"stat\"><div class=\"stat-val ".concat(ppO2Ok ? 'g' : 'r', "\">").concat(pO2.toFixed(2), "</div><div class=\"stat-lbl\">ppO\u2082 (bar)</div></div>");
    var tableRef = isNitrox ? "PADI Nitrox ".concat(gasLabel) : 'PADI Air';
    var recStatusAlertHtml = pct >= 80 ? '<div class="alert" style="margin:0 0 12px;background:#ffff00;border-color:#cccc00;color:#111;font-weight:700;"><span>⚡</span><div><strong>APPROACHING LIMIT.</strong> 80%+ NDL used. Monitor closely and ascend conservatively.</div></div>' : '<div class="alert ok" style="margin:0 0 12px;"><span>✓</span><div><strong>WITHIN LIMITS.</strong> ' + rem + ' minutes remaining. Good safety margin.</div></div>';
    html = "<div class=\"card\">\n      <div style=\"display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;\">\n        <div class=\"card-title\" style=\"margin:0;\">Dive Profile</div>\n        <div class=\"export-row\">\n          <button class=\"btn-export\" onclick=\"copyDiveProfile('planner')\" title=\"Copy to clipboard\"><svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"9\" y=\"9\" width=\"13\" height=\"13\" rx=\"2\"/><path d=\"M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1\"/></svg></button>\n          <button class=\"btn-export\" onclick=\"exportTXT('planner')\" title=\"Download .txt\"><svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><polyline points=\"14 2 14 8 20 8\"/><line x1=\"16\" y1=\"13\" x2=\"8\" y2=\"13\"/><line x1=\"16\" y1=\"17\" x2=\"8\" y2=\"17\"/><line x1=\"10\" y1=\"9\" x2=\"8\" y2=\"9\"/></svg></button>\n        </div>\n      </div>\n      ".concat(recStatusAlertHtml, "\n      <div class=\"stats rec-profile-stats\" style=\"grid-template-columns:repeat(5,1fr);margin-top:4px;margin-bottom:4px;\">\n        <div class=\"stat\"><div class=\"stat-val u10\">").concat(isMetric ? rawD : Math.round(depthM * 3.28084), "<span class=\"unit\">").concat(isMetric ? 'm' : 'ft', "</span></div><div class=\"stat-lbl\">Depth</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(btOk ? 'g' : 'r', "\">").concat(bt, "</div><div class=\"stat-lbl\">BT Time</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(rem === 0 ? 'r' : rem < 10 ? 'o' : 'g', "\">").concat(rem, "<span class=\"unit\">min</span></div><div class=\"stat-lbl\">No NDL</div></div>\n        <div class=\"stat\"><div class=\"stat-val\" style=\"font-size:22px;\"><span class=\"group-badge\">").concat(group, "</span></div><div class=\"stat-lbl\">Press. Group</div></div>\n        ").concat(gasStatHtml, "\n      </div>\n      ").concat(resultHintHtml ? "<div class=\"rec-card-hint-row\">".concat(resultHintHtml, "</div>") : '', "\n      <div style=\"margin-top:14px;\">\n        <div style=\"display:flex;justify-content:space-between;font-family:var(--font-mono);font-size:10px;color:var(--muted);margin-bottom:5px;letter-spacing:1px;\"><span>NDL USAGE \xB7 ").concat(tableRef, "</span><span>").concat(pct, "%</span></div>\n        <div class=\"bar-wrap\"><div class=\"bar-fill\" style=\"width:").concat(pct, "%;background:").concat(bc, ";\"></div></div>\n      </div>\n      ").concat(beyondMOD ? "<div class=\"alert dang\" style=\"margin-top:12px;\"><span>\u26A0</span><div><strong>BEYOND MOD.</strong> ".concat(dDisp, " exceeds ").concat(gasLabel, " MOD of ").concat(isMetric ? modM + ' m' : modFt + ' ft', " at ").concat(modPpo2.toFixed(1), " bar ppO\u2082. CNS oxygen toxicity risk \u2014 use a lower O\u2082 mix or reduce depth.</div></div>") : '', "\n      ").concat(!beyondMOD && ndl === 0 ? "<div class=\"alert dang\" style=\"margin-top:12px;\"><span>\u26A0</span><div><strong>BEYOND TABLE.</strong> ".concat(dDisp, " exceeds the PADI recreational table maximum (").concat(isMetric ? PADI_TABLE_MAX_M + ' m' : PADI_TABLE_MAX_FT + ' ft', ").</div></div>") : '', "\n      ").concat(!beyondMOD && ndl > 0 && !btOk ? '<div class="alert" style="margin-top:12px;background:#FF4433;border-color:#cc2200;color:#fff;font-weight:700;"><span>⚠</span><div><strong>NDL EXCEEDED.</strong> Not permitted for recreational diving. Reduce bottom time.</div></div>' : '', "\n      ").concat(safetyStopHTML(stopDepthM, stopFt, stopDurMin), "\n    </div>");
  } else {
    var _document$getElementB44;
    // Bühlmann — BT includes descent; saturate linearly on descent, then hold at depth
    var tissues = initTissues();
    var descentRate = Math.max(1, parseInt((_document$getElementB44 = document.getElementById('descentRate')) === null || _document$getElementB44 === void 0 ? void 0 : _document$getElementB44.value, 10) || 20);
    var descentTime = depthM / descentRate;
    var btAtDepth = Math.max(0, bt - descentTime);
    if (descentTime > 0 && depthM > 0) {
      tissues = saturateLinear(tissues, 0, depthM, descentTime, fN2, fHe);
    }
    tissues = saturate(tissues, depthM, btAtDepth, fN2, fHe);
    lastTissues = tissues;
    var gfHF = gfH / 100;
    var _ndl = buhNDL(depthM, fN2, gfL, gfH, fHe);
    var ceil = ceiling(tissues, gfHF);
    var sat = maxSatPct(tissues, gfH);
    var _rem = Math.max(0, _ndl - bt);
    var _pct = Math.min(100, Math.round(bt / _ndl * 100));
    summarySurfGF = "".concat(_pct, "%");
    var _bc = _pct >= 100 ? 'var(--red)' : _pct >= 85 ? 'var(--orange)' : _pct >= 70 ? 'var(--yellow)' : 'var(--green)';
    var _btOk = bt <= _ndl && ceil <= 0;
    var _fO2 = Math.max(0, 1 - fN2 - fHe);
    var _pO = parseFloat((depthBar(depthM) * _fO2).toFixed(2));
    var _beyondMOD = _pO > modPpo2;
    var ndlExceededNoDeco = !_btOk && ceil <= 0;
    updateTissueViz(tissues, gfH);

    // Match PADI blocked layout for beyond-MOD / NDL-exceeded (no deco ceiling).
    if (_beyondMOD || ndlExceededNoDeco) {
      var _blockMessage = _beyondMOD ? "<strong>BEYOND MOD.</strong> ".concat(dDisp, " exceeds ").concat(recGasLabel, " MOD of ").concat(isMetric ? recModM + ' m' : recModFt + ' ft', " at ").concat(modPpo2.toFixed(1), " bar ppO\u2082. Use a lower O\u2082 mix or reduce depth.") : "<strong>NDL EXCEEDED.</strong> ".concat(bt, " min exceeds the ").concat(_ndl, " min no-decompression limit. Reduce bottom time by ").concat(Math.max(0, bt - _ndl), " min.");
      var _plannerResult = document.getElementById('plannerResult');
      if (_plannerResult) {
        _plannerResult.innerHTML = "<div class=\"card rec-block-card\">\n          <div class=\"card-title\" style=\"margin:0 0 10px;\">REC Plan Blocked</div>\n          <div class=\"alert dang\" style=\"margin:0;\"><span>\u26A0</span><div>".concat(_blockMessage, "</div></div>\n        </div>");
        _plannerResult.style.display = 'block';
      }
      _renderResultSummaryStrip({
        mode: 'rec',
        runTime: String(bt),
        firstStop: 'Blocked'
      });
      _onPlanResultsReady();
      renderSurfIntPanel('recSurfIntContainer', 'recSi', depthM, bt);
      return;
    }
    html = "<div class=\"card\">\n      <div style=\"display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;\">\n        <div class=\"card-title\" style=\"margin:0;\">B\xFChlmann \xB7 ".concat(dDisp, " / ").concat(bt, " min \xB7 GF ").concat(gfL, "/").concat(gfH, "</div>\n        <div class=\"export-row\">\n          <button class=\"btn-export\" onclick=\"copyDiveProfile('planner')\" title=\"Copy to clipboard\"><svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><rect x=\"9\" y=\"9\" width=\"13\" height=\"13\" rx=\"2\"/><path d=\"M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1\"/></svg></button>\n          <button class=\"btn-export\" onclick=\"exportTXT('planner')\" title=\"Download .txt\"><svg width=\"16\" height=\"16\" viewBox=\"0 0 24 24\" fill=\"none\" stroke=\"currentColor\" stroke-width=\"2\" stroke-linecap=\"round\" stroke-linejoin=\"round\"><path d=\"M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z\"/><polyline points=\"14 2 14 8 20 8\"/><line x1=\"16\" y1=\"13\" x2=\"8\" y2=\"13\"/><line x1=\"16\" y1=\"17\" x2=\"8\" y2=\"17\"/><line x1=\"10\" y1=\"9\" x2=\"8\" y2=\"9\"/></svg></button>\n        </div>\n      </div>\n      <div class=\"stats\" style=\"grid-template-columns:repeat(5,1fr);margin-top:4px;margin-bottom:4px;\">\n        <div class=\"stat\"><div class=\"stat-val ").concat(_btOk ? 'g' : 'r', "\">").concat(bt, "</div><div class=\"stat-lbl\">Your BT</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(_rem === 0 ? 'r' : _rem < 10 ? 'o' : 'g', "\">").concat(_rem, "</div><div class=\"stat-lbl\">Remaining</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(ceil > 0 ? 'r' : 'g', "\">").concat(ceil > 0 ? Math.ceil(ceil) + ' m' : '0 m', "</div><div class=\"stat-lbl\">Deco Ceil</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(sat >= 100 ? 'r' : sat >= 85 ? 'o' : sat >= 70 ? 'y' : 'g', "\">").concat(sat, "%</div><div class=\"stat-lbl\">Max Sat</div></div>\n        <div class=\"stat\"><div class=\"stat-val ").concat(_pO > modPpo2 ? 'r' : 'g', "\">").concat(_pO.toFixed(1), "</div><div class=\"stat-lbl\">ppO\u2082</div></div>\n      </div>\n      <div style=\"margin-top:14px;\">\n        <div style=\"display:flex;justify-content:space-between;font-family:var(--font-mono);font-size:10px;color:var(--muted);margin-bottom:5px;letter-spacing:1px;\"><span>NDL USAGE</span><span>").concat(_pct, "%</span></div>\n        <div class=\"bar-wrap\"><div class=\"bar-fill\" style=\"width:").concat(_pct, "%;background:").concat(_bc, ";\"></div></div>\n      </div>\n      ").concat(ceil > 0 ? '<div class="alert deco" style="margin-top:12px;"><span>⚠</span><div><strong>DECOMPRESSION REQUIRED.</strong> Deco ceiling at ' + Math.ceil(ceil) + ' m. See Deco Schedule tab for full profile.</div></div>' : '', "\n      ").concat(_btOk ? '<div class="alert ok" style="margin-top:12px;"><span>✓</span><div><strong>WITHIN NDL.</strong> GF ' + gfL + '/' + gfH + '. ' + _rem + ' minutes remaining.</div></div>' : '', "\n      <div style=\"margin-top:8px;\"><div class=\"alert info\" style=\"margin-bottom:0;\"><span>\uD83D\uDCA1</span><div>Tissue saturation chart updated \u2014 see <strong>Tissue Sat.</strong> tab.</div></div></div>\n      ").concat(safetyStopHTML(stopDepthM, stopFt, stopDurMin), "\n    </div>");
  }
  document.getElementById('plannerResult').innerHTML = html;
  document.getElementById('plannerResult').style.display = 'block';
  (_schedulePlannerProfi = schedulePlannerProfileDraw) === null || _schedulePlannerProfi === void 0 || _schedulePlannerProfi();
  var stopDisp = isMetric ? "".concat(stopDepthM, "m") : "".concat(stopFt, "ft");
  _renderResultSummaryStrip({
    mode: 'rec',
    runTime: String(bt),
    decoTime: '0',
    cns: '—',
    firstStop: stopDisp,
    hintHtml: resultHintHtml,
    profileChip: "".concat(dDisp, " / ").concat(bt, " min"),
    surfaceGF: summarySurfGF,
    otu: '—',
    tts: '—',
    decozone: '—'
  });
  _onPlanResultsReady();

  // Surface Interval panel (collapsed) — pre-fill Dive 1 from this dive
  renderSurfIntPanel('recSurfIntContainer', 'recSi', depthM, bt);
}
function safetyStopHTML(depthM, depthFt, dur) {
  var isMetric = units === 'metric';
  var depthDisp = isMetric ? "".concat(depthM, " m") : "".concat(depthFt, " ft");
  var rateDisp = isMetric ? '9 m/min' : '30 ft/min';
  var desc = depthM === 3 ? "".concat(depthDisp, " standard safety stop. Ascend at max ").concat(rateDisp, " and hold for ").concat(dur, " min.") : depthM === 6 ? "".concat(depthDisp, " deep stop: recommended for dives below 30 m. Follow with a standard 3 m stop before surfacing.") : "".concat(depthDisp, " safety stop. Ascend at max ").concat(rateDisp, " and hold for ").concat(dur, " min.");
  return "<div style=\"background:rgba(38,208,124,0.06);border:1px solid rgba(38,208,124,0.2);border-radius:10px;padding:14px;margin-top:14px;\">\n    <div style=\"font-family:var(--font-display);font-size:13px;letter-spacing:3px;color:var(--green);margin-bottom:10px;\">SAFETY STOP</div>\n    <div class=\"stats\" style=\"margin-top:0;\">\n      <div class=\"stat\" style=\"padding:10px;\"><div class=\"stat-val g\" style=\"font-size:22px;\">".concat(depthDisp, "</div><div class=\"stat-lbl\">Safety Stop Depth</div></div>\n      <div class=\"stat\" style=\"padding:10px;\"><div class=\"stat-val g\" style=\"font-size:22px;\">").concat(dur, "</div><div class=\"stat-lbl\">Duration (min)</div></div>\n      <div class=\"stat\" style=\"padding:10px;\"><div class=\"stat-val g\" style=\"font-size:22px;\">").concat(rateDisp, "</div><div class=\"stat-lbl\">Max Ascent</div></div>\n    </div>\n    <div style=\"font-family:var(--font-mono);font-size:11px;color:var(--muted);margin-top:10px;line-height:1.5;\">").concat(desc, "</div>\n  </div>");
}

// ═══════════════════════════════════════════════
// NDL TABLE
// ═══════════════════════════════════════════════
// AUDIT-UNIT:UI-SCHEDULE-INPUTS
function renderNDLTable() {
  var _document$getElementB45;
  if (window._zhlHeadless) return; // skip during headless test execution
  var isM = ndlUnits === 'metric';
  var ranges = {
    shallow: [0, 1],
    moderate: [2, 3],
    deeper: [4, 6],
    deep: [7, 10]
  };
  var sel = document.getElementById('ndlRange').value;
  var _ranges$sel = _slicedToArray(ranges[sel], 2),
    from = _ranges$sel[0],
    to = _ranges$sel[1];
  var depths = isM ? PADI_DEPTHS_M.slice(from, to + 1) : PADI_DEPTHS_FT.slice(from, to + 1);
  var ndls = PADI_NDL_M.slice(from, to + 1);
  var isBuh = algo === 'buh';
  var tbody = document.getElementById('ndlBody');
  tbody.innerHTML = '';
  var modPpo2Ndl = parseFloat((_document$getElementB45 = document.getElementById('ppo2Bottom')) === null || _document$getElementB45 === void 0 ? void 0 : _document$getElementB45.value) || 1.4;
  var mod32 = nitroxMOD(0.32, modPpo2Ndl);
  var mod36 = nitroxMOD(0.36, modPpo2Ndl);
  depths.forEach(function (d, i) {
    var depthM = isM ? d : d / 3.28084;
    var ndl = ndls[i];
    var buhNDLVal = '';
    if (isBuh) {
      var fN2 = FN2_AIR;
      buhNDLVal = buhNDL(depthM, fN2, mGF.low, mGF.high);
    }
    var dUnit = isM ? ' m' : ' ft';
    var isDeep = depthM > 30;
    var isCaution = depthM > 21 && depthM <= 30;
    var rowClass = isDeep ? 'danger-row' : isCaution ? 'warn-row' : '';
    var ndlClass = isDeep ? 'td-r' : isCaution ? 'td-o' : 'td-g';
    var groupIdx = Math.min(PADI_GROUPS.length - 1, Math.floor(i * PADI_GROUPS.length / depths.length));
    var group = PADI_GROUPS[groupIdx];
    var beyondMOD32 = depthM > mod32;
    var beyondMOD36 = depthM > mod36;

    // EAN32 NDL cell
    var idx2 = PADI_DEPTHS_M.indexOf(PADI_DEPTHS_M.reduce(function (a, v) {
      return Math.abs(v - depthM) < Math.abs(a - depthM) ? v : a;
    }));
    var ndl32 = beyondMOD32 ? "<span style=\"color:var(--red);font-weight:700;\" title=\"Beyond MOD (".concat(isM ? mod32 + ' m' : Math.floor(mod32 * 3.28084) + ' ft', ")\">\u2014</span>") : "<span style=\"color:var(--green);\">".concat(NITROX_NDL_EAN32[idx2], " min</span>");
    var ndl36 = beyondMOD36 ? "<span style=\"color:var(--red);font-weight:700;\" title=\"Beyond MOD (".concat(isM ? mod36 + ' m' : Math.floor(mod36 * 3.28084) + ' ft', ")\">\u2014</span>") : "<span style=\"color:var(--accent);\">".concat(NITROX_NDL_EAN36[idx2], " min</span>");
    var ndlDisplay = isBuh ? "".concat(buhNDLVal >= 500 ? '500+' : buhNDLVal, " min <span style=\"color:var(--muted);font-size:10px;\">(Air: ").concat(ndl, ") GF ").concat(mGF.low, "/").concat(mGF.high, "</span>") : "".concat(ndl, " min");
    tbody.innerHTML += "<tr class=\"".concat(rowClass, "\">\n      <td><strong>").concat(d).concat(dUnit, "</strong></td>\n      <td class=\"").concat(ndlClass, "\">").concat(ndlDisplay, "</td>\n      <td>").concat(isBuh ? '—' : ndl32, "</td>\n      <td>").concat(isBuh ? '—' : ndl36, "</td>\n      <td>").concat(isBuh ? '—' : "<span class=\"group-badge\">".concat(group, "</span>"), "</td>\n      <td style=\"color:var(--muted);\">60+ min recommended</td>\n    </tr>");
  });
}

// ═══════════════════════════════════════════════
// TISSUE VIZ (Bühlmann)
// ═══════════════════════════════════════════════
function lspSatColors(pct) {
  var p = Math.round(pct);
  if (p >= 80) return {
    bar: 'var(--status-red)',
    text: 'var(--status-red)',
    status: p >= 100 ? 'LIMIT' : 'HIGH',
    level: 'warn'
  };
  if (p >= 50) return {
    bar: 'var(--status-orange)',
    text: 'var(--status-orange)',
    status: 'LOADED',
    level: 'caution'
  };
  return {
    bar: 'var(--status-green)',
    text: 'var(--status-green)',
    status: 'OK',
    level: 'safe'
  };
}
function updateTissueViz(tissues, gfH) {
  var gfF = gfH / 100;
  var grid = document.getElementById('tissueGrid');
  var tbody = document.getElementById('tissueTableBody');
  if (!grid) return;
  grid.innerHTML = '';
  if (tbody) tbody.innerHTML = '';
  tissues.forEach(function (t0, i) {
    var pN2 = t0.pN2;
    var pHe = t0.pHe || 0;
    var pTotal = pN2 + pHe;
    var _ZHL16C$i2 = _slicedToArray(ZHL16C[i], 3),
      ht = _ZHL16C$i2[0],
      a_n2 = _ZHL16C$i2[1],
      b_n2 = _ZHL16C$i2[2];
    var a, b;
    if (pHe > 0 && pTotal > 0) {
      a = (pN2 * a_n2 + pHe * ZHL16C_HE_AB[i][0]) / pTotal;
      b = (pN2 * b_n2 + pHe * ZHL16C_HE_AB[i][1]) / pTotal;
    } else {
      a = a_n2;
      b = b_n2;
    }
    var mv = gfAdjustedMValue(a, b, altSurfaceP, gfF);
    var pct = Math.min(100, Math.round(pTotal / mv * 100));
    var sat = lspSatColors(pct);
    var col = sat.bar;
    var status = sat.status;

    // Horizontal bar row
    var row = document.createElement('div');
    row.style.cssText = 'display:flex;align-items:center;gap:8px;margin-bottom:5px;';
    row.innerHTML = "\n      <span style=\"font-family:var(--font-mono);font-size:9px;color:var(--muted);min-width:24px;text-align:right;\">".concat(i + 1, "</span>\n      <div style=\"flex:1;background:var(--bg-alt);border-radius:3px;height:12px;overflow:hidden;\">\n        <div style=\"width:").concat(pct, "%;height:100%;background:").concat(col, ";border-radius:3px;transition:width 0.3s;\"></div>\n      </div>\n      <span style=\"font-family:var(--font-mono);font-size:10px;color:").concat(col, ";min-width:36px;text-align:right;\">").concat(pct, "%</span>");
    grid.appendChild(row);
    if (tbody) tbody.innerHTML += "<tr class=\"tissue-sat-".concat(sat.level, "\">\n      <td>").concat(i + 1, "</td><td>").concat(ht, "</td>\n      <td>").concat(pTotal.toFixed(3)).concat(pHe > 0 ? ' (' + pN2.toFixed(2) + '+' + pHe.toFixed(2) + ')' : '', "</td><td>").concat(mv.toFixed(3), "</td>\n      <td class=\"tissue-sat-pct\">").concat(pct, "%</td><td class=\"tissue-sat-status\">").concat(status, "</td>\n    </tr>");
  });
  // Show unified tissue card
  var ttc = document.getElementById('tissueLoadCard');
  if (ttc) ttc.style.display = 'block';
  var tic = document.getElementById('tissueInlineCard');
  if (tic) tic.style.display = 'none'; // old card suppressed
}

// ═══════════════════════════════════════════════
// DECO SCHEDULE
// ═══════════════════════════════════════════════
function toggleDecoCustomO2(selectId, fieldId) {
  var selEl = document.getElementById(selectId);
  var fieldEl = document.getElementById(fieldId);
  if (!selEl || !fieldEl) return;
  var isCustom = selEl.value === 'custom';
  fieldEl.style.display = isCustom ? 'block' : 'none';
}

// Toggle trimix He/O2 fields for bottom gas card
function toggleBottomTrimix() {
  var isTrimix = document.getElementById('decoGas').value === 'trimix';
  document.getElementById('botTrimixO2Field').style.display = isTrimix ? 'block' : 'none';
  document.getElementById('botTrimixHeField').style.display = isTrimix ? 'block' : 'none';
}

// Toggle trimix fields for a deco gas card (idx = card index)
function toggleDecoTrimix(idx) {
  var _document$getElementB46;
  var val = (_document$getElementB46 = document.getElementById("dg".concat(idx, "Mix"))) === null || _document$getElementB46 === void 0 ? void 0 : _document$getElementB46.value;
  var isTrimix = val === 'trimix';
  var o2f = document.getElementById("dg".concat(idx, "TrimixO2Field"));
  var hef = document.getElementById("dg".concat(idx, "TrimixHeField"));
  if (o2f) o2f.style.display = isTrimix ? 'block' : 'none';
  if (hef) hef.style.display = isTrimix ? 'block' : 'none';
}
function toggleTravelTrimix() {
  var _document$getElementB47;
  var isTrimix = ((_document$getElementB47 = document.getElementById('travelGasMix')) === null || _document$getElementB47 === void 0 ? void 0 : _document$getElementB47.value) === 'trimix';
  var o2f = document.getElementById('travelGasTrimixO2Field');
  var hef = document.getElementById('travelGasTrimixHeField');
  if (o2f) o2f.style.display = isTrimix ? 'block' : 'none';
  if (hef) hef.style.display = isTrimix ? 'block' : 'none';
}

// Read raw percent inputs from DOM without clamping (for validation only).
function readDomO2Pct(elementId) {
  var _document$getElementB48;
  var raw = (_document$getElementB48 = document.getElementById(elementId)) === null || _document$getElementB48 === void 0 ? void 0 : _document$getElementB48.value;
  if (raw === '' || raw == null) return NaN;
  var o2 = parseFloat(raw);
  return Number.isFinite(o2) && o2 > 0 ? o2 : NaN;
}
function readDomHePct(elementId) {
  var _document$getElementB49;
  var raw = (_document$getElementB49 = document.getElementById(elementId)) === null || _document$getElementB49 === void 0 ? void 0 : _document$getElementB49.value;
  if (raw === '' || raw == null) return NaN;
  var he = parseFloat(raw);
  return Number.isFinite(he) && he >= 0 ? he : NaN;
}
function getDomBottomGasPct() {
  var _document$getElementB50;
  var mix = (_document$getElementB50 = document.getElementById('decoGas')) === null || _document$getElementB50 === void 0 ? void 0 : _document$getElementB50.value;
  if (mix === 'air') return {
    o2: 21,
    he: 0
  };
  if (mix === 'ean32') return {
    o2: 32,
    he: 0
  };
  if (mix === 'ean36') return {
    o2: 36,
    he: 0
  };
  if (mix === 'custom') {
    return {
      o2: readDomO2Pct('decoCustomO2'),
      he: 0
    };
  }
  if (mix === 'trimix') {
    return {
      o2: readDomO2Pct('botTrimixO2'),
      he: readDomHePct('botTrimixHe')
    };
  }
  return {
    o2: 21,
    he: 0
  };
}
function getDomDecoGasPct(idx) {
  var _document$getElementB51;
  var mix = (_document$getElementB51 = document.getElementById("dg".concat(idx, "Mix"))) === null || _document$getElementB51 === void 0 ? void 0 : _document$getElementB51.value;
  if (!mix || mix === 'none') return null;
  if (mix === 'ean50') return {
    o2: 50,
    he: 0
  };
  if (mix === 'ean80') return {
    o2: 80,
    he: 0
  };
  if (mix === 'o2') return {
    o2: 100,
    he: 0
  };
  if (mix === 'custom') {
    return {
      o2: readDomO2Pct("dg".concat(idx, "CustomO2")),
      he: 0
    };
  }
  if (mix === 'trimix') {
    return {
      o2: readDomO2Pct("dg".concat(idx, "TrimixO2")),
      he: readDomHePct("dg".concat(idx, "TrimixHe"))
    };
  }
  return null;
}
function validateDomDecoGases() {
  var _document$getElementB52;
  var errors = [];
  var circuit = ((_document$getElementB52 = document.getElementById('circuitSelect')) === null || _document$getElementB52 === void 0 ? void 0 : _document$getElementB52.value) || 'OC';
  var bot = getDomBottomGasPct();
  var botCheck = validateGasFractionsPct(bot.o2, bot.he, 'bottomGas');
  if (!botCheck.ok) errors.push(botCheck);else {
    var _document$getElementB53;
    var botHypo = ZhlEngineBundle.validateHypoxicDecoGas(bot.o2, bot.he, 'bottomGas', circuit);
    if (botHypo) errors.push(botHypo);
    if (((_document$getElementB53 = document.getElementById('decoGas')) === null || _document$getElementB53 === void 0 ? void 0 : _document$getElementB53.value) === 'trimix' && bot.o2 > 40) {
      errors.push({
        message: 'Bottom trimix O₂ must not exceed 40%.'
      });
    }
  }
  var seenMixes = new Map();
  var _iterator2 = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step2;
  try {
    for (_iterator2.s(); !(_step2 = _iterator2.n()).done;) {
      var idx = _step2.value;
      var g = getDomDecoGasPct(idx);
      if (!g) continue;
      var chk = validateGasFractionsPct(g.o2, g.he, "dg".concat(idx));
      if (!chk.ok) errors.push(chk);else {
        var hypo = ZhlEngineBundle.validateHypoxicDecoGas(g.o2, g.he, "dg".concat(idx), circuit);
        if (hypo) errors.push(hypo);
        var dgf = getDecoCardFractions(idx);
        var mixKey = dgf ? "".concat(Math.round(dgf.fO2 * 1000), "|").concat(Math.round(dgf.fHe * 1000)) : "".concat(g.o2, "|").concat(g.he);
        if (seenMixes.has(mixKey)) {
          errors.push({
            message: "Duplicate deco gas mix: dg".concat(idx, " matches dg").concat(seenMixes.get(mixKey))
          });
        } else {
          seenMixes.set(mixKey, idx);
        }
      }
    }
  } catch (err) {
    _iterator2.e(err);
  } finally {
    _iterator2.f();
  }
  return {
    ok: errors.length === 0,
    errors: errors
  };
}
if (typeof window !== 'undefined') window.validateDomDecoGases = validateDomDecoGases;

// Get {fO2, fHe, fN2} from a gas card select+custom fields
function getDecoGasFractions(selectId, customId, trimixO2Id, trimixHeId) {
  var _document$getElementB54;
  var mix = (_document$getElementB54 = document.getElementById(selectId)) === null || _document$getElementB54 === void 0 ? void 0 : _document$getElementB54.value;
  if (!mix || mix === 'none') return null;
  if (mix === 'ean50') return {
    fO2: 0.50,
    fHe: 0,
    fN2: 0.50
  };
  if (mix === 'ean80') return {
    fO2: 0.80,
    fHe: 0,
    fN2: 0.20
  };
  if (mix === 'o2') return {
    fO2: 1.00,
    fHe: 0,
    fN2: 0.00
  };
  if (mix === 'custom') {
    var o2 = readDomO2Pct(customId);
    if (!Number.isFinite(o2)) return null;
    var fO2 = Math.min(1, Math.max(0.05, o2 / 100));
    return {
      fO2: fO2,
      fHe: 0,
      fN2: Math.max(0, 1 - fO2)
    };
  }
  if (mix === 'trimix') {
    var _o = readDomO2Pct(trimixO2Id);
    var he = readDomHePct(trimixHeId);
    if (!Number.isFinite(_o) || !Number.isFinite(he)) return null;
    var _fO3 = Math.min(0.99, Math.max(0.05, _o / 100));
    var fHe = Math.min(0.95, Math.max(0, he / 100));
    if (_fO3 + fHe > 1.0) return null;
    var fN2 = Math.max(0, 1 - _fO3 - fHe);
    return {
      fO2: _fO3,
      fHe: fHe,
      fN2: fN2
    };
  }
  return null;
}

// Gas label format: delegate to canonical schedule-core implementation in ZhlEngineBundle
function getGasLabel(fO2, fHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getGasLabel(fO2, fHe);
}
function getDecoGasFrac(selectId, customId) {
  var fracs = getDecoGasFractions(selectId, customId, selectId.replace('Mix', 'TrimixO2'), selectId.replace('Mix', 'TrimixHe'));
  return fracs ? fracs.fN2 : null;
}
function getDecoGasLabel(selectId, customId) {
  var fracs = getDecoGasFractions(selectId, customId, selectId.replace('Mix', 'TrimixO2'), selectId.replace('Mix', 'TrimixHe'));
  return fracs ? getGasLabel(fracs.fO2, fracs.fHe) : null;
}

// Full fractions for a deco gas card by card-index n
function getDecoCardFractions(n) {
  return getDecoGasFractions("dg".concat(n, "Mix"), "dg".concat(n, "CustomO2"), "dg".concat(n, "TrimixO2"), "dg".concat(n, "TrimixHe"));
}

// Get bottom gas {fO2, fHe, fN2} from the bottom gas card UI
function getBottomGasFractions() {
  var _document$getElementB55;
  var mix = (_document$getElementB55 = document.getElementById('decoGas')) === null || _document$getElementB55 === void 0 ? void 0 : _document$getElementB55.value;
  if (mix === 'air') return {
    fO2: 0.21,
    fHe: 0,
    fN2: 0.79
  };
  if (mix === 'ean32') return {
    fO2: 0.32,
    fHe: 0,
    fN2: 0.68
  };
  if (mix === 'ean36') return {
    fO2: 0.36,
    fHe: 0,
    fN2: 0.64
  };
  if (mix === 'custom') {
    var o2 = readDomO2Pct('decoCustomO2');
    if (!Number.isFinite(o2)) return null;
    var fO2 = Math.min(1.00, Math.max(0.05, o2 / 100));
    return {
      fO2: fO2,
      fHe: 0,
      fN2: 1 - fO2
    };
  }
  if (mix === 'trimix') {
    var _o2 = readDomO2Pct('botTrimixO2');
    var he = readDomHePct('botTrimixHe');
    if (!Number.isFinite(_o2) || !Number.isFinite(he)) return null;
    var _fO4 = Math.min(0.99, Math.max(0.05, _o2 / 100));
    var fHe = Math.min(0.95, Math.max(0, he / 100));
    if (_fO4 + fHe > 1.0) return null;
    return {
      fO2: _fO4,
      fHe: fHe,
      fN2: Math.max(0, 1 - _fO4 - fHe)
    };
  }
  return {
    fO2: 0.21,
    fHe: 0,
    fN2: 0.79
  };
}

// Returns the best available gas at a given depth.
// "Best" = lowest fN2 (highest O₂) whose floored ppO₂ at curDepthM is within
// that gas's own ppO2 limit (determined by O2 fraction band via getPPO2Limit).
// Switch depths are exact meters so gas switches happen precisely at the right ppO₂.

// ── Min Deco Profile helpers ──────────────────────────
function toggleMinDecoProfile() {
  var en = document.getElementById('minDecoProfileEnable').value === 'yes';
  var fields = document.getElementById('minDecoProfileFields');
  if (fields) fields.style.display = en ? 'contents' : 'none';
}
function syncMinStopTimeRounding() {
  var _document$getElementB56;
  var rounding = ((_document$getElementB56 = document.getElementById('stopRounding')) === null || _document$getElementB56 === void 0 ? void 0 : _document$getElementB56.value) || 'wholeminute';
  var subMin = document.getElementById('minStopTimeSubMin');
  var sel = document.getElementById('minStopTime');
  if (!subMin || !sel) return;
  var whole = rounding === 'wholeminute';
  subMin.disabled = whole;
  if (whole && sel.value === '0.0167') sel.value = '0.1667';
}
function updateMinDecoLabels(isMetric) {
  var l9 = document.getElementById('minDeco9mLabel');
  var l6 = document.getElementById('minDeco6mLabel');
  if (l9) l9.textContent = isMetric ? '9m (min)' : '30ft (min)';
  if (l6) l6.textContent = isMetric ? '6m (min)' : '20ft (min)';
}

/**
 * Enforce minimum stop times at 9 m (30 ft) and 6 m (20 ft).
 * Works on the collapsed steps array used by both Bühlmann and VPM-B engines.
 * @param {Array} steps  - array of step objects with {type, depth, to, dur, gas, ...}
 * @param {boolean} enabled
 * @param {number} min9m  - minimum minutes at 9 m stop
 * @param {number} min6m  - minimum minutes at 6 m stop
 * @param {boolean} isMetric
 * @returns {Array} steps with minimum enforced
 */
function enforceMinDecoProfile(steps, enabled, min9m, min6m, isMetric, fallbackGas, fallbackFN2, fallbackFHe) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.enforceMinDecoProfile(steps, enabled, min9m, min6m, isMetric, fallbackGas, fallbackFN2, fallbackFHe);
}
function getVpmMinDecoSettingsFromDom() {
  var _document$getElementB57, _document$getElementB58, _document$getElementB59;
  return {
    enabled: ((_document$getElementB57 = document.getElementById('minDecoProfileEnable')) === null || _document$getElementB57 === void 0 ? void 0 : _document$getElementB57.value) === 'yes',
    min9m: parseFloat((_document$getElementB58 = document.getElementById('minDeco9m')) === null || _document$getElementB58 === void 0 ? void 0 : _document$getElementB58.value) || 1,
    min6m: parseFloat((_document$getElementB59 = document.getElementById('minDeco6m')) === null || _document$getElementB59 === void 0 ? void 0 : _document$getElementB59.value) || 3,
    isMetric: units !== 'imperial'
  };
}
var _confirmCallback = null;
function showConfirm(message, callback) {
  _confirmCallback = callback || null;
  document.getElementById('confirmModalMsg').textContent = message;
  var modal = document.getElementById('confirmModal');
  modal.style.display = 'flex';
}
function closeConfirmModal(ok) {
  document.getElementById('confirmModal').style.display = 'none';
  if (ok && typeof _confirmCallback === 'function') {
    _confirmCallback();
  }
  _confirmCallback = null;
}

// ── Env Settings collapse (Water / Units / Altitude / Acclimatized) ─────────
// AUDIT-UNIT:UI-SETTINGS-CONTROLS
function toggleEnvSettings(forceOpen) {
  if (forceOpen === false) return;
  setMainNav('settings', document.getElementById('navBtnSettings'));
  try {
    localStorage.setItem('lspEnvOpen', '1');
  } catch (e) {}
}

// ── Advanced Settings collapse ───────────────────────────────────────────────
function toggleAdvancedSettings(forceOpen) {
  var body = document.getElementById('advancedSettingsBody');
  var chevron = document.getElementById('advancedSettingsChevron');
  var btn = document.getElementById('advancedSettingsToggle');
  if (!body) return;
  var isOpen = body.style.display !== 'none';
  var open = forceOpen !== undefined ? forceOpen : !isOpen;
  body.style.display = open ? '' : 'none';
  if (chevron) chevron.style.transform = open ? 'rotate(90deg)' : '';
  if (btn) {
    btn.style.borderColor = open ? 'var(--accent)' : 'var(--border)';
    btn.style.color = open ? 'var(--accent)' : 'var(--muted)';
  }
  _updateAdvancedSummary();
  try {
    localStorage.setItem('lspAdvancedOpen', open ? '1' : '0');
  } catch (e) {}
}
function _updateAdvancedSummary() {
  var _document$getElementB60, _document$getElementB61, _document$getElementB62, _document$getElementB63, _document$getElementB64, _document$getElementB65, _document$getElementB66, _document$getElementB67;
  // Show key non-default values in the collapsed summary line
  var el = document.getElementById('advancedSettingsSummary');
  if (!el) return;
  var body = document.getElementById('advancedSettingsBody');
  if (body && body.style.display !== 'none') {
    el.textContent = '';
    return;
  }
  var rateU = units === 'imperial' ? 'ft/min' : 'm/min';
  var depthU = units === 'imperial' ? 'ft' : 'm';
  var parts = [];
  var dr = (_document$getElementB60 = document.getElementById('descentRate')) === null || _document$getElementB60 === void 0 ? void 0 : _document$getElementB60.value;
  if (dr && dr !== '20') parts.push(dr + '↓' + rateU);
  var ar = (_document$getElementB61 = document.getElementById('ascentRate')) === null || _document$getElementB61 === void 0 ? void 0 : _document$getElementB61.value;
  if (ar && ar !== '10') parts.push(ar + '↑' + rateU);
  var dar = (_document$getElementB62 = document.getElementById('decoAscentRate')) === null || _document$getElementB62 === void 0 ? void 0 : _document$getElementB62.value;
  if (dar && dar !== '3') parts.push(dar + '↑deco');
  var ls = (_document$getElementB63 = document.getElementById('lastDecoStop')) === null || _document$getElementB63 === void 0 ? void 0 : _document$getElementB63.value;
  if (ls && ls !== '3') parts.push('last ' + ls + depthU);
  var ms = (_document$getElementB64 = document.getElementById('minStopTime')) === null || _document$getElementB64 === void 0 ? void 0 : _document$getElementB64.value;
  if (ms && ms !== '2') parts.push('min ' + ms + 'min');
  var wv = (_document$getElementB65 = document.getElementById('waterVapor')) === null || _document$getElementB65 === void 0 ? void 0 : _document$getElementB65.value;
  if (wv === '0.0627') parts.push('WV Bühl');
  var tr = (_document$getElementB66 = document.getElementById('decoTransitMode')) === null || _document$getElementB66 === void 0 ? void 0 : _document$getElementB66.value;
  if (tr === 'schreiner') parts.push('Schreiner');
  var md = (_document$getElementB67 = document.getElementById('minDecoProfileEnable')) === null || _document$getElementB67 === void 0 ? void 0 : _document$getElementB67.value;
  if (md === 'yes') parts.push('MinDeco on');
  el.textContent = parts.length ? parts.join(' · ') : 'defaults';
}

// ── User personal defaults ────────────────────────────────────────────────
// The ADVANCED settings that "Set as My Default" persists.
// Factory defaults are used as fallback when no user defaults are saved.
var _ADV_FIELDS = ['gfPresetSelect', 'gfLowInput', 'gfHighInput', 'descentRate', 'ascentRate', 'decoAscentRate', 'surfaceAscentRate', 'decoStep', 'lastDecoStop', 'minStopTime', 'stopRounding', 'decoTransitMode', 'shallowGradient', 'waterVapor', 'heHalfTimeMode', 'sacBottom', 'sacDeco', 'contingencySacMultiplier', 'n2NarcSel', 'o2NarcSel', 'ppo2Bottom', 'ppo2Deco', 'o2AtMODSelect', 'ccrDescentSetpoint', 'ccrBottomSetpoint', 'ccrDecoSetpoint', 'ccrLoopVolume', 'ccrMetabolicO2', 'minDecoProfileEnable', 'minDeco9m', 'minDeco6m'];
function saveAsMyDefault() {
  var snapshot = {};
  _ADV_FIELDS.forEach(function (id) {
    var el = document.getElementById(id);
    if (el) snapshot[id] = el.value;
  });
  try {
    localStorage.setItem('lspUserAdvDefaults', JSON.stringify(snapshot));
    showToast('✓ Advanced settings saved as your personal defaults', 'preset');
    // Update button to show it's been saved
    var btn = document.getElementById('saveMyDefaultBtn');
    if (btn) {
      var orig = btn.innerHTML;
      btn.style.color = 'var(--green)';
      btn.style.borderColor = 'var(--green)';
      setTimeout(function () {
        btn.style.color = '';
        btn.style.borderColor = '';
      }, 1800);
    }
  } catch (e) {
    showToast('Could not save defaults — storage unavailable', 'error', true);
  }
}
function resetToDefaults() {
  showConfirm('Reset settings to defaults?', function () {
    var ud = null;
    try {
      ud = JSON.parse(localStorage.getItem('lspUserAdvDefaults') || 'null');
    } catch (e) {}
    _doResetToDefaults(ud);
  });
}
function _factoryMetricDefaults() {
  return {
    // Rates
    descentRate: '20',
    ascentRate: '10',
    decoAscentRate: '3',
    surfaceAscentRate: '3',
    // Stop settings
    decoStep: '3',
    lastDecoStop: '3',
    minStopTime: '2',
    stopRounding: 'wholeminute',
    waterVapor: '0.0577',
    // ppO2 limits
    ppo2Bottom: '1.4',
    ppo2Deco: '1.6',
    // SAC rates
    sacBottom: '22',
    sacDeco: '20',
    contingencySacMultiplier: '1.5',
    // Narcosis
    n2NarcSel: 'yes',
    o2NarcSel: 'yes',
    // Deco plan inputs
    tecDepth: '40',
    tecBT: '25',
    recDepth: '40',
    recBT: '30',
    decoGas: 'air',
    circuitSelect: 'OC',
    ccrDecoSetpoint: '1.3',
    ccrBottomSetpoint: '1.2',
    ccrDescentSetpoint: '0.7',
    ccrBailoutToggle: 'off',
    ccrBailoutGfLow: '50',
    ccrBailoutGfHigh: '85',
    ccrLoopVolume: '10',
    ccrMetabolicO2: '1.5',
    ccrSacStress: '50',
    ccrSacDeco: '25',
    ccrStressTime: '10',
    ccrProblemSolve: '3',
    ccrSetpoint: '1.3',
    scrLoopVolume: '10',
    scrMetabolicO2: '1.5',
    // Deco gases
    dg1Mix: 'ean50',
    dg2Mix: 'o2',
    // Cylinder defaults
    cylBot_size: '12',
    cylBot_pres: '200',
    cylDg1_size: '11',
    cylDg1_pres: '200',
    cylDg2_size: '11',
    cylDg2_pres: '200',
    cylTravelGas_size: '11',
    cylTravelGas_pres: '200',
    // Cylinder reserve pressures (bar)
    cylBot_reserve: '50',
    cylDg1_reserve: '50',
    cylDg2_reserve: '50',
    cylTravelGas_reserve: '50',
    // Gas Plan tab cylinders (metric defaults)
    gpBot_size: '12',
    gpBot_fill: '200',
    gpBot_reserve: '50',
    gpTravel_size: '11',
    gpTravel_fill: '200',
    gpTravel_reserve: '50',
    gpDg1_size: '11',
    gpDg1_fill: '200',
    gpDg1_reserve: '50',
    gpDg2_size: '11',
    gpDg2_fill: '200',
    gpDg2_reserve: '50',
    // Min deco profile
    minDecoProfileEnable: 'no',
    minDeco9m: '1',
    minDeco6m: '3',
    // He half-time
    heHalfTimeMode: 'baker'
  };
}
function _factoryDefaultsForUnits() {
  var d = _factoryMetricDefaults();
  if (units !== 'imperial') return d;
  var FT_PER_M = 3.28084;
  var PSI_PER_BAR = 14.5038;
  var CUFT_PER_L = CUFT_PER_LITRE;
  var cuft = function cuft(litres) {
    return (litres * CUFT_PER_L).toFixed(1).replace(/\.0$/, '');
  };
  var psi = function psi(bar) {
    return String(Math.round(bar * PSI_PER_BAR));
  };
  var sacCuft = function sacCuft(lMin) {
    return (lMin * CUFT_PER_L).toFixed(2).replace(/\.0+$/, '');
  };
  return Object.assign(d, {
    tecDepth: String(Math.round(40 * FT_PER_M)),
    recDepth: String(Math.round(40 * FT_PER_M)),
    sacBottom: sacCuft(22),
    sacDeco: sacCuft(20),
    cylBot_size: cuft(12),
    cylBot_pres: psi(200),
    cylBot_reserve: psi(50),
    cylDg1_size: cuft(11),
    cylDg1_pres: psi(200),
    cylDg1_reserve: psi(50),
    cylDg2_size: cuft(11),
    cylDg2_pres: psi(200),
    cylDg2_reserve: psi(50),
    cylTravelGas_size: cuft(11),
    cylTravelGas_pres: psi(200),
    cylTravelGas_reserve: psi(50),
    gpBot_size: cuft(12),
    gpBot_fill: psi(200),
    gpBot_reserve: psi(50),
    gpTravel_size: cuft(11),
    gpTravel_fill: psi(200),
    gpTravel_reserve: psi(50),
    gpDg1_size: cuft(11),
    gpDg1_fill: psi(200),
    gpDg1_reserve: psi(50),
    gpDg2_size: cuft(11),
    gpDg2_fill: psi(200),
    gpDg2_reserve: psi(50)
  });
}
function _doResetToDefaults(userDefaults) {
  var _updateGasMODDisplays2;
  var defaults = _factoryDefaultsForUnits();

  // Overlay user's personal advanced defaults on top of factory defaults (if any)
  if (userDefaults && _typeof(userDefaults) === 'object') {
    _ADV_FIELDS.forEach(function (id) {
      if (userDefaults[id] != null) defaults[id] = userDefaults[id];
    });
  }

  // Apply to all DOM fields
  Object.entries(defaults).forEach(function (_ref5) {
    var _ref6 = _slicedToArray(_ref5, 2),
      id = _ref6[0],
      val = _ref6[1];
    var el = document.getElementById(id);
    if (el) el.value = val;
  });

  // Reset Gas Plan rule to thirds
  setGasRule('thirds');

  // Reset GF — honour saved personal GF when present
  if ((userDefaults === null || userDefaults === void 0 ? void 0 : userDefaults.gfLowInput) != null && (userDefaults === null || userDefaults === void 0 ? void 0 : userDefaults.gfHighInput) != null) {
    setGF(parseInt(userDefaults.gfLowInput, 10) || 20, parseInt(userDefaults.gfHighInput, 10) || 85);
  } else if (userDefaults !== null && userDefaults !== void 0 && userDefaults.gfPresetSelect) {
    var gfEl = document.getElementById('gfPresetSelect');
    if (gfEl) gfEl.value = userDefaults.gfPresetSelect;
    if (typeof applyGFPreset === 'function') applyGFPreset();else setGF(20, 85);
  } else {
    setGF(20, 85);
  }

  // Reset water density to salt default
  setWaterDensity('salt');

  // Reset altitude to sea level
  var altSel = document.getElementById('altitudeSelect');
  if (altSel) altSel.value = '0';
  var altCustomRow = document.getElementById('altitudeCustomRow');
  if (altCustomRow) altCustomRow.style.display = 'none';
  var acclSel = document.getElementById('acclimatizedSelect');
  if (acclSel) acclSel.value = 'yes';
  altitudeM = 0;
  altSurfaceP = SEA_LEVEL_P;
  altAcclimatized = true;
  try {
    localStorage.removeItem('lspAltitude');
    localStorage.removeItem('lspAcclimatized');
  } catch (e) {}
  try {
    localStorage.removeItem('gfCustomLow');
    localStorage.removeItem('gfCustomHigh');
  } catch (e) {}
  var badge = document.getElementById('altitudePressureDisplay');
  if (badge) badge.textContent = '';

  // Apply water vapor constant
  updateWaterVapor();

  // Clear custom O2 fields visibility
  toggleDecoCustomO2 === null || toggleDecoCustomO2 === void 0 || toggleDecoCustomO2('decoGas', 'decoCustomO2Field');
  toggleDecoCustomO2 === null || toggleDecoCustomO2 === void 0 || toggleDecoCustomO2('dg1Mix', 'dg1CustomField');
  toggleDecoCustomO2 === null || toggleDecoCustomO2 === void 0 || toggleDecoCustomO2('dg2Mix', 'dg2CustomField');
  toggleBottomTrimix === null || toggleBottomTrimix === void 0 || toggleBottomTrimix();
  var _iterator3 = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step3;
  try {
    for (_iterator3.s(); !(_step3 = _iterator3.n()).done;) {
      var idx = _step3.value;
      toggleDecoTrimix === null || toggleDecoTrimix === void 0 || toggleDecoTrimix(idx);
    }
  } catch (err) {
    _iterator3.e(err);
  } finally {
    _iterator3.f();
  }
  toggleCircuitFields === null || toggleCircuitFields === void 0 || toggleCircuitFields();
  syncMinStopTimeRounding === null || syncMinStopTimeRounding === void 0 || syncMinStopTimeRounding();
  (_updateGasMODDisplays2 = updateGasMODDisplays) === null || _updateGasMODDisplays2 === void 0 || _updateGasMODDisplays2();
  _updateAdvancedSummary();

  // Clear localStorage v4
  try {
    localStorage.removeItem('lspDiveSettings_v6');
  } catch (e) {}

  // Save fresh defaults
  if (typeof appSettings !== 'undefined' && appSettings.save) {
    appSettings.save(false);
  }

  // Clear any existing results
  var dr = document.getElementById('decoResult');
  if (dr) dr.style.display = 'none';

  // Update switch depth displays without running full schedule
  document.querySelectorAll('.switch-depth-display').forEach(function (el) {
    el.value = 'Calculate to see';
  });
  console.log('[LSP] ↺ Settings reset to defaults (ApexDeco-matched)');
}
function getActiveGas(curDepthM, bottomFN2, bottomFHe, decoGases, getPPO2LimitFn, bottomLabel) {
  _syncZhlBundleEnv();
  return ZhlEngineBundle.getActiveGas(curDepthM, bottomFN2, bottomFHe || 0, decoGases, getPPO2LimitFn, bottomLabel);
}
function ppO2DisplayStyle(ppO2Str, limit) {
  if (ppO2Str === 'ERR') return 'color:var(--red);font-weight:700;';
  var v = parseFloat(ppO2Str);
  var lim = limit != null ? limit : 1.6;
  if (!Number.isFinite(v)) return 'color:var(--red);font-weight:700;';
  if (v > lim) return 'color:var(--red);font-weight:700;';
  if (v > lim * 0.97) return 'color:var(--orange);';
  return 'color:var(--muted);';
}
function ppO2Check(depthM, fN2, fHe, opts) {
  _syncZhlBundleEnv();
  if (opts && opts.ccr) {
    opts = _objectSpread(_objectSpread({}, opts), {}, {
      ccr: mergeCCRSettings(opts.ccr)
    });
  }
  return ZhlEngineBundle.ppO2Check(depthM, fN2, fHe, opts);
}
function fmtPpO2(v) {
  if (v === 'ERR' || v == null) return String(v);
  return Number.isFinite(v) ? v.toFixed(2) : String(v);
}
function renderBlockingScheduleError(message) {
  var _clearPlannerResults2, _notifyScheduleError;
  (_clearPlannerResults2 = _clearPlannerResults) === null || _clearPlannerResults2 === void 0 || _clearPlannerResults2();
  (_notifyScheduleError = notifyScheduleError) === null || _notifyScheduleError === void 0 || _notifyScheduleError(message);
  scheduleDecoScheduleStackSync === null || scheduleDecoScheduleStackSync === void 0 || scheduleDecoScheduleStackSync();
}
function validateOcBottomGasPpo2(depthM, bottomFracs, ppo2Limit, settings, algorithmLabel) {
  if (!bottomFracs || !Number.isFinite(depthM) || depthM <= 0) return {
    ok: true
  };
  if (isRebreatherCircuit((settings === null || settings === void 0 ? void 0 : settings.circuit) || 'OC')) return {
    ok: true
  };
  var actual = ppO2Check(depthM, bottomFracs.fN2, bottomFracs.fHe);
  var limit = Number.isFinite(ppo2Limit) ? ppo2Limit : 1.4;
  if (!Number.isFinite(actual) || actual <= limit + 0.005) return {
    ok: true,
    actual: actual
  };
  var modM = calcGasMODm(bottomFracs.fO2, limit);
  var depthDisp = units === 'imperial' ? Math.round(depthM * 3.28084) : Math.round(depthM);
  var modDisp = units === 'imperial' ? Math.round(modM * 3.28084) : modM;
  var du = units === 'imperial' ? 'ft' : 'm';
  var label = getGasLabel(bottomFracs.fO2, bottomFracs.fHe);
  var algoPrefix = algorithmLabel ? "".concat(algorithmLabel, ": ") : '';
  return {
    ok: false,
    actual: actual,
    message: "".concat(algoPrefix, "BEYOND MOD. ").concat(depthDisp).concat(du, " exceeds ").concat(label, " MOD of ").concat(modDisp).concat(du, " at actual ").concat(actual.toFixed(2), " bar ppO\u2082. Use a lower O\u2082 bottom gas, add a travel gas, or reduce depth.")
  };
}
function validateVpmOcBottomGasPpo2(depthM, bottomFracs, ppo2Limit, settings) {
  return validateOcBottomGasPpo2(depthM, bottomFracs, ppo2Limit, settings, 'VPM');
}

// ── VPM SCHEDULE RUNNER ──────────────────────────────────────────────
// ── Feature B: VPM repetitive dive carry ───────────────────────────────────
var _lastVPMResult = null; // { finalTissues, finalBubbleState }

function updateVpmRepUI() {
  var cb = document.getElementById('vpmRepMode');
  var siRow = document.getElementById('vpmRepSIRow');
  var lbl = document.getElementById('vpmRepLabel');
  if (!cb) return;
  if (cb.checked) {
    if (siRow) siRow.style.display = 'flex';
    if (lbl && _lastVPMResult) lbl.textContent = '● bubble state loaded';
  } else {
    if (siRow) siRow.style.display = 'none';
    if (lbl) lbl.textContent = '';
  }
}
function clearVpmRepState() {
  _lastVPMResult = null;
  var cb = document.getElementById('vpmRepMode');
  if (cb) cb.checked = false;
  var siRow = document.getElementById('vpmRepSIRow');
  if (siRow) siRow.style.display = 'none';
  var lbl = document.getElementById('vpmRepLabel');
  if (lbl) lbl.textContent = '';
  var row = document.getElementById('vpmRepRow');
  if (row) row.style.display = 'none';
}
function updateZhlRepUI() {
  var cb = document.getElementById('zhlRepMode');
  var siRow = document.getElementById('zhlRepSIRow');
  var lbl = document.getElementById('zhlRepLabel');
  if (!cb) return;
  if (cb.checked) {
    if (siRow) siRow.style.display = 'flex';
    if (lbl && peekZhlRepState()) lbl.textContent = '● tissue state loaded';
  } else {
    if (siRow) siRow.style.display = 'none';
    if (lbl) lbl.textContent = '';
  }
}
function clearZhlRepState() {
  window._zhlRepState = null;
  var cb = document.getElementById('zhlRepMode');
  if (cb) cb.checked = false;
  var siRow = document.getElementById('zhlRepSIRow');
  if (siRow) siRow.style.display = 'none';
  var lbl = document.getElementById('zhlRepLabel');
  if (lbl) lbl.textContent = '';
  var row = document.getElementById('zhlRepRow');
  if (row) row.style.display = 'none';
}
// ───────────────────────────────────────────────────────────────────────────────

function onConservatismChange() {
  _syncConservatismBtns();
  if (window._zhlHeadless) return;
  var sel = document.getElementById('conservatismSelect');
  var nextVal = (sel === null || sel === void 0 ? void 0 : sel.value) || '2';
  if (sel && sel.dataset.lspLastValue === nextVal) {
    if (typeof appSettings !== 'undefined' && appSettings.save) {
      appSettings.save(false);
    }
    return;
  }
  if (sel) sel.dataset.lspLastValue = nextVal;
  var decoRes = document.getElementById('decoResult');
  if (decoRes && decoRes.style.display !== 'none' && decoRes.innerHTML.trim()) {
    runDecoSchedule();
  }
  if (typeof appSettings !== 'undefined' && appSettings.save) {
    appSettings.save(false);
  }
}

// AUDIT-UNIT:UI-VPM-RUNNER
function runVPMSchedule(depthM, bt, descentRate, ascentRate, decoAscentRate, surfaceAscentRate, stepSize, lastStop, minStopTime, ppo2Bottom, ppo2Deco, model, wholeMinStops) {
  var _document$getElementB68, _ref7, _settings$bottomSetpo;
  // Headless mode: skip all VPM computation and DOM rendering
  // (called from ZHLEngine or fastRDS headless path when algo is VPM-B)
  if (window._zhlHeadless) return;
  _syncZhlBundleEnv();
  var conservatism = parseInt(((_document$getElementB68 = document.getElementById('conservatismSelect')) === null || _document$getElementB68 === void 0 ? void 0 : _document$getElementB68.value) || '0');
  var gfHi = mGF && Number.isFinite(mGF.high) ? mGF.high : 85;

  // Gather deco gases from DOM (same as Bühlmann)
  var decoGases = [];
  var _iterator4 = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step4;
  try {
    for (_iterator4.s(); !(_step4 = _iterator4.n()).done;) {
      var n = _step4.value;
      var mixEl = document.getElementById("dg".concat(n, "Mix"));
      if (!mixEl || mixEl.value === 'none' || !mixEl.value) continue;
      var dgvFracs = getDecoCardFractions(n);
      if (!dgvFracs || !Number.isFinite(dgvFracs.fO2) || !Number.isFinite(dgvFracs.fHe)) {
        var dgPct = getDomDecoGasPct(n);
        if (dgPct) {
          notifyScheduleError("VPM: invalid deco gas ".concat(n, " (").concat(dgPct.o2, "%/").concat(dgPct.he, "%) \u2014 fix mix or set to None."));
        } else {
          notifyScheduleError("VPM: deco gas ".concat(n, " has no valid mix \u2014 enter O\u2082/He or set to None."));
        }
        continue;
      }
      decoGases.push({
        o2: parseFloat((dgvFracs.fO2 * 100).toFixed(2)),
        he: parseFloat((dgvFracs.fHe * 100).toFixed(2))
      });
    }

    // Build settings object for VPMEngine
    // IMPORTANT: waterType 0=salt, 1=fresh, 2=EN13319, 3=custom (uses barPerM)
  } catch (err) {
    _iterator4.e(err);
  } finally {
    _iterator4.f();
  }
  var waterTypeVal = getWaterTypeForVPM();
  var settings = _objectSpread({
    metric: true,
    waterType: waterTypeVal,
    barPerM: BAR_PER_METRE,
    descentRate: descentRate,
    ascentRate: ascentRate,
    decoAscentRate: decoAscentRate,
    surfaceAscentRate: surfaceAscentRate,
    stepSize: stepSize,
    lastStop: lastStop,
    minStopTime: minStopTime,
    ppO2Bottom: ppo2Bottom,
    ppO2Deco: ppo2Deco,
    // ppO2 thresholds for selectDecoGas — must match ApexDeco defaults:
    // ≤28% O2 → ppO2Low, 28-45% O2 → ppO2Mid, >45% O2 → ppO2High
    // EAN50 is >45% so uses ppO2High=1.6 — critical for correct switch depth
    ppO2Low: ppo2Bottom,
    // air/lean: 1.4
    ppO2Mid: 1.5,
    // nitrox 28-45%: 1.5 (NOT 1.4 — ApexDeco default)
    ppO2High: ppo2Deco,
    // rich deco gas >45%: 1.6
    o2MaxDepth: 6,
    conservatism: conservatism,
    gfHi: gfHi,
    gfs: gfHi,
    waterVapor: WATER_VAPOR,
    firstStop30sec: false,
    extendedStops: false,
    wholeMinStops: wholeMinStops === true,
    altitude: Number.isFinite(altitudeM) ? altitudeM : 0,
    minDecoProfile: function () {
      var m = getVpmMinDecoSettingsFromDom();
      return {
        enabled: m.enabled,
        m9: m.min9m,
        m6: m.min6m,
        isMetric: m.isMetric
      };
    }()
  }, getCCRSettingsFromDOM());

  // ── Feature B: inject repetitive dive state if active ─────────────────────────
  var vpmRepEl = document.getElementById('vpmRepMode');
  var vpmSIEl = document.getElementById('vpmSurfaceInterval');
  if (vpmRepEl && vpmRepEl.checked && _lastVPMResult) {
    var siCheck = validateVpmSurfaceInterval();
    if (!siCheck.ok) {
      var msg = siCheck.msg || 'Invalid VPM surface interval.';
      if (!renderScheduleErrorRow(msg)) alert(msg);
      return;
    }
    var siMin = siCheck.value;
    settings._preTissues = _lastVPMResult.finalTissues;
    settings._surfaceInterval = siMin;
    settings._prevBubbleState = _lastVPMResult.finalBubbleState;
    // Carry CNS (decays on 90-min half-life) and OTU (daily accumulator, no decay)
    // CNS half-life: 90 min (Baker 1998 / NOAA standard)
    if (_lastVPMResult.finalCNS != null) {
      settings._preCNS = _lastVPMResult.finalCNS * Math.pow(0.5, siMin / 90);
    }
    if (_lastVPMResult.finalOTU != null) {
      settings._preOTU = _lastVPMResult.finalOTU; // OTU is a daily dose — no decay within a day
    }
  }
  // ── Prior dive O₂ carry (v2.20.0): inject from CNS tab if set ────────────────
  // Applied additively on top of VPM repetitive carry (if both active).
  // Day-boundary: OTU resets if >24h (computed in updatePriorDiveCarry).
  if (window._priorDiveCarry) {
    settings._preOTU = (settings._preOTU || 0) + window._priorDiveCarry.otuCarry;
    settings._preCNS = (settings._preCNS || 0) + window._priorDiveCarry.cnsCarry;
  }
  // ───────────────────────────────────────────────────────────────────────────

  // Bottom gas from DOM
  var bottomGasEl = document.getElementById('decoGas');
  var _vpmBotFracs = getBottomGasFractions();
  if (!_vpmBotFracs) {
    var _msg = 'Cannot run VPM schedule: invalid bottom gas configuration.';
    if (!renderScheduleErrorRow(_msg)) alert(_msg);
    return;
  }
  var bottomO2pct = Math.round(_vpmBotFracs.fO2 * 100);
  var bottomHePct = Math.round(_vpmBotFracs.fHe * 100);
  var vpmBottomPpo2 = validateVpmOcBottomGasPpo2(depthM, _vpmBotFracs, ppo2Bottom, settings);
  if (!vpmBottomPpo2.ok) {
    renderBlockingScheduleError(vpmBottomPpo2.message);
    return;
  }
  var btAtDepthMin = Math.max(0, bt - depthM / descentRate);
  var levels = [{
    depth: depthM,
    time: btAtDepthMin,
    o2: bottomO2pct,
    he: bottomHePct,
    setpoint: (_ref7 = (_settings$bottomSetpo = settings.bottomSetpoint) !== null && _settings$bottomSetpo !== void 0 ? _settings$bottomSetpo : settings.setpoint) !== null && _ref7 !== void 0 ? _ref7 : 1.2,
    oc: settings.bailout
  }];
  var result;
  var vpmApi = typeof window !== 'undefined' && window.VPMEngine ? window.VPMEngine : null;
  if (!vpmApi || typeof vpmApi.calculate !== 'function') {
    var _msg2 = 'VPM engine failed to load (vpm-engine-bundle.js). Check network or refresh.';
    console.error('[VPM]', _msg2);
    renderScheduleErrorRow(_msg2);
    return;
  }
  try {
    result = vpmApi.calculate(levels, decoGases, settings, model);
  } catch (e) {
    console.error('[VPM] Engine error:', e);
    renderScheduleErrorRow("VPM Error: ".concat(e.message));
    return;
  }
  if (result.code === 'VPM_STOP_CAP' && result.plan && result.plan.length) {
    renderVPMResults(result, settings, depthM, bt, bottomO2pct, bottomHePct, decoGases, model, _vpmBotFracs);
    return;
  }
  if (result.error) {
    renderScheduleErrorRow("VPM Error: ".concat(result.error));
    return;
  }

  // ── Feature B: save dive result for next repetitive dive ───────────────────────
  if (result.finalTissues && !_contingencyRunning) {
    _lastVPMResult = {
      finalTissues: result.finalTissues,
      finalBubbleState: result.finalBubbleState,
      finalCNS: result.totalCNS,
      // carry CNS for repetitive dive (decays on 90-min half-life)
      finalOTU: result.totalOTU // carry OTU for repetitive dive (daily accumulator)
    };
    // Show/enable the repetitive dive UI now that we have a result
    var vpmRepRow = document.getElementById('vpmRepRow');
    if (vpmRepRow) vpmRepRow.style.display = 'flex';
  }
  // ───────────────────────────────────────────────────────────────────────────

  // Render into the existing deco table using the same format as Bühlmann
  renderVPMResults(result, settings, depthM, bt, bottomO2pct, bottomHePct, decoGases, model, _vpmBotFracs);
}

// ── END VPM SCHEDULE RUNNER ──────────────────────────────────────────

// VPMEngine — Tier 3 bundle (vpm-engine-bundle.js)

function validateDiveInputs(depthId, btId, depthLimits) {
  var _document$getElementB69, _document$getElementB70;
  if (window._zhlHeadless) return {
    ok: true
  };
  var lim = depthLimits || {};
  var maxDepth = units === 'imperial' ? lim.imperial != null ? lim.imperial : 330 : lim.metric != null ? lim.metric : 100;
  var maxBt = lim.maxBt != null ? lim.maxBt : 300;
  var rawD = parseFloat((_document$getElementB69 = document.getElementById(depthId)) === null || _document$getElementB69 === void 0 ? void 0 : _document$getElementB69.value);
  var bt = parseInt((_document$getElementB70 = document.getElementById(btId)) === null || _document$getElementB70 === void 0 ? void 0 : _document$getElementB70.value, 10);
  var du = units === 'imperial' ? 'ft' : 'm';
  if (!Number.isFinite(rawD) || rawD <= 0) {
    return {
      ok: false,
      msg: 'Depth must be a positive number.'
    };
  }
  if (rawD > maxDepth) {
    return {
      ok: false,
      msg: "Depth exceeds the ".concat(maxDepth, " ").concat(du, " limit.")
    };
  }
  if (!Number.isFinite(bt) || bt <= 0) {
    return {
      ok: false,
      msg: 'Bottom time must be a positive number of minutes.'
    };
  }
  if (bt > maxBt) {
    return {
      ok: false,
      msg: "Bottom time exceeds the ".concat(maxBt, " minute limit.")
    };
  }
  return {
    ok: true
  };
}
function validatePlannerInputs() {
  var _document$getElementB71, _document$getElementB72, _document$getElementB73;
  var isPadi = typeof algo !== 'undefined' && algo === 'padi';
  var depthLim = isPadi ? {
    metric: PADI_TABLE_MAX_M,
    imperial: PADI_TABLE_MAX_FT,
    maxBt: 300
  } : {
    metric: 100,
    imperial: 330,
    maxBt: 300
  };
  var base = validateDiveInputs('recDepth', 'recBT', depthLim);
  if (!base.ok) return base;
  var isMetric = units === 'metric';
  var rawD = parseFloat((_document$getElementB71 = document.getElementById('recDepth')) === null || _document$getElementB71 === void 0 ? void 0 : _document$getElementB71.value);
  var bt = parseInt((_document$getElementB72 = document.getElementById('recBT')) === null || _document$getElementB72 === void 0 ? void 0 : _document$getElementB72.value, 10);
  var depthM = isMetric ? rawD : rawD / 3.28084;
  if (isPadi && padiTableRowIndex(depthM) == null) {
    var du = isMetric ? "".concat(PADI_TABLE_MAX_M, " m") : "".concat(PADI_TABLE_MAX_FT, " ft");
    return {
      ok: false,
      msg: "Depth exceeds the PADI table maximum (".concat(du, ").")
    };
  }
  var descentRate = Math.max(1, parseInt((_document$getElementB73 = document.getElementById('descentRate')) === null || _document$getElementB73 === void 0 ? void 0 : _document$getElementB73.value, 10) || 20);
  if (Number.isFinite(depthM) && Number.isFinite(bt) && depthM > 0 && bt <= depthM / descentRate + 1e-6) {
    var descentMin = (depthM / descentRate).toFixed(1);
    return {
      ok: false,
      msg: "Bottom time (".concat(bt, " min) is not longer than descent (").concat(descentMin, " min); time at depth will be 0.")
    };
  }
  if (!isPadi) {
    var _document$getElementB74;
    var mix = ((_document$getElementB74 = document.getElementById('gasMix')) === null || _document$getElementB74 === void 0 ? void 0 : _document$getElementB74.value) || 'air';
    if (mix === 'trimix') {
      var check = validateGasFractionsPct(readDomO2Pct('plannerTrimixO2'), readDomHePct('plannerTrimixHe'), 'plannerTrimix');
      if (!check.ok) return {
        ok: false,
        msg: check.message
      };
      if (check.o2 < 18) {
        return {
          ok: false,
          msg: 'Trimix O₂ below 18% is hypoxic — use a higher O₂ fraction.'
        };
      }
    } else if (mix === 'custom') {
      var _check = validateGasFractionsPct(readDomO2Pct('customO2'), 0, 'customO2');
      if (!_check.ok) return {
        ok: false,
        msg: _check.message
      };
      var o2 = _check.o2;
      if (o2 < 21 || o2 > 100) {
        return {
          ok: false,
          msg: 'Custom O₂ must be between 21% and 100%.'
        };
      }
    }
  }
  return {
    ok: true
  };
}
var REP_SURFACE_INTERVAL_DEFAULT_MIN = 60;
var REP_SURFACE_INTERVAL_MAX_MIN = 10080;
function sanitizeRepetitiveSurfaceInterval(raw) {
  var fallback = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : REP_SURFACE_INTERVAL_DEFAULT_MIN;
  var value = Number(raw);
  if (!Number.isFinite(value) || value < 0 || value > REP_SURFACE_INTERVAL_MAX_MIN) {
    return fallback;
  }
  return Math.round(value);
}
function normalizeRepetitiveSurfaceIntervalInput(id) {
  var fallback = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : REP_SURFACE_INTERVAL_DEFAULT_MIN;
  var el = document.getElementById(id);
  var value = sanitizeRepetitiveSurfaceInterval(el === null || el === void 0 ? void 0 : el.value, fallback);
  if (el && String(el.value) !== String(value)) {
    el.value = String(value);
  }
  return value;
}
function validateVpmSurfaceInterval() {
  var _document$getElementB75;
  var vpmRepEl = document.getElementById('vpmRepMode');
  if (!(vpmRepEl !== null && vpmRepEl !== void 0 && vpmRepEl.checked)) return {
    ok: true,
    value: 0
  };
  var raw = (_document$getElementB75 = document.getElementById('vpmSurfaceInterval')) === null || _document$getElementB75 === void 0 ? void 0 : _document$getElementB75.value;
  var si = parseFloat(raw);
  if (!Number.isFinite(si) || si < 0 || si > REP_SURFACE_INTERVAL_MAX_MIN) {
    return {
      ok: false,
      msg: 'Surface interval must be a number from 0 to 10080 minutes.'
    };
  }
  return {
    ok: true,
    value: si
  };
}
function validateDecoInputs() {
  var base = validateDiveInputs('tecDepth', 'tecBT', {
    metric: 120,
    imperial: 394,
    maxBt: 300
  });
  if (!base.ok) return base;
  var decoCheck = validateDomDecoGases();
  if (!decoCheck.ok) {
    return {
      ok: false,
      msg: decoCheck.errors.map(function (e) {
        return e.message;
      }).join('\n')
    };
  }
  var vpmSi = validateVpmSurfaceInterval();
  if (!vpmSi.ok) return vpmSi;
  return {
    ok: true
  };
}

// ═══════════════════════════════════════════════
// ZHL SCHEDULE CORE — Tier 3 (bundle + DOM param builder)
// ═══════════════════════════════════════════════

function getZhlEnvironment(settings) {
  var s = settings || {};
  var surfP = typeof altSurfaceP !== 'undefined' ? altSurfaceP : 1.01325;
  var accl = typeof altAcclimatized !== 'undefined' ? altAcclimatized : true;
  if (s.altitude != null && Number(s.altitude) > 0) {
    surfP = altToPressure(Number(s.altitude));
    if (s.acclimatized != null) accl = s.acclimatized !== false;
  }
  return {
    altSurfaceP: surfP,
    barPerMetre: BAR_PER_METRE,
    waterVapor: WATER_VAPOR,
    altAcclimatized: accl,
    allowO2AtMOD: typeof allowO2AtMOD !== 'undefined' ? allowO2AtMOD : true
  };
}
function zhlOptimalSwitchDepth(fO2, ctx) {
  ZhlEngineBundle.applyEnvironment(getZhlEnvironment());
  return ZhlEngineBundle.zhlOptimalSwitchDepth(fO2, ctx);
}
function peekZhlRepState() {
  return window._zhlRepState && Array.isArray(window._zhlRepState.tissues) ? window._zhlRepState : null;
}
function saveZhlRepState(tissues, surfaceIntervalMin, exposure) {
  if (tissues && tissues.length) {
    var _exposure$totalCNS, _exposure$totalOTU;
    var lp = window._lastPlan;
    window._zhlRepState = {
      tissues: tissues,
      surfaceIntervalMin: sanitizeRepetitiveSurfaceInterval(surfaceIntervalMin, REP_SURFACE_INTERVAL_DEFAULT_MIN),
      surfaceP: altSurfaceP || SEA_LEVEL_P,
      totalCNS: (_exposure$totalCNS = exposure === null || exposure === void 0 ? void 0 : exposure.totalCNS) !== null && _exposure$totalCNS !== void 0 ? _exposure$totalCNS : lp === null || lp === void 0 ? void 0 : lp.totalCNS,
      totalOTU: (_exposure$totalOTU = exposure === null || exposure === void 0 ? void 0 : exposure.totalOTU) !== null && _exposure$totalOTU !== void 0 ? _exposure$totalOTU : lp === null || lp === void 0 ? void 0 : lp.totalOTU
    };
  }
}
function getZhlRepStateForSchedule() {
  var _snap$surfaceP;
  var zhlRepEl = document.getElementById('zhlRepMode');
  var snap = peekZhlRepState();
  if (!(zhlRepEl !== null && zhlRepEl !== void 0 && zhlRepEl.checked) || !snap) return null;
  return {
    tissues: snap.tissues,
    surfaceIntervalMin: sanitizeRepetitiveSurfaceInterval(snap.surfaceIntervalMin, REP_SURFACE_INTERVAL_DEFAULT_MIN),
    surfaceP: (_snap$surfaceP = snap.surfaceP) !== null && _snap$surfaceP !== void 0 ? _snap$surfaceP : altSurfaceP || SEA_LEVEL_P,
    totalCNS: snap.totalCNS,
    totalOTU: snap.totalOTU
  };
}
function takeZhlRepStateSnapshot() {
  return getZhlRepStateForSchedule();
}
function getWaterTypeForVPM() {
  var _document$getElementB76;
  var sel = (_document$getElementB76 = document.getElementById('waterDensitySelect')) === null || _document$getElementB76 === void 0 ? void 0 : _document$getElementB76.value;
  if (sel === 'fresh') return 1;
  if (sel === 'en13319') return 2;
  if (sel === 'salt') return 0;
  if (sel === 'custom') return 3;
  var bpm = BAR_PER_METRE || 0.1;
  if (Math.abs(bpm - WATER_DENSITY.fresh) < 0.0001) return 1;
  if (Math.abs(bpm - WATER_DENSITY.en13319) < 0.0001) return 2;
  if (Math.abs(bpm - WATER_DENSITY.salt) < 0.0001) return 0;
  return 3;
}
function buildZhlScheduleParamsFromEngine(levels, decoGases, settings, profileSplit) {
  return ZhlEngineBundle.buildZhlScheduleParamsFromEngine(levels, decoGases, settings, profileSplit, getZhlEnvironment());
}
function buildZhlScheduleParamsFromDom(rawD, depthM, bt) {
  var _document$getElementB77, _document$getElementB78, _document$getElementB79, _document$getElementB80, _document$getElementB81, _document$getElementB82, _document$getElementB83, _document$getElementB84, _document$getElementB85, _document$getElementB86, _document$getElementB87, _document$getElementB88, _document$getElementB89, _document$getElementB90;
  _travelGasFractionWarning = '';
  var rate = Math.max(1, parseInt((_document$getElementB77 = document.getElementById('ascentRate')) === null || _document$getElementB77 === void 0 ? void 0 : _document$getElementB77.value) || 9);
  var decoRate = Math.max(1, parseInt((_document$getElementB78 = document.getElementById('decoAscentRate')) === null || _document$getElementB78 === void 0 ? void 0 : _document$getElementB78.value) || 9);
  var surfaceRate = Math.max(1, parseInt((_document$getElementB79 = document.getElementById('surfaceAscentRate')) === null || _document$getElementB79 === void 0 ? void 0 : _document$getElementB79.value) || 9);
  var descentRate = Math.max(1, parseInt((_document$getElementB80 = document.getElementById('descentRate')) === null || _document$getElementB80 === void 0 ? void 0 : _document$getElementB80.value) || 22);
  var ppo2Bottom = parseFloat((_document$getElementB81 = document.getElementById('ppo2Bottom')) === null || _document$getElementB81 === void 0 ? void 0 : _document$getElementB81.value) || 1.4;
  var ppo2Deco = parseFloat((_document$getElementB82 = document.getElementById('ppo2Deco')) === null || _document$getElementB82 === void 0 ? void 0 : _document$getElementB82.value) || 1.6;
  var minStopTime = parseFloat((_document$getElementB83 = document.getElementById('minStopTime')) === null || _document$getElementB83 === void 0 ? void 0 : _document$getElementB83.value) || 1;
  var lastStop = parseInt((_document$getElementB84 = document.getElementById('lastDecoStop')) === null || _document$getElementB84 === void 0 ? void 0 : _document$getElementB84.value) || 3;
  var decoStep = parseInt((_document$getElementB85 = document.getElementById('decoStep')) === null || _document$getElementB85 === void 0 ? void 0 : _document$getElementB85.value) || 3;
  var metric = units === 'metric';
  var switchCtx = {
    ppo2Bottom: ppo2Bottom,
    ppo2Deco: ppo2Deco,
    lastStop: lastStop,
    decoStep: decoStep,
    metric: metric
  };
  var _botFracs = getBottomGasFractions();
  if (!_botFracs) {
    throw new Error('Invalid bottom gas: enter valid O₂ and He percentages.');
  }
  var bottomFN2 = _botFracs.fN2;
  var bottomFHe = _botFracs.fHe;
  var bottomFO2 = _botFracs.fO2;
  var bottomMixLabel = getGasLabel(bottomFO2, bottomFHe);
  var _ccrDom = getCCRSettingsFromDOM();
  var _ccrMerged = mergeCCRSettings(_ccrDom);
  var botHypo = ZhlEngineBundle.validateHypoxicDecoGas(bottomFO2 * 100, bottomFHe * 100, 'bottomGas', _ccrMerged.circuit);
  if (botHypo) throw new Error(botHypo.message);
  var dU = metric;
  var decoGases = [];
  var switchDisplays = [];
  var _iterator5 = _createForOfIteratorHelper(getAllDecoGasIds()),
    _step5;
  try {
    for (_iterator5.s(); !(_step5 = _iterator5.n()).done;) {
      var idx = _step5.value;
      var mixEl = document.getElementById("dg".concat(idx, "Mix"));
      if (!mixEl || mixEl.value === 'none' || !mixEl.value) continue;
      var dgf = getDecoCardFractions(idx);
      if (!dgf) {
        throw new Error("Invalid deco gas ".concat(idx, ": enter valid O\u2082 and He percentages."));
      }
      var hypo = ZhlEngineBundle.validateHypoxicDecoGas(dgf.fO2 * 100, dgf.fHe * 100, "dg".concat(idx), _ccrMerged.circuit);
      if (hypo) throw new Error(hypo.message);
      var fN2 = dgf.fN2;
      var fHe = dgf.fHe || 0;
      var fO2 = dgf.fO2;
      var label = getDecoGasLabel("dg".concat(idx, "Mix"), "dg".concat(idx, "CustomO2"));
      var depth = zhlOptimalSwitchDepth(fO2, switchCtx);
      if (depth !== null && fN2 !== null) {
        switchDisplays.push({
          idx: idx,
          text: (dU ? depth + 'm' : mToFt(depth) + 'ft') + '  (ppO₂ ' + ((altSurfaceP + depth * BAR_PER_METRE) * fO2).toFixed(2) + ')'
        });
        decoGases.push({
          depth: depth,
          fN2: fN2,
          fHe: fHe,
          fO2: fO2,
          label: label
        });
      } else {
        switchDisplays.push({
          idx: idx,
          text: '—'
        });
      }
    }
  } catch (err) {
    _iterator5.e(err);
  } finally {
    _iterator5.f();
  }
  decoGases.sort(function (a, b) {
    return b.depth - a.depth;
  });
  var travelInfo = getTravelGasInfo();
  if (travelInfo) {
    var botMODm = bottomFO2 > 0 ? calcGasMODm(bottomFO2, ppo2Bottom) : depthM;
    var resolved = resolveTravelGasFractions(travelInfo);
    if (!resolved.valid) {
      warnTravelGasFractionIssue(resolved.reason);
    } else {
      var travelFO2 = resolved.fO2;
      var travelFHe = resolved.fHe;
      var ppO2AtBotMOD = (altSurfaceP + botMODm * BAR_PER_METRE) * travelFO2;
      if (ppO2AtBotMOD <= ppo2Bottom + 0.01) {
        decoGases.push({
          depth: botMODm,
          fN2: travelInfo.fN2,
          fHe: travelFHe,
          fO2: travelFO2,
          label: travelInfo.label,
          isTravelGas: true
        });
        decoGases.sort(function (a, b) {
          return b.depth - a.depth;
        });
      }
    }
  }
  return {
    params: {
      depthM: depthM,
      bt: bt,
      rawD: rawD,
      metric: metric,
      ascentRate: rate,
      decoAscentRate: decoRate,
      surfaceAscentRate: surfaceRate,
      descentRate: descentRate,
      gfL: mGF.low / 100,
      gfH: mGF.high / 100,
      ppo2Bottom: ppo2Bottom,
      ppo2Deco: ppo2Deco,
      minStopTime: minStopTime,
      switchPauseT: 0,
      mdCompatMode: (((_document$getElementB86 = document.getElementById('decoTransitMode')) === null || _document$getElementB86 === void 0 ? void 0 : _document$getElementB86.value) || 'multideco') === 'multideco',
      wholeMinStops: (((_document$getElementB87 = document.getElementById('stopRounding')) === null || _document$getElementB87 === void 0 ? void 0 : _document$getElementB87.value) || 'fractional') === 'wholeminute',
      lastStop: lastStop,
      decoStep: decoStep,
      shallowGradient: isShallowGradientOn(),
      bottomFN2: bottomFN2,
      bottomFHe: bottomFHe,
      bottomFO2: bottomFO2,
      bottomMixLabel: bottomMixLabel,
      travelInfo: travelInfo,
      repState: takeZhlRepStateSnapshot(),
      continuationLevels: window._zhlHeadless && Array.isArray(window._zhlContinuationLevels) ? window._zhlContinuationLevels : [],
      minDecoProfile: {
        enabled: ((_document$getElementB88 = document.getElementById('minDecoProfileEnable')) === null || _document$getElementB88 === void 0 ? void 0 : _document$getElementB88.value) === 'yes',
        m9: parseFloat((_document$getElementB89 = document.getElementById('minDeco9m')) === null || _document$getElementB89 === void 0 ? void 0 : _document$getElementB89.value) || 1,
        m6: parseFloat((_document$getElementB90 = document.getElementById('minDeco6m')) === null || _document$getElementB90 === void 0 ? void 0 : _document$getElementB90.value) || 3,
        isMetric: units !== 'imperial'
      },
      decoGases: decoGases,
      environment: getZhlEnvironment(),
      ccr: _ccrMerged,
      onLoop: isRebreatherCircuit(_ccrMerged.circuit) && !_ccrMerged.bailout,
      loopMixLabel: loopMixLabelFor(bottomMixLabel, _ccrMerged)
    },
    switchDisplays: switchDisplays
  };
}
function applyZhlSwitchDepthDisplays(switchDisplays) {
  if (!switchDisplays || window._zhlHeadless) return;
  switchDisplays.forEach(function (_ref8) {
    var idx = _ref8.idx,
      text = _ref8.text;
    var swEl = document.getElementById("dg".concat(idx, "SwitchDepthDisplay"));
    if (swEl) swEl.value = text;
  });
}

// AUDIT-UNIT:UI-ZHL-RUNNER-SETUP
function runZhlScheduleCore(params) {
  if (!params.environment) {
    params = Object.assign({}, params, {
      environment: getZhlEnvironment()
    });
  }
  return ZhlEngineBundle.runZhlScheduleCore(params);
}
function nextDecoScheduleGen() {
  window._decoScheduleSeq = (window._decoScheduleSeq || 0) + 1;
  return window._decoScheduleSeq;
}
function isStaleDecoScheduleGen(gen) {
  return gen !== window._decoScheduleSeq;
}
function runDecoSchedule() {
  var _zhlPrevGF = null;
  var scheduleGen = nextDecoScheduleGen();
  _syncZhlBundleEnv();
  window._scheduleWorkerBusy = true;
  try {
    var _getPlannerInputEl2, _getPlannerInputEl3, _document$getElementB91, _document$getElementB92, _document$getElementB93, _document$getElementB94, _document$getElementB95, _document$getElementB96, _document$getElementB97, _document$getElementB98, _document$getElementB99, _document$getElementB100, _document$getElementB101, _document$getElementB102, _document$getElementB103;
    if (!guardEngineBootForCalculate()) return;
    if (!window._zhlHeadless && !_contingencyRunning) {
      var inputVal = validateDecoInputs();
      if (!inputVal.ok) {
        _clearPlannerResults();
        notifyScheduleError(inputVal.msg);
        return;
      }
    }
    if (!window._zhlHeadless && !_contingencyRunning && isCcrGasUiMode()) {
      var gasVal = validateCcrGasConfiguration();
      if (!gasVal.ok) {
        updateCcrGasValidation();
        var err0 = gasVal.errors[0];
        _clearPlannerResults();
        notifyScheduleError(typeof err0 === 'string' ? err0 : (err0 === null || err0 === void 0 ? void 0 : err0.message) || 'CCR gas configuration is invalid.');
        return;
      }
    }
    if (!window._zhlHeadless && !_contingencyRunning) {
      var domGasVal = validateDomDecoGases();
      if (!domGasVal.ok) {
        updateCcrGasValidation();
        var domErr0 = domGasVal.errors[0];
        _clearPlannerResults();
        notifyScheduleError((domErr0 === null || domErr0 === void 0 ? void 0 : domErr0.message) || 'Invalid gas configuration.');
        return;
      }
    }
    var rawD = parseFloat((_getPlannerInputEl2 = getPlannerInputEl('decoDepth')) === null || _getPlannerInputEl2 === void 0 ? void 0 : _getPlannerInputEl2.value) || 40;
    var depthM = units === 'metric' ? rawD : rawD / 3.28084;
    var bt = parseInt((_getPlannerInputEl3 = getPlannerInputEl('decoBT')) === null || _getPlannerInputEl3 === void 0 ? void 0 : _getPlannerInputEl3.value) || 30;
    var bottomFracsForPpo2 = getBottomGasFractions();
    var _uiCcr = getCCRSettingsFromDOM();
    var bottomPpo2Limit = parseFloat((_document$getElementB91 = document.getElementById('ppo2Bottom')) === null || _document$getElementB91 === void 0 ? void 0 : _document$getElementB91.value) || 1.4;
    var zhlBottomPpo2 = validateOcBottomGasPpo2(depthM, bottomFracsForPpo2, bottomPpo2Limit, mergeCCRSettings(_uiCcr), 'Bühlmann');
    if (!zhlBottomPpo2.ok && !window._zhlHeadless && !_contingencyRunning) {
      renderBlockingScheduleError(zhlBottomPpo2.message);
      return;
    }
    if (!window._zhlHeadless && isRebreatherCircuit(_uiCcr.circuit)) {
      var bot = getDomBottomGasPct();
      var ccrVal = validateCcrCalculationInputs([{
        depth: depthM,
        time: bt,
        o2: bot.o2,
        he: bot.he
      }], mergeCCRSettings(_uiCcr), collectDecoGasesPctFromDom());
      if (!ccrVal.ok) {
        var msg = ccrVal.errors[0].message;
        _clearPlannerResults();
        notifyScheduleError(msg);
        return;
      }
    }
    var rate = Math.max(1, parseInt((_document$getElementB92 = document.getElementById('ascentRate')) === null || _document$getElementB92 === void 0 ? void 0 : _document$getElementB92.value) || 9);
    var decoRate = Math.max(1, parseInt((_document$getElementB93 = document.getElementById('decoAscentRate')) === null || _document$getElementB93 === void 0 ? void 0 : _document$getElementB93.value) || 9);
    var surfaceRate = Math.max(1, parseInt((_document$getElementB94 = document.getElementById('surfaceAscentRate')) === null || _document$getElementB94 === void 0 ? void 0 : _document$getElementB94.value) || 9);
    var descentRate = Math.max(1, parseInt((_document$getElementB95 = document.getElementById('descentRate')) === null || _document$getElementB95 === void 0 ? void 0 : _document$getElementB95.value) || 22);
    var gfL = mGF.low / 100;
    var gfH = mGF.high / 100;
    var ppo2Bottom = parseFloat((_document$getElementB96 = document.getElementById('ppo2Bottom')) === null || _document$getElementB96 === void 0 ? void 0 : _document$getElementB96.value) || 1.4;
    var ppo2Deco = parseFloat((_document$getElementB97 = document.getElementById('ppo2Deco')) === null || _document$getElementB97 === void 0 ? void 0 : _document$getElementB97.value) || 1.6;
    var minStopT = parseFloat((_document$getElementB98 = document.getElementById('minStopTime')) === null || _document$getElementB98 === void 0 ? void 0 : _document$getElementB98.value) || 1;
    var switchPauseT = 0;
    var wholeMinStops = (((_document$getElementB99 = document.getElementById('stopRounding')) === null || _document$getElementB99 === void 0 ? void 0 : _document$getElementB99.value) || 'fractional') === 'wholeminute';
    var mdCompatMode = (((_document$getElementB100 = document.getElementById('decoTransitMode')) === null || _document$getElementB100 === void 0 ? void 0 : _document$getElementB100.value) || 'schreiner') === 'multideco';
    updateWaterVapor();
    var ppo2High = ppo2Deco;
    var ppo2Mid = 1.5; // mid-band: 28–44% O2 → 1.5 bar (not ppo2Bottom)
    var ppo2Low = ppo2Bottom;
    // VPM-B: last stop must be 6m when using EAN50+O2 as deco gases
    // ApexDeco uses lastStop=6 for VPM — this determines where critical volume
    // time accumulates. lastStop=3 incorrectly splits time across 3m and 6m.
    var lastStop = parseInt((_document$getElementB101 = document.getElementById('lastDecoStop')) === null || _document$getElementB101 === void 0 ? void 0 : _document$getElementB101.value) || 3;
    var decoStep = parseInt((_document$getElementB102 = document.getElementById('decoStep')) === null || _document$getElementB102 === void 0 ? void 0 : _document$getElementB102.value) || 3;

    // ── VPM-B / VPM-B+GFS branch ──────────────────────────────────────
    var decoModelSel = ((_document$getElementB103 = document.getElementById('algorithmSelect')) === null || _document$getElementB103 === void 0 ? void 0 : _document$getElementB103.value) || 'ZHLC_GF';
    if ((decoModelSel === 'VPMB' || decoModelSel === 'VPMB_GFS') && window.VPMEngine) {
      return runVPMSchedule(depthM, bt, descentRate, rate, decoRate, surfaceRate, decoStep, lastStop, minStopT, ppo2Bottom, ppo2Deco, decoModelSel, wholeMinStops);
    }
    // ── END VPM branch — Bühlmann continues below ─────────────────────

    var _ccrSettings = getCCRSettingsFromDOM();
    var _zhlOnLoop = isRebreatherCircuit(_ccrSettings.circuit) && !_ccrSettings.bailout;
    if (_ccrSettings.bailout) {
      var _ccrSettings$bailoutG, _ccrSettings$bailoutG2;
      _zhlPrevGF = {
        low: mGF.low,
        high: mGF.high
      };
      var boLo = (_ccrSettings$bailoutG = _ccrSettings.bailoutGfLow) !== null && _ccrSettings$bailoutG !== void 0 ? _ccrSettings$bailoutG : 50;
      var boHi = (_ccrSettings$bailoutG2 = _ccrSettings.bailoutGfHigh) !== null && _ccrSettings$bailoutG2 !== void 0 ? _ccrSettings$bailoutG2 : 85;
      mGF.low = boLo;
      mGF.high = boHi;
      gfL = boLo / 100;
      gfH = boHi / 100;
    }
    var bottomFN2, bottomFHe, bottomFO2, bottomMixLabel;
    var diveRuntimeMin = 0;
    function zhlLoadLinear(tissues, from, to, t, fO2, fHe, onLoop, phase) {
      if (onLoop) {
        var out = loadTissuesWithCCR(tissues, from, to, t, fO2, fHe, _objectSpread(_objectSpread({}, _ccrSettings), {}, {
          scrRuntimeMin: diveRuntimeMin,
          ccrPhase: phase
        }));
        diveRuntimeMin += t;
        return out;
      }
      return saturateLinear(tissues, from, to, t, Math.max(0, 1 - fO2 - fHe), fHe);
    }
    function zhlLoadConst(tissues, depth, t, fO2, fHe, onLoop, phase) {
      if (onLoop) {
        var out = loadTissuesWithCCR(tissues, depth, depth, t, fO2, fHe, _objectSpread(_objectSpread({}, _ccrSettings), {}, {
          scrRuntimeMin: diveRuntimeMin,
          ccrPhase: phase
        }));
        diveRuntimeMin += t;
        return out;
      }
      return saturate(tissues, depth, t, Math.max(0, 1 - fO2 - fHe), fHe);
    }

    // ── Bühlmann via Tier-3 unified core (OC + CCR) ───────────────────────
    // AUDIT-UNIT:UI-ZHL-RUNNER-ENGINE
    var _buildZhlSchedulePara = buildZhlScheduleParamsFromDom(rawD, depthM, bt),
      zhlParams = _buildZhlSchedulePara.params,
      switchDisplays = _buildZhlSchedulePara.switchDisplays;
    zhlParams.wholeMinStops = wholeMinStops;
    zhlParams.ccr = mergeCCRSettings(_ccrSettings);
    zhlParams.onLoop = _zhlOnLoop;
    if (_ccrSettings.bailout) {
      zhlParams.gfL = gfL;
      zhlParams.gfH = gfH;
    }
    applyZhlSwitchDepthDisplays(switchDisplays);
    var zhlCore = runZhlScheduleCore(zhlParams);
    var collapsed = zhlCore.collapsed;
    var collapsedMDP = zhlCore.collapsedMDP;
    var tissuesAtBottom = zhlCore.tissuesAtBottom;
    var tissues = zhlCore.lastPlan.finalTissues.map(function (t) {
      return {
        pN2: t.pN2,
        pHe: t.pHe || 0
      };
    });
    var decoStops = zhlCore.decoStops;
    var decoTime = zhlCore.decoTime;
    var hasDeco = zhlCore.hasDeco;
    var gasUsed = zhlCore.gasUsed;
    var descentTime = zhlCore.descentTime;
    var trueDecoZoneStart = zhlCore.trueDecoZoneStart;
    var firstStopDepth = zhlCore.firstStopDepth;
    var gfAt = zhlCore.gfAt;
    bottomFN2 = zhlCore.bottomFN2;
    bottomFHe = zhlCore.bottomFHe;
    bottomFO2 = zhlCore.bottomFO2;
    bottomMixLabel = zhlCore.bottomMixLabel;
    var dU = zhlCore.dU;
    var loopMixLabel = loopMixLabelFor(bottomMixLabel, _ccrSettings);
    var runTimeMin = zhlCore.lastPlan.rt;
    var ttsMin = zhlCore.lastPlan.tts;
    var _headlessExposure = accumulateHeadlessPlanExposure(depthM, bt, descentRate, bottomFN2, bottomFHe, bottomFO2, _zhlOnLoop, collapsedMDP, _ccrSettings, zhlParams.repState);
    window._lastPlan = _objectSpread(_objectSpread({}, zhlCore.lastPlan), {}, {
      totalOTU: _headlessExposure.totalOTU,
      totalCNS: _headlessExposure.totalCNS
    });
    if (!_contingencyRunning && tissues && tissues.length) {
      saveZhlRepState(tissues, normalizeRepetitiveSurfaceIntervalInput('zhlSurfaceInterval', REP_SURFACE_INTERVAL_DEFAULT_MIN), {
        totalCNS: _headlessExposure.totalCNS,
        totalOTU: _headlessExposure.totalOTU
      });
      var zhlRepRow = document.getElementById('zhlRepRow');
      if (zhlRepRow) zhlRepRow.style.display = 'flex';
    }

    // ── Headless mode early return: skip ceiling/graph/DOM work ─────────────
    // Guard moved before ceiling waypoints — the intermediate saturate() calls
    // for graph smoothing are expensive and unnecessary in headless engine tests.
    if (window._zhlHeadless) return;

    // ── Store ceiling waypoints for graph overlay ──
    // Sample ceiling at finer resolution for smooth overlay line
    // Track ceiling from dive start (descent + bottom + deco ascent)
    {
      var ceilWps = [];

      // ── Phase 1: descent (0 → depthM over descentTime) ──
      {
        var descSteps = 8;
        var dTissues = initTissues();
        for (var di = 0; di <= descSteps; di++) {
          var frac = di / descSteps;
          var partDur = descentTime * frac;
          var partDepth = depthM * frac;
          var tis = di === 0 ? dTissues : _zhlOnLoop ? zhlLoadLinear(_toConsumableArray(initTissues()), 0, partDepth, partDur, bottomFO2, bottomFHe, true) : saturateLinear(_toConsumableArray(initTissues()), 0, partDepth, partDur, bottomFN2, bottomFHe);
          var gf = gfAt ? gfAt(partDepth) : gfH;
          var ceilM = Math.max(0, ceiling(tis, gf));
          ceilWps.push({
            t: partDur,
            ceil: ceilM
          });
        }
      }

      // ── Phase 2: bottom time (at depthM, descentTime → bt) ──
      {
        var btDuration = Math.max(0, bt - descentTime);
        var btSteps = Math.min(8, Math.max(2, Math.floor(btDuration)));
        var bTissues = _toConsumableArray(tissuesAtBottom); // end of bottom
        // Reconstruct intermediate bottom states (simplified: just record ceiling at start and end)
        var gfBottom = gfAt ? gfAt(depthM) : gfH;
        var btStartTis = _zhlOnLoop ? zhlLoadLinear(_toConsumableArray(initTissues()), 0, depthM, descentTime, bottomFO2, bottomFHe, true) : saturateLinear(_toConsumableArray(initTissues()), 0, depthM, descentTime, bottomFN2, bottomFHe);
        ceilWps.push({
          t: descentTime,
          ceil: Math.max(0, ceiling(btStartTis, gfBottom))
        });
        ceilWps.push({
          t: bt,
          ceil: Math.max(0, ceiling(bTissues, gfBottom))
        });
      }

      // ── Phase 3: deco ascent (from bt onward through collapsed steps) ──
      var cTissues2 = tissuesAtBottom ? _toConsumableArray(tissuesAtBottom) : null;
      if (cTissues2) {
        var walkT = bt;
        var _iterator6 = _createForOfIteratorHelper(collapsed),
          _step6;
        try {
          for (_iterator6.s(); !(_step6 = _iterator6.n()).done;) {
            var s = _step6.value;
            var _gf = gfAt ? gfAt(s.type === 'ascent' ? s.to : s.depth || 0) : gfH;
            var _ceilM = Math.max(0, ceiling(cTissues2, _gf));
            ceilWps.push({
              t: walkT,
              ceil: _ceilM
            });
            // Intermediate samples for smoother line
            if (s.dur > 1.5) {
              var nSteps = Math.min(Math.floor(s.dur), 8);
              for (var si = 1; si < nSteps; si++) {
                var _frac = si / nSteps;
                var _partDur = s.dur * _frac;
                var midTis = s.type === 'ascent' ? _zhlOnLoop ? zhlLoadLinear(_toConsumableArray(cTissues2), s.from, s.to, _partDur, bottomFO2, bottomFHe, true) : saturateLinear(_toConsumableArray(cTissues2), s.from, s.to, _partDur, s.fN2 != null ? s.fN2 : bottomFN2, s.fHe !== undefined ? s.fHe : bottomFHe) : _zhlOnLoop ? zhlLoadConst(_toConsumableArray(cTissues2), s.depth || 0, _partDur, bottomFO2, bottomFHe, true) : saturate(_toConsumableArray(cTissues2), s.depth || 0, _partDur, s.fN2 != null ? s.fN2 : bottomFN2, s.fHe !== undefined ? s.fHe : bottomFHe);
                var gfMid = gfAt ? gfAt(s.type === 'ascent' ? s.from + (s.to - s.from) * _frac : s.depth || 0) : gfH;
                ceilWps.push({
                  t: walkT + _partDur,
                  ceil: Math.max(0, ceiling(midTis, gfMid))
                });
              }
            }
            if (s.type === 'ascent') {
              cTissues2 = _zhlOnLoop ? zhlLoadLinear(cTissues2, s.from, s.to, s.dur, bottomFO2, bottomFHe, true) : saturateLinear(cTissues2, s.from, s.to, s.dur, s.fN2 != null ? s.fN2 : bottomFN2, s.fHe !== undefined ? s.fHe : bottomFHe);
            } else {
              cTissues2 = _zhlOnLoop ? zhlLoadConst(cTissues2, s.depth || 0, s.dur, bottomFO2, bottomFHe, true) : saturate(cTissues2, s.depth || 0, s.dur, s.fN2 != null ? s.fN2 : bottomFN2, s.fHe !== undefined ? s.fHe : bottomFHe);
            }
            walkT += s.dur;
          }
        } catch (err) {
          _iterator6.e(err);
        } finally {
          _iterator6.f();
        }
        ceilWps.push({
          t: walkT,
          ceil: 0
        }); // surface
      }

      // Sort by time (phases may have out-of-order due to descent reconstruction)
      ceilWps.sort(function (a, b) {
        return a.t - b.t;
      });
      window._decoCeilingWps = ceilWps;

      // Gas color segments: [{fromT, toT, gas, color}]
      // bt is run time including descent — deco phase starts at bt
      var gasColors = [_lspCssVar('--accent', '#22d3ee'), _lspCssVar('--green', '#4ade80'), _lspCssVar('--orange', '#fbbf24'), _lspCssVar('--yellow', '#fbbf24'), '#a78bfa', '#f472b6'];
      var gasColorMap = {};
      var gcIdx = 0;
      var gsT = bt; // deco phase starts at bt (run time)
      var gasSegs = [];
      var _iterator7 = _createForOfIteratorHelper(collapsed),
        _step7;
      try {
        for (_iterator7.s(); !(_step7 = _iterator7.n()).done;) {
          var _s = _step7.value;
          if (!gasColorMap[_s.gas]) {
            gasColorMap[_s.gas] = gasColors[gcIdx++ % gasColors.length];
          }
          gasSegs.push({
            fromT: gsT,
            toT: gsT + _s.dur,
            gas: _s.gas,
            color: gasColorMap[_s.gas]
          });
          gsT += _s.dur;
        }
        // Prepend descent + bottom segment using bottom gas (covers 0 → bt)
      } catch (err) {
        _iterator7.e(err);
      } finally {
        _iterator7.f();
      }
      if (!gasColorMap[loopMixLabel]) {
        gasColorMap[loopMixLabel] = gasColors[gcIdx++ % gasColors.length];
      }
      gasSegs.unshift({
        fromT: 0,
        toT: bt,
        gas: loopMixLabel,
        color: gasColorMap[loopMixLabel]
      });
      window._decoGasSegments = gasSegs;
      window._decoGasColorMap = gasColorMap;
    }
    renderZhlScheduleResults({
      depthM: depthM,
      bt: bt,
      rawD: rawD,
      descentRate: descentRate,
      bottomFN2: bottomFN2,
      bottomFHe: bottomFHe,
      bottomFO2: bottomFO2,
      bottomMixLabel: bottomMixLabel,
      collapsedMDP: collapsedMDP,
      tissues: tissues,
      decoTime: decoTime,
      hasDeco: hasDeco,
      firstStopDepth: firstStopDepth,
      trueDecoZoneStart: trueDecoZoneStart,
      zhlCore: zhlCore,
      zhlOnLoop: _zhlOnLoop,
      ccrSettings: _ccrSettings,
      dU: dU,
      loopMixLabel: loopMixLabel,
      ppo2Deco: ppo2Deco,
      ppo2Bottom: ppo2Bottom
    });

    // Tissue handled by updateTissueViz via tissueInlineCard

    if (!_contingencyRunning) {
      var _scheduleDecoProfileF, _scheduleGfCurveDraw;
      {
        var _tlc = document.getElementById('tissueLoadCard');
        if (_tlc) _tlc.style.display = 'block';
      }
      _syncGraphsSectionHeads();
      // Clear any VPM "N/A" notices from tissue/GF cards
      ['gfCurveInlineCard'].forEach(function (id) {
        // tissueInlineCard/tableCard merged into tissueLoadCard
        var el = document.getElementById(id);
        if (el) el.querySelectorAll('.info-box').forEach(function (n) {
          if (n.textContent.includes('N/A for VPM')) n.remove();
        });
      });
      {
        var _cc = document.getElementById('contingencyCard');
        if (_cc) _cc.style.display = 'block';
      }
      var _fdgc = document.getElementById('fullDiveGraphCard');
      if (_fdgc) _fdgc.style.display = 'block';
      buildContingencyButtons();
      {
        var _cr = document.getElementById('contingencyResult');
        if (_cr) _cr.style.display = 'none';
      }
      (_scheduleDecoProfileF = scheduleDecoProfileFullDraw) === null || _scheduleDecoProfileF === void 0 || _scheduleDecoProfileF(function () {
        return isStaleDecoScheduleGen(scheduleGen);
      });
      (_scheduleGfCurveDraw = scheduleGfCurveDraw) === null || _scheduleGfCurveDraw === void 0 || _scheduleGfCurveDraw(function () {
        return isStaleDecoScheduleGen(scheduleGen);
      });
      // Surface Interval panel (collapsed) — pre-fill Dive 1 from this dive
      renderSurfIntPanel('tecSurfIntContainer', 'tecSi', depthM, bt);
      if (!_contingencyRunning) renderTissueLoadChart();
      {
        var _tBtn = document.getElementById('tissueChartToggleBtn');
        var _cBtn = document.getElementById('contingencyJumpBtn');
        if (_tBtn) _tBtn.style.display = 'inline-block';
        if (_cBtn) _cBtn.style.display = 'inline-block';
      }
    }
    if (_zhlPrevGF) {
      mGF.low = _zhlPrevGF.low;
      mGF.high = _zhlPrevGF.high;
    }
  } catch (err) {
    if (_zhlPrevGF) {
      mGF.low = _zhlPrevGF.low;
      mGF.high = _zhlPrevGF.high;
    }
    if (_contingencyRunning) throw err;
    console.error('[Deco] Error:', err);
    var _ds = document.getElementById('decoSummary');
    if (_ds) _ds.innerHTML = "<div class=\"alert dang\"><span>\u26A0</span><div><strong>Error:</strong> ".concat(escapeHtmlText(err.message || String(err)), "</div></div>");
    var _dr2 = document.getElementById('decoResult');
    if (_dr2) _dr2.style.display = 'block';
  } finally {
    window._scheduleWorkerBusy = false;
  }
}

// Shared OTU/CNS integration from plan segments (headless harness + pSCR validation).
function planSegDepthM(seg) {
  if (seg.depth != null && isFinite(seg.depth)) return seg.depth;
  if (seg.endDepth != null && isFinite(seg.endDepth)) return seg.endDepth;
  if (seg.startDepth != null && seg.endDepth != null) return (seg.startDepth + seg.endDepth) / 2;
  if (seg.from != null && seg.to != null) return (seg.from + seg.to) / 2;
  if (seg.startDepth != null) return seg.startDepth;
  return 0;
}
function planSegDurationMin(seg) {
  return seg.time != null ? seg.time : seg.dur || 0;
}
function planSegRunEndMin(seg) {
  return seg.run != null ? seg.run : seg.runtime != null ? seg.runtime : 0;
}
function addExposureSample(otu, cnsFrac, ppO2, dur) {
  if (dur <= 0) return;
  if (ppO2 > 0.5) {
    otu.val += dur * Math.pow((ppO2 - 0.5) / 0.5, OTU_EXPONENT);
  }
  if (ppO2 < 0.6) return;
  var cnsLimits = {
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
    16: 45,
    17: 45
  };
  var lo = Math.min(Math.floor(ppO2 * 10), 17);
  var hi = Math.min(lo + 1, 17);
  var lim = (cnsLimits[lo] || 0) + ((cnsLimits[hi] || 0) - (cnsLimits[lo] || 0)) * (ppO2 * 10 - lo);
  cnsFrac.val += dur / (lim > 0 ? lim : 45);
}

/** Headless Bühlmann OTU/CNS — build plan from collapsed steps and delegate to computePlanExposureTotals. */
function accumulateHeadlessPlanExposure(depthM, bt, descentRate, bottomFN2, bottomFHe, bottomFO2, onLoop, collapsedSteps, ccrSettings, repState) {
  var _document$getElementB104;
  var _pdCarry = window._priorDiveCarry;
  var _zhlRepSnap = repState || ((_document$getElementB104 = document.getElementById('zhlRepMode')) === null || _document$getElementB104 === void 0 ? void 0 : _document$getElementB104.checked) && peekZhlRepState() || null;
  var otuCarry = _pdCarry ? _pdCarry.otuCarry || 0 : 0;
  var cnsCarry = _pdCarry ? _pdCarry.cnsCarry || 0 : 0;
  if (_zhlRepSnap && !_pdCarry) {
    if (_zhlRepSnap.totalOTU != null) otuCarry = _zhlRepSnap.totalOTU;
    if (_zhlRepSnap.totalCNS != null) cnsCarry = _zhlRepSnap.totalCNS;
  }
  var descentTimeMin = depthM / descentRate;
  var btAtDepthMin = Math.max(0, bt - descentTimeMin);
  var o2pct = Math.round(bottomFO2 * 100);
  var hepct = Math.round((bottomFHe || 0) * 100);
  var gasLabel = collapsedSteps && collapsedSteps[0] && collapsedSteps[0].gas || 'bottom';
  var plan = [];
  var runAccum = 0;
  if (descentTimeMin > 0) {
    runAccum += descentTimeMin;
    plan.push({
      type: 'descent',
      depth: depthM,
      time: descentTimeMin,
      run: runAccum,
      o2: o2pct,
      he: hepct,
      gas: gasLabel
    });
  }
  if (btAtDepthMin > 0) {
    runAccum += btAtDepthMin;
    plan.push({
      type: 'bottom',
      depth: depthM,
      time: btAtDepthMin,
      run: runAccum,
      o2: o2pct,
      he: hepct,
      gas: gasLabel
    });
  }
  (collapsedSteps || []).forEach(function (s) {
    var _s$fN, _s$fHe, _s$fHe2;
    if (s.decoTransit) return; // mdCompatMode: transit folded into stop display
    var dur = typeof s.dur === 'number' ? s.dur : parseFloat(s.dur) || 0;
    if (dur <= 0) return;
    runAccum += dur;
    plan.push({
      type: s.type === 'deco' ? 'stop' : s.type,
      depth: s.type === 'ascent' ? s.to : s.depth,
      from: s.type === 'ascent' ? s.from : undefined,
      to: s.type === 'ascent' ? s.to : undefined,
      time: dur,
      run: Math.round(runAccum * 10) / 10,
      pO2: s.pO2,
      o2: Math.round((1 - ((_s$fN = s.fN2) !== null && _s$fN !== void 0 ? _s$fN : bottomFN2) - ((_s$fHe = s.fHe) !== null && _s$fHe !== void 0 ? _s$fHe : 0)) * 100),
      he: Math.round(((_s$fHe2 = s.fHe) !== null && _s$fHe2 !== void 0 ? _s$fHe2 : 0) * 100),
      gas: s.gas
    });
  });
  var exposure = computePlanExposureTotals(plan, ccrSettings || {}, bottomFO2, bottomFHe, altSurfaceP || 1.01325, BAR_PER_METRE || 0.1);
  return {
    totalOTU: Math.round(exposure.totalOTU + otuCarry),
    totalCNS: parseFloat((exposure.totalCNS + cnsCarry).toFixed(1))
  };
}
function computePlanExposureTotals(plan, settings, defaultFO2, defaultFHe, surfP, barPerM) {
  var otu = {
      val: 0
    },
    cnsFrac = {
      val: 0
    };
  var cfg = mergeCCRSettings(settings || {});
  function segFrac(val, fallback) {
    if (val == null) return fallback;
    return val > 1 ? val / 100 : val;
  }
  (plan || []).forEach(function (seg) {
    var dur = planSegDurationMin(seg);
    if (dur <= 0) return;
    var depth = planSegDepthM(seg);
    var fo2 = segFrac(seg.o2, defaultFO2);
    var fh = segFrac(seg.he, defaultFHe);
    var runEnd = planSegRunEndMin(seg);
    var runStart = Math.max(0, runEnd - dur);
    var pAmbBase = surfP || altSurfaceP || 1.01325;
    var bar = barPerM || BAR_PER_METRE || 0.1;
    var onLoop = isRebreatherCircuit(cfg.circuit) && !cfg.bailout;
    var baked = seg.pO2 != null ? parseFloat(seg.pO2) : NaN;
    if (isFinite(baked) && baked > 0) {
      addExposureSample(otu, cnsFrac, baked, dur);
      return;
    }
    if (onLoop && cfg.circuit === 'pSCR') {
      var steps = Math.max(1, Math.min(Math.ceil(dur), 12));
      for (var i = 0; i < steps; i++) {
        var subDur = dur / steps;
        var frac = (i + 0.5) / steps;
        var rt = runStart + frac * dur;
        var d = seg.type === 'ascent' && seg.from != null && seg.to != null ? seg.from + (seg.to - seg.from) * frac : depth;
        var _pAmb = pAmbBase + d * bar;
        var _ppO = getEffectivePpo2(_pAmb, 0, fo2, _objectSpread(_objectSpread({}, cfg), {}, {
          scrRuntimeMin: rt,
          bailout: false
        }), d, fh);
        addExposureSample(otu, cnsFrac, _ppO, subDur);
      }
      return;
    }
    var pAmb = pAmbBase + depth * bar;
    var ppO2;
    if (onLoop && cfg.circuit === 'CCR') {
      var phase = seg.type === 'deco' || seg.type === 'stop' ? 'deco' : seg.type === 'descent' ? 'descent' : seg.type === 'bottom' ? 'bottom' : 'ascent';
      var sp = getEffectiveSetpointAtDepth(depth, cfg, surfP || altSurfaceP, phase);
      ppO2 = getEffectivePpo2(pAmb, sp, fo2, cfg, depth, fh);
    } else {
      ppO2 = fo2 * pAmb;
    }
    addExposureSample(otu, cnsFrac, ppO2, dur);
  });
  return {
    totalOTU: Math.round(otu.val),
    totalCNS: parseFloat((cnsFrac.val * 100).toFixed(1))
  };
}
if (typeof window !== 'undefined') window.computePlanExposureTotals = computePlanExposureTotals;

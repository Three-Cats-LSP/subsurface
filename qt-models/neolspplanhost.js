// SPDX-License-Identifier: GPL-2.0
// Neo data/geometry adapter. All loading, ceilings, schedules and oxygen
// calculations below call the pinned LSP implementation.
NeoLspPlanner.project = function(input) {
    function fail(code, message) {return {error:message, code:code, planSaveAllowed:false};}
    function copy(value) {return JSON.parse(JSON.stringify(value));}
    function plainGas(label) {return String(label).replace(/^(CCR|pSCR)\s+/i, '').replace(/^0+(?=\d)/, '').toUpperCase();}
    function sameGas(a, b) {return Math.abs(a.o2-b.o2)<0.0001 && Math.abs((a.he||0)-(b.he||0))<0.0001;}
    if (!input || input.schema !== 1 || !input.engineRequest || !Array.isArray(input.cylinders) || !input.cylinders.length)
        return fail('INVALID_INPUT', 'Complete Neo planner input is required.');
    var request = copy(input.engineRequest), s=request.settings, env=request.environment;
    var prior = input.priorProfiles || [], state=null, lastEnd=null, carryCns=0, carryOtu=0;
    ZhlEngineBundle.setHeHalfTimeMode(s.heHalfTimeMode);
    if (!Array.isArray(prior) || prior.length>128) return fail('PRIOR_PROFILE', 'Prior dive chain exceeds the supported calculation bound.');
    if (prior.length && request.model!=='ZHLC_GF')
        return fail('PRIOR_BUBBLE_STATE', 'LSP VPM repetitive loading requires a verified bubble-state history. Use the existing Subsurface profile for this history.');
    for (var previous of prior) {
        if (!previous.environment || !Array.isArray(previous.segments) || !previous.segments.length || previous.segments.length>32768 ||
                !Number.isFinite(previous.start) || !Number.isFinite(previous.end) || previous.end<=previous.start ||
                (lastEnd!==null && previous.start<lastEnd))
            return fail('PRIOR_PROFILE', 'Prior sampled dives are incomplete or overlap.');
        ZhlEngineBundle.applyEnvironment(previous.environment);
        if (state===null) state=ZhlEngineBundle.initTissues();
        else {
            var gap=(previous.start-lastEnd)/60;
            state=ZhlEngineBundle.saturate(state,0,gap,0.7902,0);
            carryCns*=Math.pow(0.5,gap/90);if(gap>=1440)carryOtu=0;
        }
        var priorOtu={val:0},priorCns={val:0};
        var previousRuntime=0, previousDepth=0;
        for (var seg of previous.segments) {
            if (!Number.isFinite(seg.time) || seg.time<=0 || !Number.isFinite(seg.from) || !Number.isFinite(seg.to) ||
                    seg.from<0 || seg.to<0 || Math.abs(seg.from-previousDepth)>1e-7 || !Number.isFinite(seg.o2) || !Number.isFinite(seg.he) ||
                    seg.o2<=0 || seg.he<0 || seg.o2+seg.he>100 || ['OC','CCR'].indexOf(seg.circuit)<0 ||
                    (seg.circuit==='CCR' && !(seg.setpoint>0)))
                return fail('PRIOR_PROFILE', 'A prior sample has unknown gas, circuit, setpoint or timing.');
            var cfg={circuit:seg.circuit, setpoint:seg.setpoint, descentSetpoint:seg.setpoint,
                bottomSetpoint:seg.setpoint, decoSetpoint:seg.setpoint, bailout:false};
            state=ZhlEngineBundle.loadTissuesWithCCR(state,seg.from,seg.to,seg.time,seg.o2/100,seg.he/100,cfg,seg.from===seg.to);
            var meanDepth=(seg.from+seg.to)/2;
            var pp=ZhlEngineBundle.getEffectivePpo2(previous.environment.altSurfaceP+meanDepth*previous.environment.barPerMetre,
                seg.setpoint,seg.o2/100,cfg,meanDepth,seg.he/100);
            addExposureSample(priorOtu,priorCns,pp,seg.time);
            previousRuntime+=seg.time*60;previousDepth=seg.to;
        }
        if (Math.abs(previousRuntime-(previous.end-previous.start))>0.01 || previousDepth>0.75)
            return fail('PRIOR_PROFILE', 'A prior dive does not include a complete sampled return to the surface.');
        lastEnd=previous.end;
        carryCns+=priorCns.val*100;carryOtu+=priorOtu.val;
    }
    if (state!==null) {
        var interval=(input.start-lastEnd)/60;
        if (!Number.isFinite(interval) || interval<0 || interval>10080)
            return fail('PRIOR_PROFILE', 'The sampled surface interval is outside LSP repetitive-state limits.');
        s._preTissues=state;s._surfaceInterval=interval;
        // LSP's existing prior-dive policy: CNS 90-minute half-life; OTU
        // resets after a 24-hour interval (index.html prior-dive carry).
        s.totalCNSCarry=carryCns*Math.pow(0.5,interval/90);
        s.totalOTUCarry=interval>=1440?0:carryOtu;
    }
    var raw=this.calculate(request);
    if (!raw || raw.error) return raw || fail('ENGINE_ERROR','No LSP result.');
    var trace=request.model==='ZHLC_GF' ? ZhlEngineBundle.neoNativeTrace() : null;
    if (request.model==='ZHLC_GF' && (!trace || trace.hitSafetyGuard))
        return fail('ENGINE_GUARD','LSP could not prove a complete ascent within its calculation guard.');
    var physical=[];
    var first=request.levels[0], descent=first.depth/s.descentRate;
    if (trace) {
        if (s.travelInfo) return fail('TRAVEL_PROFILE','Explicit travel-gas geometry requires the Subsurface profile until its LSP adapter is validated.');
        physical.push({type:'descent',from:0,to:first.depth,time:descent,o2:first.o2,he:first.he});
        physical.push({type:'bottom',depth:first.depth,time:Math.max(0,first.time-descent),o2:first.o2,he:first.he,_tissues:trace.tissuesAtBottom});
        for (var step of trace.steps) physical.push({type:step.type,from:step.from,to:step.to,depth:step.depth,
            time:step.nativeDuration!==undefined?step.nativeDuration:step.dur,
            o2:100*(1-step.fN2-(step.fHe||0)),he:100*(step.fHe||0),_tissues:step._tissues});
    } else physical=raw.plan;
    var cylinders=input.cylinders, gasVolumes=cylinders.map(function(){return {usedLitres:0,decoLitres:0};});
    for (var ci=0;ci<cylinders.length;ci++) {
        var c=cylinders[ci];
        if (c.role===3) continue;
        if (!(c.sizeLitres>0 && c.startBar>0) || !Number.isFinite(c.o2) || !Number.isFinite(c.he))
            return fail('CYLINDER','Each active cylinder needs a known volume, fill pressure and gas.');
        for(var cj=0;cj<ci;cj++) if(cylinders[cj].role!==3 && sameGas(c,cylinders[cj]) && c.role===cylinders[cj].role)
            return fail('AMBIGUOUS_CYLINDER','Use one combined cylinder entry for identical gas supplies, or the existing Subsurface profile.');
    }
    var runtime=0, depth=0, timeline=[], schedule=[], profile=[{time:0,depth:0,gas:'',ndl:-1}], bottomRuntime=0, totalDeco=0;
    var onLoop=isRebreatherCircuit(s.circuit)&&!s.bailout, missingOxygen=false;
    var oldSettings=getCCRSettingsFromDOM, oldBottom=getBottomGasFractions, oldBailout=getConfiguredBailoutMixes;
    getCCRSettingsFromDOM=function(){return s;};
    try {
        for (var segment of physical) {
            var dur=segment.time||0;
            if (!Number.isFinite(dur) || dur<0) return fail('GEOMETRY','Invalid physical segment duration.');
            if (!dur) continue;
            var to=segment.to!==undefined?segment.to:segment.endDepth!==undefined?segment.endDepth:segment.depth;
            var gas={o2:segment.o2,he:segment.he};
            if (!Number.isFinite(gas.o2) || !Number.isFinite(gas.he)) {
                for(var known of cylinders)if(plainGas(ZhlEngineBundle.getGasLabel(known.o2/100,known.he/100))===plainGas(segment.gas)) {
                    gas={o2:known.o2,he:known.he};break;
                }
            }
            if (!Number.isFinite(to) || to<0 || !Number.isFinite(gas.o2) || !Number.isFinite(gas.he))
                return fail('GEOMETRY','Missing physical depth or gas.');
            var index=-1;
            for(var cidx=0;cidx<cylinders.length;cidx++) {
                var candidate=cylinders[cidx];
                if(candidate.role!==3 && candidate.role!==2 && sameGas(gas,candidate) && (!onLoop || candidate.role===1 || s.circuit==='pSCR')) {index=cidx;break;}
            }
            if(index<0) return fail('UNAVAILABLE_GAS','LSP selected a gas without an available matching cylinder.');
            var phase=segment.type==='stop' || segment.type==='deco' ? 'deco' : segment.type==='safety' ? 'safety' : segment.type==='bottom' ? 'level' : segment.type;
            var label=ZhlEngineBundle.getGasLabel(gas.o2/100,gas.he/100);
            if(onLoop) label=loopMixLabelFor(label,s);
            getBottomGasFractions=function(){return {fO2:gas.o2/100,fHe:gas.he/100};};
            var mean=(depth+to)/2;
            var sac=phase==='level'||phase==='descent'?input.bottomSacLpm:input.decoSacLpm;
            if (!(sac>0)) return fail('GAS_SETTINGS','Positive bottom and decompression SAC are required.');
            var litres=ccrGasLitres(label,mean,dur,sac);
            if (!Number.isFinite(litres)||litres<0) return fail('GAS_SETTINGS','LSP could not compute gas consumption.');
            gasVolumes[index].usedLitres+=litres;
            if(phase==='deco'||phase==='ascent'||phase==='safety')gasVolumes[index].decoLitres+=litres;
            if(onLoop&&s.circuit==='CCR') {
                var oxygenIndex=-1;
                for(var oi=0;oi<cylinders.length;oi++)if(cylinders[oi].role===2&&cylinders[oi].o2===100&&cylinders[oi].he===0){oxygenIndex=oi;break;}
                if(oxygenIndex<0)missingOxygen=true;
                else gasVolumes[oxygenIndex].usedLitres+=ZhlEngineBundle.getCcrMetabolicO2Rate(s)*dur;
            }
            var previousSeconds=Math.round(runtime*60);runtime+=dur;
            if(phase==='level')bottomRuntime=runtime;
            var setpoint=ZhlEngineBundle.getEffectiveSetpointAtDepth(to,s,env.altSurfaceP,phase==='level'?'bottom':phase);
            var po2=ZhlEngineBundle.getEffectivePpo2(env.altSurfaceP+to*env.barPerMetre,setpoint,gas.o2/100,
                Object.assign({},s,{scrRuntimeMin:runtime}),to,gas.he/100);
            var row={depth:Math.round(to*1000),duration:Math.round(runtime*60)-previousSeconds,runTime:Math.round(runtime*60),gas:label,
                cylinder:index,gasSwitch:timeline.length>0&&timeline[timeline.length-1].cylinder!==index,phase:phase,
                entered:phase==='level'||phase==='descent',setpoint:Math.round(setpoint*1000),po2:Math.round(po2*1000)};
            timeline.push(row);
            if(phase==='deco'){schedule.push(copy(row));totalDeco+=dur*60;}
            var point={time:row.runTime,depth:row.depth,gas:label,gasSwitch:row.gasSwitch,setpoint:row.setpoint,po2:row.po2,
                ndl:-1,tts:0,inDeco:phase==='deco'};
            if(trace&&segment._tissues) {
                point.surfaceGf=ZhlEngineBundle.computeSurfaceGF(segment._tissues);
                point.ceiling=Math.round(ZhlEngineBundle.ceiling(segment._tissues,s.gfHi/100)*1000);
            } else if(!trace&&segment._ceilingDepth!==undefined)point.ceiling=Math.round(segment._ceilingDepth*1000);
            profile.push(point);depth=to;
        }
        if(onLoop && (s.stressTimeMin||0)+(s.problemSolveMin||0)>0) {
            var bailoutMixes=[];
            for(var bi=0;bi<cylinders.length;bi++) {
                var bc=cylinders[bi];
                if(bc.role!==0 && !(s.diluentUseAsBailout&&bc.role===1))continue;
                bailoutMixes.push({index:bi,label:ZhlEngineBundle.getGasLabel(bc.o2/100,bc.he/100),fO2:bc.o2/100,modM:calcGasMODm(bc.o2/100,s.ppO2Bottom,s.lastStop)});
            }
            if(!bailoutMixes.some(function(m){return m.modM>=first.depth-0.01;}))
                return fail('BAILOUT_SUPPLY','Add a breathable OC bailout supply for the configured stress/problem-solving reserve.');
            getConfiguredBailoutMixes=function(){return bailoutMixes;};
            var loopSettings=s;
            getCCRSettingsFromDOM=function(){return Object.assign({},loopSettings,{circuit:'OC',bailout:true});};
            addBailoutStressReserve(function(label,durationDepth,minutes,sac){
                var chosen=bailoutMixes.filter(function(m){return m.label===label;})[0];
                if(!chosen)throw new Error('Missing configured bailout cylinder.');
                gasVolumes[chosen.index].usedLitres+=ccrGasLitres(label,durationDepth,minutes,sac);
            },(s.stressTimeMin||0)+(s.problemSolveMin||0),first.depth,schedule.map(function(row){return row.depth/1000;}),
                first.o2/100,first.he/100,s.sacStress||input.bottomSacLpm);
        }
    } finally {getCCRSettingsFromDOM=oldSettings;getBottomGasFractions=oldBottom;getConfiguredBailoutMixes=oldBailout;}
    for(var gi=0;gi<cylinders.length;gi++) {
        gasVolumes[gi].bottomModMetres=calcGasMODm(cylinders[gi].o2/100,s.ppO2Bottom,s.lastStop);
        gasVolumes[gi].decoModMetres=calcGasMODm(cylinders[gi].o2/100,s.ppO2Deco,s.lastStop);
    }
    if(depth>0.001 || (trace&&Math.abs(runtime-trace.runtimeMinutes)>1e-7) || (!trace&&Math.abs(runtime-raw.totalRuntime)>1.01))
        return fail('GEOMETRY','The LSP physical profile does not reconcile with its computed ascent.');
    var seconds=Math.round(runtime*60), analysis={};
    for(var point of profile){point.tts=Math.max(0,seconds-point.time);if(point.time<=Math.round(bottomRuntime*60))analysis=point;}
    var final=profile[profile.length-1];final.cns=raw.totalCNS;
    if(trace){final.surfaceGf=ZhlEngineBundle.computeSurfaceGF(trace.finalTissues);final.ceiling=Math.round(ZhlEngineBundle.ceiling(trace.finalTissues,s.gfHi/100)*1000);}
    var warnings=[];
    if(missingOxygen)warnings.push('Add a pure-oxygen supply cylinder for this CCR plan.');
    var exceedsNDL=!!input.noDecompression && schedule.length>0;
    if(exceedsNDL)warnings.push('The requested recreational profile requires decompression.');
    return {error:null,engineRequest:request,raw:raw,profile:profile,timeline:timeline,schedule:schedule,analysis:analysis,
        gasVolumes:gasVolumes,runtimeSeconds:seconds,bottomTimeSeconds:Math.round(bottomRuntime*60),decoTimeSeconds:Math.round(totalDeco),
        surfaceIntervalSeconds:state!==null?Math.round(s._surfaceInterval*60):null,
        otu:raw.totalOTU,cns:raw.totalCNS,exceedsNDL:exceedsNDL,planSaveAllowed:!exceedsNDL&&!missingOxygen,warnings:warnings};
};

NeoLspPlanner.ndlReference = function(input) {
    if(!input||!input.request||!Array.isArray(input.depthsMetres)||input.depthsMetres.length>16)
        return {error:'Enter a valid calculated profile for the NDL reference.'};
    var request=input.request;
    // Reuse the engine's complete request guards, including explicit pressure,
    // gas fractions and tissue state, before running its fresh-tissue NDL helper.
    var checked=this.calculate(request);
    if(checked.error)return {error:checked.error,rows:[]};
    var s=request.settings,env=request.environment;
    var available=request.model==='ZHLC_GF'&&s.circuit==='OC'&&!s._preTissues;
    var rows=[];
    if(available) {
        ZhlEngineBundle.applyEnvironment(env);ZhlEngineBundle.setHeHalfTimeMode(s.heHalfTimeMode);
    }
    for(var depth of input.depthsMetres) {
        if(!Number.isFinite(depth)||depth<=0||depth>500)return {error:'Invalid NDL reference depth.'};
        var minutes=available?ZhlEngineBundle.buhNDL(depth,1-request.levels[0].o2/100-request.levels[0].he/100,
            s.gfLo,s.gfHi,request.levels[0].he/100,s.lastStop,s.stepSize,s.shallowGradient):-1;
        rows.push({depthMetres:depth,ndl:minutes<0?-1:minutes*60,available:available,capped:minutes===500});
    }
    return {error:null,rows:rows};
};

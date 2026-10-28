// SPDX-License-Identifier: GPL-2.0
.pragma library

// Display-unit adaptation only. Keep the complete LSP settings in SI so loading
// an imperial display cannot round a preset's actual ascent/descent rates.
var fields = {planner_gflow:"gfLowInput",planner_gfhigh:"gfHighInput",vpmb_conservatism:"conservatism",bottomsac:"sacBottom",decosac:"sacDeco",reserve_gas:"reserveBar",bottompo2:"ppo2Bottom",decopo2:"ppo2Deco",descrate:"descentRate",ascrate75:"ascentRate",ascratestops:"decoAscentRate",ascratelast6m:"surfaceAscentRate",default_setpoint:"bottomSetpoint",dobailout:"bailout",problemsolvingtime:"problemSolveMin",o2narcotic:"o2NarcSel"}

function read(name, values, metric, bar, litres, enums) {
    if(name==="planner_deco_mode")return values.recreational?enums.RECREATIONAL:String(values.model||"ZHLC_GF").indexOf("VPM")===0?enums.VPMB:enums.BUEHLMANN
    if(name==="last_stop6m")return Number(values.lastDecoStop||3)===6
    var key=fields[name],value=key?values[key]:undefined
    if(value===undefined)return undefined
    if(name==="dobailout")return !!value
    if(name==="o2narcotic")return value==="yes"
    value=Number(value)
    if(name==="bottompo2"||name==="decopo2")return value*100
    if(name==="default_setpoint")return value*1000
    if(name==="reserve_gas")return bar?value:value*14.5037738
    if(name==="bottomsac"||name==="decosac")return litres?value:value*3.53146667
    if(["descrate","ascrate75","ascratestops","ascratelast6m"].indexOf(name)>=0)return metric?value:value/0.3048
    return value
}

function write(name, value, values, metric, bar, litres, enums) {
    var next=JSON.parse(JSON.stringify(values))
    if(name==="planner_deco_mode") {
        next.recreational=value===enums.RECREATIONAL;next.model=value===enums.VPMB?"VPMB":"ZHLC_GF";return next
    }
    if(name==="last_stop6m"){next.lastDecoStop=value?6:3;return next}
    var key=fields[name];if(!key)return next
    if(name==="default_setpoint")value=Number(value)/1000
    if(name==="reserve_gas"&&!bar)value=Number(value)/14.5037738
    if((name==="bottomsac"||name==="decosac")&&!litres)value=Number(value)/3.53146667
    if(["descrate","ascrate75","ascratestops","ascratelast6m"].indexOf(name)>=0&&!metric)value=Number(value)*0.3048
    if(name==="o2narcotic")value=value?"yes":"no"
    next[key]=value;return next
}

function presetParameters(source) {
    var values={model:"ZHLC_GF",recreational:false,conservatism:0,reserveBar:40,descentSetpoint:0.7,bottomSetpoint:1.3,decoSetpoint:1.3,scrLoopVolume:8,scrMetabolicO2:1,stressTimeMin:10,problemSolveMin:3,sacStress:50,diluentUseAsBailout:false,acclimatized:true}
    for(var key in source)values[key]=source[key]
    return values
}

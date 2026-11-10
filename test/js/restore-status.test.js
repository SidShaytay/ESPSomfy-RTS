const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const source=fs.readFileSync(require.resolve('../../data-src/index.js'),'utf8');
const cls=source.slice(source.indexOf('class Firmware {'),source.indexOf('var firmware = new Firmware();'));
function fixture(){
    const requests=[],timers=new Map(),instructions={textContent:''},listeners=[];
    let timer=0,refused=0,refresh=0;
    const context={console:{log(){}},baseUrl:'',security:{apiKey:'test-secret',tokenRefused(){++refused;}},
        somfy:{async init(){++refresh;}},
        setTimeout(fn){const id=++timer;timers.set(id,fn);return id;},clearTimeout(id){timers.delete(id);}};
    context.XMLHttpRequest=class{constructor(){requests.push(this);this.upload={};}open(method,url){this.method=method;this.url=url;}setRequestHeader(name,value){this.header=[name,value];}send(){}abort(){this.aborted=true;}};
    const Firmware=vm.runInNewContext(cls+'\nFirmware',context),firmware=new Firmware();context.firmware=firmware;
    const field={value:'current.backup',files:[{size:1024}]};
    const widget={style:{setProperty(){}},addEventListener(_,fn){listeners.push(fn);}};
    const el={querySelector(selector){return selector.startsWith('input')?field:selector.includes('divInstText')?instructions:widget;}};
    context.FormData=class{append(){}get(){return "{}";}};
    const identity={bootSession:'9007199254740993',generation:'9007199254740995'};
    const start=()=>firmware.watchRestore(el,identity);
    const reply=(data,code=200)=>{const req=requests.at(-1);req.status=code;req.responseText=JSON.stringify({...identity,...data});req.onload();};
    const next=()=>{const [id,fn]=timers.entries().next().value;timers.delete(id);fn();};
    return {requests,timers,instructions,listeners,identity,start,reply,next,firmware,el,get refused(){return refused;},get refresh(){return refresh;}};
}
test('restore waits for matching terminal result, preserving exact decimal identity and authentication',async()=>{
    const h=fixture();h.start();assert.equal(h.requests.length,1);assert.match(h.requests[0].url,/9007199254740993.*9007199254740995/);
    assert.deepEqual(h.requests[0].header,['apikey','test-secret']);
    h.reply({phase:'queued',desc:'Queued',dataComplete:false});assert.equal(h.refresh,0);assert.equal(h.timers.size,1);
    h.next();h.reply({phase:'cleanupPending',desc:'Cleanup pending',dataComplete:true,modelApplied:true});assert.equal(h.refresh,0);
    h.next();h.reply({phase:'applied',desc:'Applied; restart blocked',dataComplete:true,modelApplied:true,restartScheduled:false});
    await Promise.resolve();assert.equal(h.refresh,1);assert.equal(h.timers.size,0);assert.match(h.instructions.textContent,/restart blocked/);
});
test('stale job, auth refusal and malformed identities never refresh, replay or restart',()=>{
    for(const kind of ['stale','auth','mismatch']){
        const h=fixture();h.start();
        if(kind==='stale')h.reply({desc:'Stale'},409);
        else if(kind==='auth')h.reply({},401);
        else h.reply({phase:'applied',generation:'9007199254740996',dataComplete:true,modelApplied:true});
        assert.equal(h.refresh,0);assert.equal(h.timers.size,0);assert.equal(h.requests.length,1);assert.equal(h.refused,kind==='auth'?1:0);
    }
    for(const generation of ['0','01','18446744073709551616',9007199254740995]){
        const h=fixture();h.firmware.watchRestore(h.el,{...h.identity,generation});assert.equal(h.requests.length,0);
    }
});
test('Close ends observation only, and pending polling is bounded',()=>{
    const closed=fixture();closed.start();closed.listeners[0]();assert.equal(closed.requests[0].aborted,true);assert.equal(closed.refresh,0);
    const h=fixture();h.start();for(let i=0;i<120;++i){h.reply({phase:'queued',desc:'Pending'});if(h.timers.size)h.next();}
    assert.equal(h.requests.length,120);assert.equal(h.timers.size,0);assert.equal(h.refresh,0);assert.match(h.instructions.textContent,/not confirmed|still pending/i);
});

test('actual restore upload 202 starts observation without announcing or refreshing completion',async()=>{
    const h=fixture();await h.firmware.uploadFile('/restore',h.el,{shades:true});
    assert.equal(h.requests.length,1);assert.equal(h.requests[0].method,'POST');
    h.reply({status:'QUEUED',phase:'queued'},202);
    assert.equal(h.requests.length,2);assert.equal(h.requests[1].method,'GET');assert.equal(h.refresh,0);
});

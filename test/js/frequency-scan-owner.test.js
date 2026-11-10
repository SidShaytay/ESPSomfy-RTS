const test = require('node:test'), assert = require('node:assert/strict');
const fs = require('node:fs'), vm = require('node:vm');
const source = fs.readFileSync(require.resolve('../../data-src/index.js'), 'utf8');
const cls = source.slice(source.indexOf('class Somfy {'), source.indexOf('var somfy = new Somfy();'));
function fixture() {
 const nodes=new Map(),calls=[],errors=[];
 const element=()=>({attributes:{},style:{},classList:{add(){}},innerHTML:'',value:0,
  setAttribute(k,v){this.attributes[k]=String(v);if(k==='id')nodes.set(v,this);},getAttribute(k){return this.attributes[k];},
  appendChild(){},remove(){if(nodes.get(this.attributes.id)===this)nodes.delete(this.attributes.id);}});
 for(const key of ['divRadioSettings','spanTestFreq','spanTestRSSI','spanBestFreq','spanBestRSSI','btnStopScanning','btnRestartScanning','btnCopyFrequency','btnCloseScanning','slidFrequency'])nodes.set(key,element());
 const context={console:{log(){}},document:{getElementById(id){return nodes.get(id);},createElement(){return element();}},
  sendOp(name,data,cb){calls.push({name,data,cb});},ui:{serviceError(error){errors.push(error);},infoMessage(error){errors.push(error);}}};
 const Somfy=vm.runInNewContext('Number.prototype.fmt=function(){return String(this);};\n'+cls+'\nSomfy',context),somfy=new Somfy();context.somfy=somfy;
 somfy.frequencyChanged=()=>{++somfy.drafts;};somfy.drafts=0;
 return {somfy,nodes,calls,errors};
}
test('scan Stop binds exact decimal ticket and Copy requests a fresh owned result',()=>{
 const h=fixture();h.somfy.scanFrequency(true);
 h.calls[0].cb(null,{scanToken:'9007199254740993.9007199254740995'});
 h.somfy.stopScanningFrequency(true);
 assert.equal(h.calls[1].data.scanToken,'9007199254740993.9007199254740995');
 h.calls[1].cb(null,{});assert.notEqual(h.nodes.get('btnCopyFrequency').style.display,'');
 h.somfy.setScannedFrequency();assert.equal(h.calls.at(-1).name,'getFrequencyScanResult');assert.equal(h.somfy.drafts,0);
 h.calls.at(-1).cb(null,{frequency:433.42});assert.equal(h.somfy.drafts,1);assert.equal(h.nodes.get('slidFrequency').value,433420);
});
test('foreign feeds cannot open a dialog and tokenless Stop sends nothing',()=>{
 const h=fixture();h.somfy.procFrequencyScan({scanToken:'1.2',scanning:true});assert.equal(h.nodes.has('divScanFrequency'),false);
 h.somfy.scanFrequency(true);h.somfy.stopScanningFrequency(true);assert.equal(h.calls.length,1);
});
test('duplicate pending Start sends once; late accepted Start on closed dialog is stopped with its own ticket',()=>{
 const h=fixture();const div=h.somfy.scanFrequency(true);h.somfy.scanFrequency(true);assert.equal(h.calls.length,1);
 div.remove();h.calls[0].cb(null,{scanToken:'42.9'});assert.equal(h.calls[1].name,'endFrequencyScan');assert.equal(h.calls[1].data.scanToken,'42.9');
});

test('matching final feed enables review; foreign feed cannot replace own results',()=>{
 const h=fixture();h.somfy.scanFrequency(true);h.calls[0].cb(null,{scanToken:'42.9'});
 const frame={scanToken:'42.9',scanning:0,testFreq:433.42,testRSSI:-60,frequency:433.43,RSSI:-55};
 h.somfy.procFrequencyScan({...frame,scanToken:'42.10'});assert.notEqual(h.nodes.get('btnCopyFrequency').style.display,'');
 h.somfy.procFrequencyScan(frame);assert.equal(h.nodes.get('btnCopyFrequency').style.display,'');
 assert.equal(h.nodes.get('divScanFrequency').getAttribute('data-frequency'),'433.43');
 h.somfy.procFrequencyScan({...frame,scanToken:'42.10',frequency:433.99});assert.equal(h.nodes.get('divScanFrequency').getAttribute('data-frequency'),'433.43');
});

test('refused Start offers Close and explicit retry without an automatic restart',()=>{
 const h=fixture();h.somfy.scanFrequency(true);h.calls[0].cb({desc:'Busy'});
 assert.equal(h.nodes.get('btnStopScanning').style.display,'none');
 assert.equal(h.nodes.get('btnCloseScanning').style.display,'');
 assert.equal(h.nodes.get('btnRestartScanning').style.display,'');
 assert.equal(h.calls.length,1);assert.equal(h.errors.length,1);
 h.somfy.scanFrequency(true);assert.equal(h.calls.length,2);
});

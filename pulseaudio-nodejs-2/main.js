const pulse_worker = require('./build/Release/pulse_worker.node');

console.log(pulse_worker);
pulse_worker.onChunk((chunk)=>{
    console.log(chunk);
    //let floats = new Flaot32Array(chunk);
    //console.log(chunk.length);
    //console.loga(float)
    
});

setInterval(()=>{
    console.log("some concurrent task");
},1000);
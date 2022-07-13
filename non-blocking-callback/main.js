const testAddon = require('./build/Release/callback-worker.node')
testAddon.registerCallback(1,()=>{
    console.log("callback called");
});
setInterval(()=>{
    console.log("js interval");
},1000);
module.exports = testAddon;

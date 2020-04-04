const bigInt = require("big-integer");
const assert = require("assert");

module.exports = buildMontgomeryMul;

function buildMontgomeryMul(fn, q) {

    const n64 = Math.floor((q.bitLength() - 1) / 64)+1;
    let code = [];
    const canOptimizeConsensys = q.shiftRight((n64-1)*64).leq( bigInt.one.shiftLeft(64).minus(1).shiftRight(1).minus(1) );
    const base = bigInt.one.shiftLeft(64);
    const np64 = base.minus(q.modInv(base));

    const t=4;
    let wrAvailable;
    let wrAssignments = [];
    let pushedRegs = [];

    let nUsedRegs;
    let regRefs = [];
    let neededRegs;

    // const availableRegs = ["tmp0", "tmp1", "np64", "r0", "t0"];
    // const pushableRegs = ["t1", "t2", "t3", "t4", "wc"];
    // const allRegs = ["rdi", "rdx", "rcx", "rsi", "tmp0", "tmp1", "np64", "r0", "t0","t1", "t2", "t3", "t4", "wc"];

    const availableRegs = ["rax", "r8", "r9", "r10", "r11"];
    const pushableRegs = ["rbp", "r12", "r13", "r14", "r15", "rbx"];
    const allRegs = ["rax", "rbx", "rcx", "rdx", "rdi", "rsi", "rsp", "rbp", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"];

    function startFn() {

        neededRegs = t + n64 + 2 - (canOptimizeConsensys ? 1 : 0);
        if (neededRegs > availableRegs.length + pushableRegs.length) {
            nUsedRegs = availableRegs.length + pushableRegs.length -1;
            wrAvailable = [pushableRegs[pushableRegs.length-1]];
        } else {
            nUsedRegs = neededRegs;
            wrAvailable = null;
        }
        for (let i=0; i<neededRegs; i++) {
            if (i < availableRegs.length) {
                regRefs[i] = availableRegs[i];
            } else if (i<nUsedRegs) {
                regRefs[i] = pushableRegs[i-availableRegs.length];
            } else {
                regRefs[i] = `[rsp + ${(i-nUsedRegs)*8  }]`;
            }
            if (   (i>=availableRegs.length)
                &&(i<availableRegs.length+pushableRegs.length))
            {
                code.push(`    push ${pushableRegs[i-availableRegs.length]}`);
                pushedRegs.push(pushableRegs[i-availableRegs.length])
            }
        }
        if (neededRegs>nUsedRegs) {
            code.push(`    sub rsp, ${(neededRegs-nUsedRegs)*8}`);
        }
    }

    function finishFn() {
        if (neededRegs>nUsedRegs) {
            code.push(`    add rsp, ${(neededRegs-nUsedRegs)*8}`);
        }
        for (let i=pushedRegs.length-1; i>=0; i--) {
            code.push(`    pop ${pushedRegs[i]}`);
        }
    }

    function findWrAssignment(n) {
        for (let i=0; i<wrAssignments.length; i++) {
            if (wrAssignments[i].ref == n) return i;
        }
        return -1;
    }

    function flushWr(removeAssignments) {
        for (let i=wrAssignments.length-1; i>=0; i--) {
            if (wrAssignments[i].modified) {
                code.push(`    mov ${regRefs[wrAssignments[i].ref]}, ${wrAssignments[i].reg}`);
                wrAssignments[i].modified = false;
            }
            if (removeAssignments) {
                wrAvailable.push(wrAssignments[i].reg);
                wrAssignments.pop();
            }
        }
    }

    function loadWr(n, loadValue) {
        const idx = findWrAssignment(n);
        if (idx>=0) {
            wrAssignments.push(wrAssignments.splice(idx, 1)[0]); // Move it to the end
        } else {
            if (wrAvailable.length == 0) {
                if (wrAssignments[0].modified) {
                    code.push(`    mov ${regRefs[wrAssignments[0].ref]}, ${wrAssignments[0].reg}`);
                }
                wrAvailable.push(wrAssignments[0].reg);
                wrAssignments.shift();
            }
            const a = {
                reg: wrAvailable.shift(),
                ref: n,
            };
            wrAssignments.push(a);
            if (loadValue) {
                code.push(`    mov ${a.reg}, ${regRefs[n]}`);
            }
        }

        return wrAssignments[wrAssignments.length-1];
    }


    function op3(inst, ...args) {
        let ref = [];
        let preload = 0;
        for (let i=0; i < 3; i++) {
            if (typeof args[i] !== "undefined") {
                if (typeof args[i] === "string") {
                    ref[i] = args[i];
                } else {
                    const idx = findWrAssignment(i);
                    if (idx >= 0) {
                        ref[i] = wrAssignments[idx] .reg;
                    } else {
                        ref[i] = regRefs[args[i]];
                    }
                }
                if (i==1) {
                    const isReg = (allRegs.indexOf(ref[i]) >=0);
                    if (!isReg) preload = 2;
                }
            } else {
                ref[i] = false;
            }
        }

        if (preload == 0) {
            code.push(buildInst(inst, ref[0], ref[1], ref[2] ));
        } else if (preload == 2) {
            const wr = loadWr(args[1], false);
            code.push(buildInst(inst, ref[0], wr.reg, ref[2] ));
            wr.modified=true;
        } else if (preload == 2) {
            assert(false);
        }

        function buildInst(inst, r1, r2, r3) {
            let sdbg = "(none)";
            if (wrAssignments.length > 0) sdbg = regRefs[wrAssignments[0].ref];
            // let SS = sdbg + " " + inst;
            let SS = "    " + inst;

            if (r1) SS += " " + r1;
            if (r2) SS += "," + r2;
            if (r3) SS += "," + r3;
            return SS;
        }
    }

    function op(inst, ...args) {

        if (!["adcx", "adox", "sub", "sbb", "cmp","mov", "mulx"].indexOf(inst) < 0) {
            console.log(inst);
            assert(false );
        }

        code.push("");

        if (args[2]) return op3(inst, ...args);

        const mustbereg = [
            ["adcx", "adox"],
            [],
            []
        ];

        let ref = [];
        let preload = 0;
        let dUsed = false;
        for (let i=0; i < 2; i++) {
            if (typeof args[i] !== "undefined") {
                if (typeof args[i] === "string") {
                    ref[i] = args[i];
                } else {
                    const idx = findWrAssignment(i);
                    if (idx >= 0) {
                        ref[i] = wrAssignments[idx] .reg;
                    } else {
                        ref[i] = regRefs[args[i]];
                    }
                }
                const isReg = (allRegs.indexOf(ref[i]) >=0);
                if (!isReg) {
                    if (  dUsed  || mustbereg[i].indexOf(inst)>=0) {
                        if ((i==1)&&(typeof args[i] === "string")) {
                            preload = 1;
                        } else {
                            preload = (1 << i);
                        }
                    } else {
                        dUsed = true;
                    }
                }
            } else {
                ref[i] = false;
            }
        }

        if (preload == 0) {
            code.push(buildInst(inst, ref[0], ref[1], ref[2] ));
        } else if (preload == 1) {
            const wr = loadWr(args[0], ["adcx", "adox", "sub", "sbb", "cmp"].indexOf(inst) >=0 );
            code.push(buildInst(inst, wr.reg, ref[1], ref[2] ));
            if (["adcx", "adox", "sbb"].indexOf(inst) >= 0) {
                wr.modified=true;
            }
        } else if (preload == 2) {
            const wr = loadWr(args[1], true);
            code.push(buildInst(inst, ref[0], wr.reg, ref[2] ));
        } else {
            assert(false);
        }

        function buildInst(inst, r1, r2) {
            let sdbg = "(none)";
            if (wrAssignments.length > 0) sdbg = regRefs[wrAssignments[0].ref];
            // let SS = sdbg + " " + inst;
            let SS = "    " + inst;
            if (r1) SS += " " + r1;
            if (r2) SS += "," + r2;
            return SS;
        }
    }

    code.push(fn+":");
    startFn();
    op("mov","rcx","rdx");   // rdx is needed for multiplications so keep it in cx
    
    op("mov", 2, `0x${np64.toString(16)}`);
    op("xor", 3, 3);    

    code.push("");
    for (let i=0; i<n64; i++) {     
        code.push("; FirstLoop");
        op("mov","rdx", `[rsi + ${i*8}]`);
        if (i==0) {                                   
            op("mulx", 0, t, "[rcx]"); 
            for (let j=1; j<n64; j++) {               
                op("mulx", j%2, t+j, `[rcx +${j*8}]`);
                op("adcx", t+j, (j-1)%2);
            }
            if (!canOptimizeConsensys) {
                op("mov", t+n64, 3);
                op("adcx", t+n64 , (n64-1)%2);
                op("mov", t+n64+1, 3);
                op("adcx", t+n64+1, 3);
            } else {
                op("mov", t+n64, 3);
                op("adcx", t+n64 , (n64-1)%2);
            }                                   
        } else {     
            if (!canOptimizeConsensys) {
                op("mov", t+n64+1, 3);
            } else {
                op("mov", t+n64, 3);
            }                                                                                  
            for (let j=0; j<n64; j++) {               
                op("mulx", 1, 0, `[rcx +${j*8}]`);
                op("adcx", t+j, 0);
                op("adox", t+j+1, 1);
            }
            if (!canOptimizeConsensys) {
                op("adcx", t+n64, 3);
                op("adcx", t+n64+1, 3);
                op("adox", t+n64+1, 3);
            } else {
                op("adcx", t+n64, 3);
            }                                                 
        }                                             

        code.push("; SecondLoop");              
        op("mov", "rdx", 2);
        op("mulx", 0, "rdx", t);
        op("mulx", 1, 0, "[q]");
        op("adcx", 0, t);
        for (let j=1; j<n64; j++) {                 
            op("mulx", (j+1)%2, t+j-1, `[q +${j*8}]`);
            op("adcx", t+j-1, j%2);
            op("adox", t+j-1, t+j);
        }
        op("mov", t+n64-1, 3);
        op("adcx", t+n64-1, n64%2);
        op("adox", t+n64-1, t+n64);
        if (!canOptimizeConsensys) {                 
            op("mov", t+n64, 3);
            op("adcx", t+n64, 3);
            op("adox", t+n64, t+n64+1);
        }

        code.push("");                                                           
    }                                                 

    code.push(";comparison");
    flushWr(false);
    if (!canOptimizeConsensys) {                 
        op("test", t+n64, t+n64);
        op("jnz", fn+"_sq");
    }
    for (let i=n64-1; i>=0; i--) {
        op("cmp", t+i, `[q + ${i*8}]`);
        op("jc", fn+"_done");
        op("jnz", fn+"_sq");
    }

    code.push(fn+ "_sq:");
    flushWr(true);
    for (let i=0; i<n64; i++) {
        op(i==0 ? "sub" : "sbb", t+i, `[q +${i*8}]`);
    }
    flushWr(true);

    code.push(fn+ "_done:");
    flushWr(true);
    wrAssignments = [];
    for (let i=0; i<n64; i++) {
        op("mov" ,  `[rdi + ${i*8}]`, t+i);
    }

    finishFn();
    op("ret");

    return code.join("\n");
}         

// const code = buildMontgomeryMul("Fr_rawMul", bigInt("21888242871839275222246405745257275088548364400416034343698204186575808495617"));
// const code = buildMontgomeryMul("Fr_rawMul", bigInt("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", 16));
// const code = buildMontgomeryMul("Fr_rawMul", bigInt("4002409555221667393417789825735904156556882819939007885332058136124031650490837864442687629129015664037894272559787"));


// const code = buildMontgomeryMul("Fr_rawMul", bigInt("41898490967918953402344214791240637128170709919953949071783502921025352812571106773058893763790338921418070971888458477323173057491593855069696241854796396165721416325350064441470418137846398469611935719059908164220784476160001"));



// console.log(code);




/*
mulx    rax, <%t0>, [rcx + <%=0%>]

mulx    r8, <%t1>, [rcx + <%=1%>]
adc     <%t1>, rax

mulx    rax, <%t2>, [rcx + <%=2%>]
adc     <%t2>, r8

mulx    rax, <%t[n-1]>, [rcx + <%=2%>]
adc     <%t[n-1]>, r8

adc     rax, 0
mov     <%t[n]>, rax

...if(bigPrime)  adc, <%t[n+1]%>, 0

// Subsequent
mulx    rax, r8, [rcx + <%=0%>]
adcx     <%t[0]>, r8
adox     <%t[1]>, rax

mulx    rax, r8, [rcx + <%=0%>]
adcx     <%t[1]>, r8
adox     <%t[2]>, rax
.
.
mulx    rax, r8, [rcx + <%=0%>]
adcx     <%t[n-1]>, r8
adox     <%t[n]>, rax

adcx     <%t[n]>, 0
...ifBigPrime a   adox <%t[n+1]>, 0
*/


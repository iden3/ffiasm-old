var tmp = require("tmp-promise");
const path = require("path");
const util = require("util");
const fs = require("fs");
const { performance } = require("perf_hooks");
const exec = util.promisify(require("child_process").exec);
const buildZqField = require("../index.js").buildZqField;
const bigInt = require("big-integer");
const N = 1000000000;

async function benchmarkMM(prime) {
    const dir = await tmp.dir({prefix: "circom_", unsafeCleanup: true });
    
    const source = await buildZqField(prime, "Fr");

    // console.log(dir.path);

    await fs.promises.writeFile(path.join(dir.path, "fr.asm"), source.asm, "utf8");
    await fs.promises.writeFile(path.join(dir.path, "fr.h"), source.h, "utf8");
    await fs.promises.writeFile(path.join(dir.path, "fr.c"), source.c, "utf8");

    await exec(`cp  ${path.join(__dirname,  "main.c")} ${dir.path}`);

    if (process.platform === "darwin") {
        await exec("nasm -fmacho64 --prefix _ " +
            ` ${path.join(dir.path,  "fr.asm")}`
        );
    }  else if (process.platform === "linux") {
        await exec("nasm -felf64 " +
            ` ${path.join(dir.path,  "fr.asm")}`
        );
    } else throw("Unsupported platform");

    await exec("g++" +
       ` ${path.join(dir.path,  "main.c")}` +
       ` ${path.join(dir.path,  "fr.o")}` +
       ` ${path.join(dir.path,  "fr.c")}` +
       ` -o ${path.join(dir.path, "benchmark")}` +
       " -lgmp -g"
    );

    const t1 = performance.now();

    await exec(`${path.join(dir.path,  "benchmark")} ${N}`);

    const t2 = performance.now();

    return t2-t1;
}

async function run() {
    let t;

    t = await benchmarkMM(bigInt("21888242871839275222246405745257275088548364400416034343698204186575808495617"));
    console.log("Multiplication bn256r Montgomery IntelASM: " + (t/1000) + "s " + (t * 1e6 / N) + "ns per multiplication.");

    t = await benchmarkMM(bigInt("4002409555221667393417789825735904156556882819939007885332058136124031650490837864442687629129015664037894272559787"));
    console.log("Multiplication bls12-381 Montgomery IntelASM: " + (t/1000) + "s " + (t * 1e6 / N) + "ns per multiplication.");

    t = await benchmarkMM(bigInt("41898490967918953402344214791240637128170709919953949071783502921025352812571106773058893763790338921418070971888253786114353726529584385201591605722013126468931404347949840543007986327743462853720628051692141265303114721689601"));
    console.log("Multiplication mnt6753 Montgomery IntelASM: " + (t/1000) + "s " + (t * 1e6 / N) + "ns per multiplication.");

}

run();

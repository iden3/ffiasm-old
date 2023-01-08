const tester = require("./tester/buildzqfieldtester.js").testField;
const generateTester = require("./tester/buildzqfieldtester.js").generateTester;

const ZqField = require("ffjavascript").ZqField;

const bigInt = require("big-integer");
const tmp = require("tmp-promise");

const bn128q = new bigInt("21888242871839275222246405745257275088696311157297823662689037894645226208583");
const bn128r = new bigInt("21888242871839275222246405745257275088548364400416034343698204186575808495617");
const bls12_381q = new bigInt("4002409555221667393417789825735904156556882819939007885332058136124031650490837864442687629129015664037894272559787");
const secp256k1q = new bigInt("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEFFFFFC2F", 16);
const secp256k1r = new bigInt("FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFEBAAEDCE6AF48A03BBFD25E8CD0364141", 16);
const mnt6753q = new bigInt("41898490967918953402344214791240637128170709919953949071783502921025352812571106773058893763790338921418070971888458477323173057491593855069696241854796396165721416325350064441470418137846398469611935719059908164220784476160001");
const mnt6753r = new bigInt("41898490967918953402344214791240637128170709919953949071783502921025352812571106773058893763790338921418070971888253786114353726529584385201591605722013126468931404347949840543007986327743462853720628051692141265303114721689601");
const gl = new bigInt("FFFFFFFF00000001", 16);

const ops = {
    "add": 2,
    "sub": 2,
    "neg": 1,
    "square": 1,
    "mul": 2,
    "eq": 2,
    "neq": 2,
    "lt": 2,
    "gt": 2,
    "leq": 2,
    "geq": 2,
    "land": 2,
    "lor": 2,
    "lnot": 1,
    "idiv": 2,
    "inv": 1,
    "div": 2,
    "shl": 2,
    "shr": 2,
    "band": 2,
    "bor": 2,
    "bxor": 2,
    "bnot": 1,
};

describe("field gl asm test", function () {
    this.timeout(1000000000);
    generateTest(gl, "gl");
});

describe("field bn128 asm test", function () {
    this.timeout(1000000000);
    generateTest(bn128r, "bn128");
});

function generateTest(curve, name) {
    let testerDir;

    before(async () => {
        console.log("generating " + name + " tester");
        tmp.setGracefulCleanup();
        testerDir = await tmp.dir({prefix: "ffiasm_", unsafeCleanup: true});
        await generateTester(curve, testerDir);
        console.log("generation finished");
    });

    for (const op in ops) {
        it(name + " " + op, async () => {
            let tv;
            if (ops[op] === 1) {
                tv = buildTestVector1(curve, op);
            } else {
                tv = buildTestVector2(curve, op);
            }
            await tester(curve, name, op, tv, testerDir);
        });
    }
}

function buildTestVector2(p, op) {
    const F = new ZqField(p);
    const tv = [];
    const nums = getCriticalNumbers(p, 2);

    const excludeZero = ["div", "mod", "idiv"].indexOf(op) >= 0;

    for (let i = 0; i < nums.length; i++) {
        for (let j = 0; j < nums.length; j++) {
            if ((excludeZero) && (nums[j][0].isZero())) continue;
            tv.push([
                [nums[i][1], nums[j][1], op],
                F[op](nums[i][0], nums[j][0])
            ]);
        }
    }

    return tv;
}

function buildTestVector1(p, op) {
    const F = new ZqField(p);
    const tv = [];
    const nums = getCriticalNumbers(p, 2);

    const excludeZero = ["inv"].indexOf(op) >= 0;

    for (let i = 0; i < nums.length; i++) {
        if ((excludeZero) && (nums[i][0].isZero())) continue;
        tv.push([
            [nums[i][1], op],
            F[op](nums[i][0])
        ]);
    }

    return tv;
}

function getCriticalNumbers(p, lim) {
    const numbers = [];

    addFrontier(0);
    addFrontier(bigInt(32));
    addFrontier(bigInt(64));
    addFrontier(bigInt(p.bitLength()));
    addFrontier(bigInt.one.shiftLeft(31));
    addFrontier(p.minus(bigInt.one.shiftLeft(31)));
    addFrontier(bigInt.one.shiftLeft(32));
    addFrontier(p.minus(bigInt.one.shiftLeft(32)));
    addFrontier(bigInt.one.shiftLeft(63));
    addFrontier(p.minus(bigInt.one.shiftLeft(63)));
    addFrontier(bigInt.one.shiftLeft(64));
    addFrontier(p.minus(bigInt.one.shiftLeft(64)));
    addFrontier(bigInt.one.shiftLeft(p.bitLength() - 1));
    addFrontier(p.shiftRight(1));

    function addFrontier(f) {
        for (let i = -lim; i <= lim; i++) {
            let n = bigInt(f).add(bigInt(i));
            n = n.mod(p);
            if (n.isNegative()) n = p.add(n);
            addNumber(n);
        }
    }

    return numbers;

    function addNumber(n) {
        if (n.lt(bigInt("80000000", 16))) {
            addShortPositive(n);
            addShortMontgomeryPositive(n);
        }
        if (n.geq(p.minus(bigInt("80000000", 16)))) {
            addShortNegative(n);
            addShortMontgomeryNegative(n);
        }
        addLongNormal(n);
        addLongMontgomery(n);

        function addShortPositive(a) {
            numbers.push([a, "0x" + a.toString(16)]);
        }

        function addShortMontgomeryPositive(a) {
            let S = "0x" + bigInt("40", 16).shiftLeft(56).add(a).toString(16);
            S = S + "," + getLongString(toMontgomery(a));
            numbers.push([a, S]);
        }

        function addShortNegative(a) {
            const b = bigInt("80000000", 16).add(a.minus(p.minus(bigInt("80000000", 16))));
            numbers.push([a, "0x" + b.toString(16)]);
        }

        function addShortMontgomeryNegative(a) {
            const b = bigInt("80000000", 16).add(a.minus(p.minus(bigInt("80000000", 16))));
            let S = "0x" + bigInt("40", 16).shiftLeft(56).add(b).toString(16);
            S = S + "," + getLongString(toMontgomery(a));
            numbers.push([a, S]);
        }

        function addLongNormal(a) {
            let S = "0x" + bigInt("80", 16).shiftLeft(56).toString(16);
            S = S + "," + getLongString(a);
            numbers.push([a, S]);
        }


        function addLongMontgomery(a) {

            let S = "0x" + bigInt("C0", 16).shiftLeft(56).toString(16);
            S = S + "," + getLongString(toMontgomery(a));
            numbers.push([a, S]);
        }

        function getLongString(a) {
            if (a.isZero()) {
                return "0x0";
            }
            let r = a;
            let S = "";
            while (!r.isZero()) {
                if (S != "") S = S + ",";
                S += "0x" + r.and(bigInt("FFFFFFFFFFFFFFFF", 16)).toString(16);
                r = r.shiftRight(64);
            }
            return S;
        }

        function toMontgomery(a) {
            const n64 = Math.floor((p.bitLength() - 1) / 64) + 1;
            const R = bigInt.one.shiftLeft(n64 * 64);
            return a.times(R).mod(p);
        }

    }
}


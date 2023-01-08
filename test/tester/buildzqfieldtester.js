const chai = require("chai");
const assert = chai.assert;

const fs = require("fs");
var tmp = require("tmp-promise");
const path = require("path");
const util = require("util");
const exec = util.promisify(require("child_process").exec);

const buildZqField = require("../../index.js").buildZqField;

module.exports.generateTester = generateTester;
module.exports.testField = testField;

async function  generateTester(prime, dir) {
    const source = await buildZqField(prime, "Fr");

    // console.log(dir.path);

    await fs.promises.writeFile(path.join(dir.path, "fr.asm"), source.asm, "utf8");
    await fs.promises.writeFile(path.join(dir.path, "fr.hpp"), source.hpp, "utf8");
    await fs.promises.writeFile(path.join(dir.path, "fr.cpp"), source.cpp, "utf8");

    await exec(`cp  ${path.join(__dirname,  "tester.cpp")} ${dir.path}`);

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
        ` ${path.join(dir.path,  "tester.cpp")}` +
        ` ${path.join(dir.path,  "fr.o")}` +
        ` ${path.join(dir.path,  "fr.cpp")}` +
        ` -o ${path.join(dir.path, "tester")}` +
        " -lgmp -g"
    );
}

async function testField(prime, name, opName, test, testerDir) {
    //tmp.setGracefulCleanup();

    //const dir = await tmp.dir({prefix: "ffiasm_", unsafeCleanup: true });

    const testVectorDir = path.join(__dirname, "testvectors", name);
    if (!fs.existsSync(testVectorDir)) {
        fs.mkdirSync(testVectorDir);
    }

    const inLines = [];
    for (let i=0; i<test.length; i++) {
        for (let j=0; j<test[i][0].length; j++) {
            inLines.push(test[i][0][j]);
        }
    }
    inLines.push("");

    await fs.promises.writeFile(path.join(testVectorDir, opName + ".in.tst"), inLines.join("\n"), "utf8");

    await exec(`${path.join(testerDir.path, "tester")}` +
        ` <${path.join(testVectorDir, opName + ".in.tst")}` +
        ` >${path.join(testVectorDir, opName + ".out.tst")}`);

    const res = await fs.promises.readFile(path.join(testVectorDir, opName + ".out.tst"), "utf8");
    const resLines = res.split("\n");

    for (let i=0; i<test.length; i++) {
        const expected = test[i][1].toString();
        const calculated = resLines[i];

        if (calculated != expected) {
            console.log("FAILED");
            for (let j=0; j<test[i][0].length; j++) {
                console.log(test[i][0][j]);
            }
            console.log("Should Return: " + expected);
            console.log("But Returns: " + calculated);
        }

        assert.equal(calculated, expected);
    }

}


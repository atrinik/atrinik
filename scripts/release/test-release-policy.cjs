"use strict";

const assert = require("node:assert/strict");
const {
  nextVersion,
  releaseForCommit,
  rulesForBranch,
} = require("./release-policy.cjs");

assert.equal(
  releaseForCommit("main", {type: "docs", breaking: false}),
  "minor",
);
assert.equal(nextVersion("8.0.0", "minor"), "8.1.0");
assert.equal(
  releaseForCommit("8.3.x", {type: "fix", breaking: false}),
  "patch",
);
assert.equal(nextVersion("8.3.0", "patch"), "8.3.1");
assert.equal(
  releaseForCommit("8.3.x", {type: "feat", breaking: false}),
  "minor",
);
assert.equal(
  releaseForCommit("main", {type: "fix", breaking: true}),
  "major",
);
assert.throws(() => rulesForBranch("release"), /unsupported/);

process.env.ATRINIK_RELEASE_BRANCH = "main";
delete require.cache[require.resolve("./release-rules.cjs")];
assert.equal(require("./release-rules.cjs").at(-1).release, "minor");
process.env.ATRINIK_RELEASE_BRANCH = "8.3.x";
delete require.cache[require.resolve("./release-rules.cjs")];
assert.equal(require("./release-rules.cjs").at(-1).release, "patch");

console.log("release policy: mainline and maintenance transitions verified");

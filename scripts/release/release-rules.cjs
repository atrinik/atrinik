"use strict";

const {currentBranch, rulesForBranch} = require("./release-policy.cjs");

module.exports = rulesForBranch(currentBranch());

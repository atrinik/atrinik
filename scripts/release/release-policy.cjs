"use strict";

const MAINTENANCE_BRANCH = /^[0-9]+\.[0-9]+\.x$/;
const RELEASE_RANK = {patch: 1, minor: 2, major: 3};

const MAIN_RULES = [
  {breaking: true, release: "major"},
  {type: "feat", release: "minor"},
  {type: "*", release: "minor"},
];

const MAINTENANCE_RULES = [
  {breaking: true, release: "major"},
  {type: "feat", release: "minor"},
  {type: "*", release: "patch"},
];

function currentBranch() {
  const refName = process.env.GITHUB_REF_NAME;
  const branch =
    process.env.ATRINIK_RELEASE_BRANCH ||
    (typeof refName === "string" && /^\d+\/merge$/.test(refName)
      ? process.env.GITHUB_BASE_REF || "main"
      : refName || process.env.GITHUB_HEAD_REF);
  if (typeof branch !== "string" || branch.length === 0) {
    throw new Error("ATRINIK_RELEASE_BRANCH is required");
  }
  return branch;
}

function rulesForBranch(branch) {
  if (branch === "main") {
    return MAIN_RULES.map((rule) => ({...rule}));
  }
  if (MAINTENANCE_BRANCH.test(branch)) {
    return MAINTENANCE_RULES.map((rule) => ({...rule}));
  }
  throw new Error("unsupported semantic-release branch: " + branch);
}

function releaseForCommit(branch, commit) {
  const matches = rulesForBranch(branch).filter((rule) => {
    if (rule.breaking === true) {
      return commit.breaking === true;
    }
    return rule.type === "*" || rule.type === commit.type;
  });
  if (matches.length === 0) {
    return null;
  }
  return matches.reduce((release, rule) =>
    RELEASE_RANK[rule.release] > RELEASE_RANK[release] ? rule.release : release,
  "patch");
}

function nextVersion(value, release) {
  const match = /^([0-9]+)\.([0-9]+)\.([0-9]+)$/.exec(value);
  if (!match || !Object.hasOwn(RELEASE_RANK, release)) {
    throw new Error("version or release level is invalid");
  }
  let [major, minor, patch] = match.slice(1).map(Number);
  if (release === "major") {
    major += 1;
    minor = 0;
    patch = 0;
  } else if (release === "minor") {
    minor += 1;
    patch = 0;
  } else {
    patch += 1;
  }
  return major + "." + minor + "." + patch;
}

module.exports = {
  MAINTENANCE_BRANCH,
  currentBranch,
  nextVersion,
  releaseForCommit,
  rulesForBranch,
};

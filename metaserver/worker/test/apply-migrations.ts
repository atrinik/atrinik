import { env } from "cloudflare:workers";
import { applyD1Migrations } from "cloudflare:test";
import type { D1Migration } from "cloudflare:test";

await applyD1Migrations(env.DB, env.TEST_MIGRATIONS);

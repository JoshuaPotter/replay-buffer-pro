import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

/** Absolute path of the repository root; the site lives in its site/ folder. */
export const repoRoot = execFileSync('git', ['rev-parse', '--show-toplevel'], { encoding: 'utf8' }).trim();

/** Reads a file by repo-relative path, so files outside site/ (buildspec.json) need no Vite import. */
export function readRepoFile(path: string): string {
  return readFileSync(join(repoRoot, path), 'utf8');
}

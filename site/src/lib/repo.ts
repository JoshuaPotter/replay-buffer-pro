import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { join } from 'node:path';

/** Absolute path of the repository root; the site lives in its site/ folder. */
export const repoRoot = execFileSync('git', ['rev-parse', '--show-toplevel'], { encoding: 'utf8' }).trim();

/** Reads a file by repo-relative path, so files outside site/ (buildspec.json) need no Vite import. */
export function readRepoFile(path: string): string {
  return readFileSync(join(repoRoot, path), 'utf8');
}

/** Committer date (YYYY-MM-DD) of the last commit touching any of `paths` (repo-relative); today if none, e.g. uncommitted. */
export function gitDate(paths: string[]): string {
  const date = execFileSync('git', ['log', '-1', '--format=%cs', '--', ...paths], { cwd: repoRoot, encoding: 'utf8' }).trim();
  return date || new Date().toISOString().slice(0, 10);
}

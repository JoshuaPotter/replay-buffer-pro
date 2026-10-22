import type { APIRoute } from 'astro';
import { RELEASE_DATE, VERSION } from '../data/site';
import { readRepoFile } from '../lib/repo';

const SOURCE = 'site/src/content/llms.txt';

// llms.txt is hand-written (with CRLF line endings); only its footer values are filled in at build time.
export const GET: APIRoute = () => {
  const text = readRepoFile(SOURCE);
  if (!text.includes('{{RELEASE_DATE}}') || !text.includes('{{VERSION}}')) {
    throw new Error(`${SOURCE} must keep its {{RELEASE_DATE}} and {{VERSION}} footer placeholders`);
  }
  return new Response(text.replace('{{RELEASE_DATE}}', RELEASE_DATE).replace('{{VERSION}}', VERSION));
};

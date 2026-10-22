import type { APIRoute } from 'astro';
import { readRepoFile } from '../lib/repo';

// The changelog's Markdown source is served as written; changelog.astro renders the same file.
export const GET: APIRoute = () => new Response(readRepoFile('site/src/content/changelog.md'));

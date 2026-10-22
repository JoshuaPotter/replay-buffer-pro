import type { APIRoute, GetStaticPaths } from 'astro';
import { getCollection, type CollectionEntry } from 'astro:content';
import { pageMarkdown } from '../lib/page-markdown';

export const getStaticPaths = (async () =>
  (await getCollection('pages')).map((page) => ({ params: { slug: page.id }, props: { page } }))) satisfies GetStaticPaths;

export const GET: APIRoute<{ page: CollectionEntry<'pages'> }> = ({ props }) => new Response(pageMarkdown(props.page));

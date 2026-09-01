// Fetch high-res character images from the safe booru for the site backgrounds.
// Output: <project>/assets/saber-<n>.jpg  and  <project>/assets/shiki-<n>.jpg
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, '..');
const OUT = path.join(ROOT, 'assets');
fs.mkdirSync(OUT, { recursive: true });

const HEADERS = { 'User-Agent': 'Mozilla/5.0', Referer: 'https://safebooru.org/' };

async function fetchText(url) {
  const c = new AbortController();
  const t = setTimeout(() => c.abort(), 12000);
  try {
    const r = await fetch(url, { signal: c.signal, headers: HEADERS });
    return { status: r.status, text: await r.text() };
  } finally { clearTimeout(t); }
}

async function fetchBuffer(url) {
  const c = new AbortController();
  const t = setTimeout(() => c.abort(), 30000);
  try {
    const r = await fetch(url, { signal: c.signal, headers: HEADERS });
    if (!r.ok) throw new Error('HTTP ' + r.status + ' for ' + url);
    const buf = Buffer.from(await r.arrayBuffer());
    return { ok: true, buf, type: r.headers.get('content-type') };
  } finally { clearTimeout(t); }
}

// Query XML API and parse posts
async function query(tags, limit = 60) {
  const url = 'https://safebooru.org/index.php?page=dapi&s=post&q=index&tags=' +
    encodeURIComponent(tags) + '&limit=' + limit;
  const { status, text } = await fetchText(url);
  if (status !== 200) throw new Error('API status ' + status);
  const posts = [];
  const re = /<post ([^>]*)\/?>/g;
  let m;
  while ((m = re.exec(text)) !== null) {
    const attrStr = m[1];
    const attrs = {};
    const are = /(\w+)="([^"]*)"/g;
    let a;
    while ((a = are.exec(attrStr)) !== null) attrs[a[1]] = a[2];
    if (attrs.file_url) posts.push(attrs);
  }
  return posts;
}

// Pick a good, high-res landscape-ish image; fall back to portrait
function choose(posts, minW = 1200) {
  const scored = posts.filter(p => {
    const w = parseInt(p.sample_width || p.width || 0, 10);
    const h = parseInt(p.sample_height || p.height || 0, 10);
    const score = parseInt(p.score || 0, 10);
    return w >= minW && h >= 700 && score >= 0;
  });
  scored.sort((a, b) => (parseInt(b.score, 10) - parseInt(a.score, 10)));
  return scored.slice(0, 14);
}

async function download(tag, outPrefix, count, minW) {
  const posts = await query(tag, 80);
  console.log(`[${tag}] total ${posts.length} posts`);
  const picked = choose(posts, minW);
  console.log(`[${tag}] downloaded candidates:`);
  let i = 0;
  for (const p of picked.slice(0, count)) {
    i++;
    const w = p.sample_width || p.width || '?';
    const h = p.sample_height || p.height || '?';
    const score = p.score;
    try {
      const { ok, buf } = await fetchBuffer(p.file_url);
      if (!ok) continue;
      const ext = path.extname(p.file_url) || '.jpg';
      const fname = `${outPrefix}-${i}${ext.toLowerCase()}`;
      fs.writeFileSync(path.join(OUT, fname), buf);
      console.log(`   saved ${fname}  (${w}x${h}, score ${score}, ${buf.length} bytes)`);
    } catch (e) {
      console.log('   download FAIL ' + p.file_url + ' :: ' + e.message);
    }
  }
}

// Saber / Artoria Pendragon
await download('saber_(fate)', 'saber', 12, 900).catch(e => console.error('saber err', e.message));

console.log('DONE -> ' + OUT);

const HEADERS = { 'User-Agent': 'Mozilla/5.0', Referer: 'https://safebooru.org/' };
async function count(tag) {
  const url = 'https://safebooru.org/index.php?page=dapi&s=post&q=index&tags=' + encodeURIComponent(tag) + '&limit=1';
  const r = await fetch(url, { headers: HEADERS, signal: AbortSignal.timeout(9000) });
  const t = await r.text();
  const m = t.match(/<posts count="([0-9]+)"/);
  return m ? m[1] : '?';
}
const tests = ['saber', 'saber_(fate)', 'artoria', 'artoria_pendragon', 'saber_artoria', 'saber_(fate) artoria', 'artoria_(fate)', 'shiki_ryougi'];
for (const t of tests) {
  try { console.log(t.padEnd(28), 'count=', await count(t)); }
  catch (e) { console.log(t, 'FAIL', e.message); }
}

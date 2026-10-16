// Where is the game stuck? Pause the page a few times (Chrome DevTools protocol) and print the call stacks.
// Build with EXTRA_LDFLAGS=--profiling-funcs for function names. Env: AFTER (ms after Play, 6000)
const { chromium } = require('/pw/node_modules/playwright-core');
(async () => {
  const browser = await chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const page = await browser.newPage({ viewport: { width: 1024, height: 768 } });
  const cdp = await page.context().newCDPSession(page);
  await page.goto('http://localhost:8099/');
  await page.waitForTimeout(1500);
  await cdp.send('Debugger.enable');
  await page.evaluate(() => document.getElementById('go').click());
  await new Promise((r) => setTimeout(r, +(process.env.AFTER || 6000)));
  const seen = new Map();
  for (let n = 0; n < 8; n++) {
    const paused = new Promise((r) => cdp.once('Debugger.paused', r));
    cdp.send('Debugger.pause').catch(() => {});
    const ev = await Promise.race([paused, new Promise((r) => setTimeout(() => r(null), 5000))]);
    if (!ev) { console.log('no pause'); continue; }
    const k = ev.callFrames.map((f) => f.functionName || '?').slice(0, 14).join(' < ');
    seen.set(k, (seen.get(k) || 0) + 1);
    await cdp.send('Debugger.resume'); await new Promise((r) => setTimeout(r, 300));
  }
  for (const [k, v] of seen) console.log(v + 'x ' + k + '\n');
  process.exit(0);
})();

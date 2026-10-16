// Two or three players (PLAYERS=2/3): ACTIONS like page.cjs, each prefixed "A:", "B:", "C:" or "both:" (= everyone);
// all:wait:ms waits once. VIEWS=1280x720,1024x768 sets each player's window. Extra steps: hold:Key,ms  stack (print call stacks of that page)
const pw = require('/pw/node_modules/playwright-core');
(async () => {
  const browser = await pw.chromium.launch({ args: ['--autoplay-policy=no-user-gesture-required', '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] });
  const pages = {};
  const names = ['A', 'B', 'C'].slice(0, +(process.env.PLAYERS || 2));
  for (const n of names) {
    const [vw, vh] = ((process.env.VIEWS || '').split(',')[names.indexOf(n)] || '1024x768').split('x').map(Number);
    const ctx = await browser.newContext({ viewport: { width: vw, height: vh } });
    const page = await ctx.newPage();
    page.on('pageerror', (e) => console.log(n, 'pageerror', e.message));
    page.on('console', (m) => { if (process.env.VERBOSE || /error|DBG/i.test(m.text())) console.log(n, m.type(), m.text().slice(0, 200)); });
    await page.goto(process.env.URL);
    page.cdp = await ctx.newCDPSession(page); await page.cdp.send('Debugger.enable');
    pages[n] = page;
  }
  const pt = async (page, fx, fy) => { const r = await page.evaluate(() => { const b = canvas.getBoundingClientRect(); return b.width ? [b.left, b.top, b.width, b.height] : [0, 0, innerWidth, innerHeight]; }); return [r[0] + fx * r[2], r[1] + fy * r[3]]; };
  for (const step of process.env.ACTIONS.split(';')) {
    const c = step.indexOf(':'); const who = step.slice(0, c); const a = step.slice(c + 1);
    const i = a.indexOf(':'), k = i < 0 ? a : a.slice(0, i), v = i < 0 ? '' : a.slice(i + 1);
    const n = v.split(',').map(Number);
    const targets = who === 'both' ? names : [who];
    if (who === 'all' && k === 'wait') { await new Promise((r) => setTimeout(r, +v)); continue; }
    await Promise.all(targets.map(async (t) => {
      const page = pages[t];
      if (k === 'wait') await page.waitForTimeout(+v);
      else if (k === 'click') { const [x, y] = await pt(page, n[0], n[1]); await page.mouse.move(x, y, { steps: 5 }); await page.waitForTimeout(150); await page.mouse.click(x, y, { delay: 120 }); }
      else if (k === 'drag') { const [x1, y1] = await pt(page, n[0], n[1]); const [x2, y2] = await pt(page, n[2], n[3]); await page.mouse.move(x1, y1); await page.mouse.down(); await page.mouse.move(x2, y2, { steps: 20 }); await page.waitForTimeout(150); await page.mouse.up(); }
      else if (k === 'key') { await page.keyboard.down(v); await page.waitForTimeout(150); await page.keyboard.up(v); }
      else if (k === 'hold') { await page.keyboard.down(v.split(',')[0]); await page.waitForTimeout(+v.split(',')[1]); await page.keyboard.up(v.split(',')[0]); }
      else if (k === 'stack') { for (let j = 0; j < 4; j++) { const ev = await Promise.race([new Promise((r) => { page.cdp.once('Debugger.paused', r); page.cdp.send('Debugger.pause').catch(() => {}); }), new Promise((r) => setTimeout(() => r(null), 5000))]); if (!ev) { console.log(t, 'no pause'); continue; } console.log(t, 'STACK', ev.callFrames.map((f) => f.functionName || '?').slice(0, 16).join(' < ')); await page.cdp.send('Debugger.resume'); await new Promise((r) => setTimeout(r, 400)); } }
      else if (k === 'type') await page.keyboard.type(v, { delay: 80 });
      else if (k === 'shot') await page.screenshot({ path: `/out/${v}-${t}.png`, timeout: 5000 }).catch(() => console.log(t, 'HUNG at', v));
      else if (k === 'js') console.log(t, 'js', JSON.stringify(await page.evaluate(v)));
      else if (k === 'waitstart') await page.waitForFunction(() => window.__started, null, { timeout: 180000 });
    }));
  }
  await browser.close();
})();

// Drive the Rise of the Triad page. ACTIONS (";"-separated, x,y as fractions of the canvas box):
// wait:ms  click:x,y  rclick:x,y  tap:x,y  hold:x,y  pan:x1,y1,x2,y2 (two fingers)
// key:Name  type:text  shot:name  js:expr  al:label  waitstart:label  reload
// Env: URL, VIEW (WxH), TOUCH=1, BROWSER (chromium|firefox), NOAUTOPLAY=1
const pw = require('/pw/node_modules/playwright-core');
(async () => {
  const [vw, vh] = (process.env.VIEW || '1280x720').split('x').map(Number);
  const type = process.env.BROWSER || 'chromium';
  const args = type === 'chromium' ? [...(process.env.NOAUTOPLAY ? [] : ['--autoplay-policy=no-user-gesture-required']), '--use-angle=swiftshader', '--enable-unsafe-swiftshader'] : [];
  const browser = await pw[type].launch({ args });
  const page = await browser.newPage({ viewport: { width: vw, height: vh }, ...(process.env.TOUCH ? { hasTouch: true, isMobile: true } : {}) });
  const logs = [];
  page.on('console', (m) => logs.push(m.type() + ': ' + m.text()));
  page.on('pageerror', (e) => logs.push('pageerror: ' + e.message + ' ' + (e.stack || '').split('\n').slice(0, 12).join(' < ')));
  await page.goto(process.env.URL || 'http://localhost:8099/');
  const box = () => page.evaluate(() => { const r = document.getElementById('canvas').getBoundingClientRect(); return r.width ? [r.left, r.top, r.width, r.height] : [0, 0, innerWidth, innerHeight]; });
  const pt = async (fx, fy) => { const [l, t, w, h] = await box(); return [l + fx * w, t + fy * h]; };
  const cdp = type === 'chromium' ? await page.context().newCDPSession(page) : null;
  for (const a of process.env.ACTIONS.split(';')) {
    const i = a.indexOf(':'), k = i < 0 ? a : a.slice(0, i), v = i < 0 ? '' : a.slice(i + 1);
    const n = v.split(',').map(Number);
    if (k === 'wait') await page.waitForTimeout(+v);
    else if (k === 'click' || k === 'rclick') { const [x, y] = await pt(n[0], n[1]); await page.mouse.move(x, y, { steps: 5 }); await page.waitForTimeout(150); await page.mouse.click(x, y, { button: k === 'rclick' ? 'right' : 'left', delay: 120 }); }
    else if (k === 'tap') { const [x, y] = await pt(n[0], n[1]); await page.touchscreen.tap(x, y); }
    else if (k === 'hold') { const [x, y] = await pt(n[0], n[1]); await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: [{ x, y }] }); await page.waitForTimeout(800); await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] }); }
    else if (k === 'pan') {
      const [x1, y1] = await pt(n[0], n[1]), [x2, y2] = await pt(n[2], n[3]);
      const pts = (x, y) => [{ x: x - 40, y, id: 1 }, { x: x + 40, y, id: 2 }];
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchStart', touchPoints: pts(x1, y1) });
      for (let j = 1; j <= 15; j++) { await cdp.send('Input.dispatchTouchEvent', { type: 'touchMove', touchPoints: pts(x1 + (x2 - x1) * j / 15, y1 + (y2 - y1) * j / 15) }); await page.waitForTimeout(30); }
      await cdp.send('Input.dispatchTouchEvent', { type: 'touchEnd', touchPoints: [] });
    }
    else if (k === 'move') { const [x, y] = await pt(n[0], n[1]); await page.mouse.move(x, y, { steps: 5 }); }
    else if (k === 'keyhold') { const [name, ms] = v.split(','); await page.keyboard.down(name); await page.waitForTimeout(+ms); await page.keyboard.up(name); }
    else if (k === 'upload') { const [sel, ...files] = v.split(','); await page.setInputFiles(sel, files.map((f) => '/web/' + f)); }
    else if (k === 'key') { await page.keyboard.down(v); await page.waitForTimeout(150); await page.keyboard.up(v); }
    else if (k === 'type') await page.keyboard.type(v, { delay: 80 });
    else if (k === 'reload') await page.reload();
    else if (k === 'shot') await page.screenshot({ path: `/out/${v}.png` });
    else if (k === 'js') console.log('js', JSON.stringify(await page.evaluate(v)));
    else if (k === 'waitstart') { const t0 = Date.now(); await page.waitForFunction(() => window.__started, null, { timeout: 180000 }); console.log('started after', ((Date.now() - t0) / 1000).toFixed(1), 's', v); }
    else if (k === 'al') console.log('al', v, await page.evaluate(() => { const c = typeof AL !== 'undefined' && AL.currentCtx; if (!c) return 'no AL context'; const src = Object.values(c.sources || {}); return JSON.stringify({ audio: c.audioCtx && c.audioCtx.state, sources: src.length, playing: src.filter((s) => s.state === 0x1012).length }); }));
  }
  console.log(logs.filter((l) => /error|warn/i.test(l)).slice(-10).join('\n'));
  await browser.close();
})();

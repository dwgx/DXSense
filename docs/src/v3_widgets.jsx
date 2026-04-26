// ── v3 Premium Widget Vocabulary ────────────────────────────────────────

// ── Glass Surface ──────────────────────────────────────────────────────
function Glass({ children, style={}, hover=true, glow=false }) {
  const t = useT();
  const [h, setH] = useState(false);
  return React.createElement('div', {
    onMouseEnter:hover?()=>setH(true):undefined,
    onMouseLeave:hover?()=>setH(false):undefined,
    style:{
      background: h ? t.glassHover : t.glass,
      backdropFilter: t.blur, WebkitBackdropFilter: t.blur,
      border:`1px solid ${h ? t.accentEdge : t.glassBorder}`,
      borderRadius:t.r, position:'relative', overflow:'hidden',
      transition:'all 0.2s ease',
      boxShadow: glow ? `0 0 20px rgba(255,255,255,0.03), inset 0 1px 0 rgba(255,255,255,0.06)` :
                        `inset 0 1px 0 rgba(255,255,255,0.04)`,
      ...style,
    }
  }, children);
}

// ── Sparkline ──────────────────────────────────────────────────────────
function Sparkline({ data, width=80, height=28, color, animate=true }) {
  const t = useT();
  const c = color || t.accent;
  const canvasRef = useRef(null);
  const frameRef = useRef(0);
  useEffect(() => {
    const cv = canvasRef.current; if(!cv) return;
    const ctx = cv.getContext('2d');
    const dpr = 2; cv.width=width*dpr; cv.height=height*dpr;
    ctx.scale(dpr,dpr);
    let raf;
    const draw = () => {
      frameRef.current++;
      ctx.clearRect(0,0,width,height);
      const pts = data.length;
      const min = Math.min(...data), max = Math.max(...data);
      const range = max-min||1;
      const step = width/(pts-1);
      // Fill gradient
      ctx.beginPath();
      ctx.moveTo(0, height);
      data.forEach((v,i) => {
        const x=i*step, y=height-(((v-min)/range)*height*0.85+height*0.05);
        i===0?ctx.lineTo(x,y):ctx.lineTo(x,y);
      });
      ctx.lineTo(width,height);
      ctx.closePath();
      const grd = ctx.createLinearGradient(0,0,0,height);
      grd.addColorStop(0, c+'30');
      grd.addColorStop(1, 'transparent');
      ctx.fillStyle=grd; ctx.fill();
      // Line
      ctx.beginPath();
      data.forEach((v,i) => {
        const x=i*step, y=height-(((v-min)/range)*height*0.85+height*0.05);
        i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);
      });
      ctx.strokeStyle=c; ctx.lineWidth=1.5; ctx.lineJoin='round'; ctx.stroke();
      // End dot
      const lastX=width, lastY=height-(((data[pts-1]-min)/range)*height*0.85+height*0.05);
      ctx.fillStyle=c; ctx.beginPath(); ctx.arc(lastX,lastY,2.5,0,Math.PI*2); ctx.fill();
      // Glow
      ctx.fillStyle=c+'40'; ctx.beginPath(); ctx.arc(lastX,lastY,5,0,Math.PI*2); ctx.fill();
    };
    draw();
    if(animate){ const iv=setInterval(draw,1000); return ()=>clearInterval(iv); }
  },[data,width,height,color]);
  return React.createElement('canvas', {ref:canvasRef, style:{width,height}});
}

// ── Toggle ─────────────────────────────────────────────────────────────
function Tog({ value, onChange, label }) {
  const t = useT();
  return React.createElement('div', {style:{display:'flex',alignItems:'center',gap:8,cursor:'pointer',userSelect:'none'},onClick:()=>onChange(!value)},
    React.createElement('div', {style:{
      width:38,height:20,borderRadius:10,position:'relative',
      background:value?t.accentHot:t.surfaceHighest,
      boxShadow:value?'0 0 8px rgba(255,255,255,0.15)':'none',
      transition:'all 0.2s',
    }},
      React.createElement('div', {style:{
        width:16,height:16,borderRadius:8,position:'absolute',top:2,
        left:value?20:2,background:value?t.surfaceDim:t.textMuted,
        transition:'left 0.15s ease, background 0.15s',
        boxShadow:'0 1px 3px rgba(0,0,0,0.3)',
      }})
    ),
    label && React.createElement('span', {style:{fontSize:t.body,color:t.textSec}}, label)
  );
}

// ── Section ────────────────────────────────────────────────────────────
function Section({ label, detail }) {
  const t = useT();
  return React.createElement('div', {style:{display:'flex',alignItems:'center',gap:10,marginBottom:6}},
    React.createElement('span', {style:{fontSize:t.cap,fontWeight:600,letterSpacing:1,color:t.textMuted,textTransform:'uppercase'}}, label),
    detail && React.createElement('span', {style:{fontSize:t.cap,color:t.textDim}}, detail),
    React.createElement('div', {style:{flex:1,height:1,background:`linear-gradient(90deg,${t.glassBorder},transparent)`,marginLeft:8}}),
  );
}

// ── Chip ───────────────────────────────────────────────────────────────
function Chip({ label, active, onClick }) {
  const t = useT();
  return React.createElement('button', {onClick, style:{
    background:active?'rgba(255,255,255,0.12)':'transparent',
    color:active?t.text:t.textMuted,
    border:`1px solid ${active?t.accentEdge:t.glassBorder}`,
    borderRadius:t.rSm,padding:'3px 11px',fontSize:t.cap,
    cursor:'pointer',fontFamily:'inherit',transition:'all 0.15s',
    backdropFilter:active?t.blur:'none',
  }}, label);
}

// ── Buttons ────────────────────────────────────────────────────────────
function Btn({ label, onClick, variant='ghost', style={} }) {
  const t = useT();
  const [h, setH] = useState(false);
  const styles = {
    ghost: {
      background:h?'rgba(255,255,255,0.08)':'transparent',
      color:t.textSec, border:`1px solid ${t.glassBorder}`,
    },
    primary: {
      background:h?t.accentHot:t.accent, color:t.bg,
      border:'none', fontWeight:600,
      boxShadow:h?'0 0 16px rgba(255,255,255,0.15)':'none',
    },
    danger: {
      background:h?'rgba(248,113,113,0.15)':'transparent',
      color:t.bad, border:`1px solid rgba(248,113,113,0.3)`,
    },
  };
  return React.createElement('button', {
    onClick, onMouseEnter:()=>setH(true), onMouseLeave:()=>setH(false),
    style:{
      ...styles[variant], borderRadius:t.rSm, padding:'6px 16px',
      fontSize:t.body, cursor:'pointer', fontFamily:'inherit',
      transition:'all 0.2s', ...style,
    }
  }, label);
}

// ── Status Dot ─────────────────────────────────────────────────────────
function Dot({ s='idle', size=7 }) {
  const t = useT();
  const c = {good:t.good,warn:t.warn,bad:t.bad,info:t.info,accent:t.accent,idle:t.textDim}[s]||t.textDim;
  return React.createElement('span', {style:{
    display:'inline-block',width:size,height:size,borderRadius:size,
    background:c, boxShadow:`0 0 6px ${c}60`,
  }});
}

// ── Status Badge ───────────────────────────────────────────────────────
function Badge({ s='idle', label }) {
  const t = useT();
  const c = {good:t.good,warn:t.warn,bad:t.bad,info:t.info,accent:t.accent,idle:t.textMuted}[s]||t.textMuted;
  return React.createElement('span', {style:{
    fontSize:t.cap-1, fontWeight:500, padding:'2px 8px', borderRadius:t.rXs,
    background:`${c}18`, color:c, letterSpacing:0.3,
  }}, label);
}

// ── Command Palette — Full-screen search experience ────────────────────
function CmdPalette({ open, onClose, onSelect, panels }) {
  const t = useT();
  const [query, setQuery] = useState('');
  const [focused, setFocused] = useState(0);
  const [entering, setEntering] = useState(false);
  const inputRef = useRef(null);

  useEffect(() => {
    if(open) { setQuery(''); setFocused(0); setEntering(true);
      setTimeout(()=>inputRef.current?.focus(), 100);
      setTimeout(()=>setEntering(false), 400);
    }
  }, [open]);

  useEffect(() => {
    const handler = e => {
      if((e.metaKey||e.ctrlKey) && e.key==='k') { e.preventDefault(); onClose('toggle'); }
      if(e.key==='Escape' && open) onClose();
    };
    window.addEventListener('keydown', handler);
    return () => window.removeEventListener('keydown', handler);
  }, [open, onClose]);

  if(!open) return null;

  const filtered = panels.filter(p => !query ||
    p.title.toLowerCase().includes(query.toLowerCase()) ||
    p.cat.toLowerCase().includes(query.toLowerCase()));

  // Group filtered results by category
  const groups = (() => {
    const m = new Map();
    filtered.forEach((p,i) => {
      const c = p.cat||'General';
      if(!m.has(c)) m.set(c,[]);
      m.get(c).push({...p, flatIdx:i});
    });
    return m;
  })();

  const handleKey = e => {
    if(e.key==='ArrowDown') { e.preventDefault(); setFocused(f=>Math.min(f+1,filtered.length-1)); }
    if(e.key==='ArrowUp') { e.preventDefault(); setFocused(f=>Math.max(f-1,0)); }
    if(e.key==='Enter' && filtered.length) { onSelect(filtered[focused]?.id); onClose(); }
  };

  return React.createElement('div', {
    onClick:e=>{if(e.target===e.currentTarget)onClose()},
    style:{
      position:'fixed',inset:0,zIndex:2000,
      display:'flex',flexDirection:'column',alignItems:'center',justifyContent:'center',
      background:'rgba(8,8,8,0.75)',
      backdropFilter:'blur(24px)',WebkitBackdropFilter:'blur(24px)',
      animation:'fadeIn 0.2s ease',
    }
  },
    // Main search container
    React.createElement('div', {style:{
      width:'100%',maxWidth:640,padding:'0 24px',
      animation:'fadeSlideUp 0.3s cubic-bezier(0.16,1,0.3,1)',
    }},
      // Search header with large input
      React.createElement('div', {style:{marginBottom:40,textAlign:'center'}},
        React.createElement('div', {style:{
          display:'inline-flex',alignItems:'center',justifyContent:'center',
          width:56,height:56,borderRadius:16,marginBottom:20,
          background:'rgba(255,255,255,0.08)',border:'1px solid rgba(255,255,255,0.12)',
        }},
          React.createElement('i', {className:'mdi mdi-magnify',style:{fontSize:26,color:'#ccc'}}),
        ),
        React.createElement('div', {style:{fontSize:20,fontWeight:300,color:'#a0a0a0',letterSpacing:1}}, 'Command Palette'),
      ),
      // Input field
      React.createElement('div', {style:{
        position:'relative',marginBottom:32,
      }},
        React.createElement('input', {
          ref:inputRef, value:query,
          onChange:e=>{setQuery(e.target.value);setFocused(0)},
          onKeyDown:handleKey,
          placeholder:'Where do you want to go?',
          style:{
            width:'100%',background:'rgba(255,255,255,0.04)',
            border:'1px solid rgba(255,255,255,0.12)',
            borderRadius:t.r, padding:'16px 20px 16px 48px',
            color:'#f0f0f0',fontSize:18,fontFamily:'inherit',fontWeight:300,
            outline:'none',transition:'border-color 0.2s, box-shadow 0.2s',
            boxShadow:'0 4px 24px rgba(0,0,0,0.3)',
            letterSpacing:0.3,
          },
          onFocus:e=>{e.target.style.borderColor='rgba(255,255,255,0.15)';e.target.style.boxShadow='0 4px 24px rgba(0,0,0,0.3), 0 0 0 1px rgba(255,255,255,0.06)'},
          onBlur:e=>{e.target.style.borderColor='rgba(255,255,255,0.08)';e.target.style.boxShadow='0 4px 24px rgba(0,0,0,0.3)'},
        }),
        React.createElement('i', {className:'mdi mdi-magnify',style:{
          position:'absolute',left:18,top:'50%',transform:'translateY(-50%)',
          fontSize:18,color:t.textDim,pointerEvents:'none',
        }}),
        // Animated underline
        React.createElement('div', {style:{
          position:'absolute',bottom:-1,left:'10%',right:'10%',height:2,
          borderRadius:1,
          background: query ? `linear-gradient(90deg, transparent, rgba(255,255,255,0.3), transparent)` : 'transparent',
          transition:'all 0.3s ease',
        }}),
      ),

      // Results
      React.createElement('div', {style:{maxHeight:'50vh',overflowY:'auto'}},
        filtered.length===0 && query && React.createElement('div', {style:{
          textAlign:'center',padding:32,color:t.textDim,fontSize:t.ui,
        }},
          React.createElement('i', {className:'mdi mdi-magnify-close',style:{fontSize:32,display:'block',marginBottom:12,color:t.textDim}}),
          'No matching panels or actions',
        ),
        !query && React.createElement('div', {style:{
          display:'grid',gridTemplateColumns:'repeat(auto-fill,minmax(140px,1fr))',gap:10,
        }},
          // Quick access grid when no query
          panels.slice(0,8).map((p,i) =>
            React.createElement('div', {key:p.id, onClick:()=>{onSelect(p.id);onClose()},
              style:{
                background:'rgba(255,255,255,0.03)',border:'1px solid rgba(255,255,255,0.05)',
                borderRadius:t.rSm,padding:'14px 12px',cursor:'pointer',
                transition:'all 0.15s',textAlign:'center',
                animation:`fadeSlideUp 0.3s ease ${i*0.04}s both`,
              },
              onMouseEnter:e=>{e.currentTarget.style.background='rgba(255,255,255,0.07)';e.currentTarget.style.borderColor='rgba(255,255,255,0.12)'},
              onMouseLeave:e=>{e.currentTarget.style.background='rgba(255,255,255,0.03)';e.currentTarget.style.borderColor='rgba(255,255,255,0.05)'},
            },
              React.createElement('i', {className:`mdi ${p.icon}`,style:{fontSize:22,color:t.textSec,display:'block',marginBottom:8}}),
              React.createElement('div', {style:{fontSize:12,color:t.textSec,fontWeight:500}}, p.title),
            )
          )
        ),
        query && Array.from(groups.entries()).map(([cat, items]) =>
          React.createElement('div', {key:cat, style:{marginBottom:16}},
            React.createElement('div', {style:{fontSize:t.cap,color:t.textDim,letterSpacing:0.5,marginBottom:6,paddingLeft:4}}, cat),
            items.map(p =>
              React.createElement('div', {key:p.id, onClick:()=>{onSelect(p.id);onClose()},
                style:{
                  display:'flex',alignItems:'center',gap:14,padding:'11px 14px',
                  cursor:'pointer',borderRadius:t.rSm,
                  background:focused===p.flatIdx?'rgba(255,255,255,0.06)':'transparent',
                  transition:'all 0.1s',
                  animation:`fadeSlideUp 0.2s ease ${p.flatIdx*0.03}s both`,
                },
                onMouseEnter:()=>setFocused(p.flatIdx),
              },
                React.createElement('i', {className:`mdi ${p.icon}`,style:{fontSize:18,width:24,textAlign:'center',color:focused===p.flatIdx?t.text:t.textMuted}}),
                React.createElement('div', {style:{flex:1}},
                  React.createElement('div', {style:{fontSize:t.ui,color:focused===p.flatIdx?t.text:t.textSec,fontWeight:focused===p.flatIdx?500:400}}, p.title),
                ),
                focused===p.flatIdx && React.createElement('div', {style:{
                  fontSize:t.cap,color:t.textDim,display:'flex',alignItems:'center',gap:4,
                }},
                  React.createElement('span', {style:{background:'rgba(255,255,255,0.08)',padding:'2px 8px',borderRadius:t.rXs,fontSize:10}}, '↵'),
                ),
              )
            )
          )
        ),
      ),

      // Footer
      React.createElement('div', {style:{
        display:'flex',justifyContent:'center',gap:24,
        marginTop:24,fontSize:t.cap,color:t.textDim,
      }},
        React.createElement('span', {style:{display:'flex',alignItems:'center',gap:4}},
          React.createElement('span', {style:{background:'rgba(255,255,255,0.06)',padding:'1px 6px',borderRadius:3,fontSize:10}}, '↑↓'),
          ' navigate'),
        React.createElement('span', {style:{display:'flex',alignItems:'center',gap:4}},
          React.createElement('span', {style:{background:'rgba(255,255,255,0.06)',padding:'1px 6px',borderRadius:3,fontSize:10}}, '↵'),
          ' open'),
        React.createElement('span', {style:{display:'flex',alignItems:'center',gap:4}},
          React.createElement('span', {style:{background:'rgba(255,255,255,0.06)',padding:'1px 6px',borderRadius:3,fontSize:10}}, 'esc'),
          ' close'),
      ),
    ),
  );
}

// ── Splash Screen — Authentic AMI BIOS POST + cinematic ───────────────

function SplashScreen({ onDone, mode='intro' }) {
  const t = useT();
  const isExit = mode === 'exit';
  const [phase, setPhase] = useState(0);
  const [postLines, setPostLines] = useState([]);
  const [memCount, setMemCount] = useState(0);
  const [cursor, setCursor] = useState(true);

  useEffect(() => {
    const iv = setInterval(() => setCursor(c => !c), 530);
    return () => clearInterval(iv);
  }, []);

  // AMI star logo ASCII
  const amiStar = [
    '          \u2605',
    '        \u2605 \u2605 \u2605',
    '      \u2605   \u2605   \u2605',
    '    \u2605\u2605\u2605\u2605\u2605\u2605\u2605\u2605\u2605\u2605\u2605',
    '      \u2605   \u2605   \u2605',
    '        \u2605 \u2605 \u2605',
    '          \u2605',
  ];

  const postBoot = [
    {t:'AMIBIOS (C) 2026 American Megatrends, Inc.', c:'#e8e8e8', d:200, b:true},
    {t:'DXSense Overlay BIOS v0.1.0 for NeoX3 Engine', c:'#aaa', d:400},
    {t:'Copyright (C) 2026 dwrg / DXSense Project', c:'#aaa', d:500},
    {t:'', d:600},
    {t:'Intel(R) Core(TM) i7-12700K @ 3.60GHz', c:'#e8e8e8', d:700},
    {t:'__MEM__', c:'#e8e8e8', d:850}, // special: memory count
    {t:'', d:1600},
    {t:'Initializing USB Controllers .. Done.', c:'#aaa', d:1700},
    {t:'', d:1800},
    {t:'Detecting DX11 Render Target...', c:'#aaa', d:1900},
    {t:'  SwapChain HWND .......... 0x000A0B2C', c:'#888', d:2100},
    {t:'  Present vtable .......... dxgi.dll+0x5A20  [HOOKED]', c:'#4ADE80', d:2300},
    {t:'  ResizeBuffers vtable .... dxgi.dll+0x5B80  [HOOKED]', c:'#4ADE80', d:2450},
    {t:'', d:2550},
    {t:'MinHook Engine v1.3.4 ........ 2 targets armed', c:'#aaa', d:2650},
    {t:'WndProc Subclass ............. GWLP_WNDPROC installed', c:'#aaa', d:2800},
    {t:'', d:2900},
    {t:'Scanning Python Runtime...', c:'#aaa', d:3000},
    {t:'  neox_engine.dll ......... CPython 3.10.8 detected', c:'#4ADE80', d:3200},
    {t:'  GIL acquired ............ sys.path: 7 entries', c:'#888', d:3350},
    {t:'', d:3450},
    {t:'HUD Manager Init:', c:'#aaa', d:3550},
    {t:'  [\u2713] Stats    [\u2713] Crosshair    [\u2713] Radar    [\u2713] ESP', c:'#4ADE80', d:3700},
    {t:'', d:3800},
    {t:'Config: DXSense.json loaded ................ 14 keys OK', c:'#aaa', d:3900},
    {t:'Remote Bridge: port 9099 ................... LISTENING', c:'#aaa', d:4050},
    {t:'', d:4150},
    {t:'All POST checks passed.', c:'#4ADE80', d:4250, b:true},
    {t:'', d:4350},
  ];

  // Memory count animation
  useEffect(() => {
    if(isExit || phase !== 0) return;
    const target = 32768; // 32GB in MB
    const steps = 20;
    const T = [];
    for(let i=1;i<=steps;i++){
      T.push(setTimeout(()=>{
        setMemCount(Math.floor(target * (i/steps)));
      }, 850 + i*35));
    }
    return () => T.forEach(clearTimeout);
  },[isExit, phase]);

  // BOOT timeline
  useEffect(() => {
    if(isExit) return;
    const T = [];
    postBoot.forEach(line => {
      T.push(setTimeout(() => setPostLines(p => [...p, line]), line.d));
    });
    // Phase 1: fade POST
    T.push(setTimeout(() => setPhase(1), 4800));
    // Phase 2: cinematic
    T.push(setTimeout(() => setPhase(2), 5200));
    // Phase 3: settle
    T.push(setTimeout(() => setPhase(3), 5900));
    // Phase 4: fade out
    T.push(setTimeout(() => setPhase(4), 6500));
    T.push(setTimeout(() => { setPhase(5); onDone(); }, 7100));

    const skip = () => { setPhase(5); onDone(); };
    T.push(setTimeout(() => {
      window.addEventListener('click', skip, {once:true});
      window.addEventListener('keydown', skip, {once:true});
    }, 500));
    return () => { T.forEach(clearTimeout);
      window.removeEventListener('click', skip);
      window.removeEventListener('keydown', skip); };
  }, [isExit, onDone]);

  // EXIT timeline
  const shutLines = [
    {t:'DXSense — initiating safe eject...', c:'#aaa', d:0},
    {t:'', d:150},
    {t:'Detaching HUD widgets .................. OK', c:'#888', d:300},
    {t:'Releasing Python interpreter ........... OK', c:'#888', d:550},
    {t:'Removing MinHook targets ............... OK', c:'#888', d:800},
    {t:'Restoring WndProc ...................... OK', c:'#888', d:1050},
    {t:'Releasing SwapChain hooks .............. OK', c:'#888', d:1300},
    {t:'Saving config (14 keys) ................ OK', c:'#888', d:1500},
    {t:'', d:1650},
    {t:'All hooks removed. Memory freed.', c:'#4ADE80', d:1800, b:true},
    {t:'', d:1950},
    {t:'// thx for using DXSense. see you next inject.', c:'#444', d:2100},
  ];

  useEffect(() => {
    if(!isExit) return;
    const T = [];
    T.push(setTimeout(() => setPhase(1), 200));
    T.push(setTimeout(() => setPhase(2), 2200));
    T.push(setTimeout(() => setPhase(3), 2800));
    shutLines.forEach(line => {
      T.push(setTimeout(() => setPostLines(p => [...p, line]), 2800 + line.d));
    });
    T.push(setTimeout(() => setPhase(4), 5200));
    T.push(setTimeout(() => { setPhase(5); onDone(); }, 5800));
    return () => T.forEach(clearTimeout);
  }, [isExit, onDone]);

  if(phase >= 5) return null;

  // ── BOOT RENDER ─────────────────────────────────────────────────────
  if(!isExit) {
    const showPost = phase === 0;
    const showTitle = phase >= 2;
    const fading = phase >= 4;

    return React.createElement('div', {style:{
      position:'fixed',inset:0,zIndex:9999,background:'#000008',
      cursor:'pointer',overflow:'hidden',
      opacity:fading?0:1, transition:'opacity 0.6s ease',
    }},

      // POST phase
      showPost && React.createElement('div', {style:{
        position:'absolute',inset:0,padding:'20px 40px',
        opacity:phase>=1?0:1, transition:'opacity 0.35s ease',
        display:'flex',flexDirection:'column',
      }},
        // AMI Star header
        React.createElement('div', {style:{
          display:'flex',gap:24,alignItems:'flex-start',marginBottom:8,
          borderBottom:'1px solid #1a1a2a',paddingBottom:8,
        }},
          // Star ASCII art
          React.createElement('pre', {style:{
            fontFamily:'monospace',fontSize:9,lineHeight:1.1,color:'#4488cc',
            margin:0,letterSpacing:2,whiteSpace:'pre',
            textShadow:'0 0 8px rgba(68,136,204,0.3)',
          }}, amiStar.join('\n')),
          React.createElement('div', {style:{paddingTop:4}},
            React.createElement('div', {style:{fontFamily:'monospace',fontSize:13,color:'#4488cc',fontWeight:700,letterSpacing:1}},
              'AMIBIOS SETUP UTILITY'),
            React.createElement('div', {style:{fontFamily:'monospace',fontSize:10,color:'#336699',marginTop:2}},
              'Version 02.61  \u00b7  DXSense Overlay Framework'),
          ),
        ),

        // POST log
        React.createElement('div', {style:{
          fontFamily:'monospace',fontSize:12,lineHeight:1.6,flex:1,overflow:'hidden',
        }},
          postLines.map((l,i) => {
            if(!l.t) return React.createElement('div',{key:i,style:{height:5}});
            if(l.t === '__MEM__') {
              return React.createElement('div',{key:i,style:{color:'#e8e8e8'}},
                'Memory Test : ' + memCount.toLocaleString() + ' MB OK');
            }
            return React.createElement('div',{key:i,style:{
              color:l.c||'#aaa',fontWeight:l.b?700:400,
            }},l.t);
          }),
          React.createElement('span',{style:{color:'#4488cc',fontFamily:'monospace',fontSize:12,opacity:cursor?1:0}},'\u2588'),
        ),

        // Bottom bar
        React.createElement('div', {style:{
          borderTop:'1px solid #1a1a2a',paddingTop:6,marginTop:6,
          fontFamily:'monospace',fontSize:10,color:'#336699',
          display:'flex',justifyContent:'space-between',
        }},
          React.createElement('span',null,'DEL: Enter Setup  \u2502  INSERT: Toggle Overlay  \u2502  F12: Boot Menu'),
          React.createElement('span',{style:{color:'#333'}},'Tab: POST / Logo'),
        ),
      ),

      // Cinematic title phase
      showTitle && React.createElement('div', {style:{
        position:'absolute',inset:0,
        display:'flex',flexDirection:'column',alignItems:'center',justifyContent:'center',
        animation:'fadeIn 0.5s ease both',
      }},
        React.createElement('div',{style:{
          position:'absolute',width:500,height:500,borderRadius:'50%',
          background:'radial-gradient(circle, rgba(255,255,255,0.02) 0%, transparent 60%)',
          pointerEvents:'none',
        }}),
        React.createElement('div', {style:{
          width:32,height:32,borderRadius:8,marginBottom:24,
          background:'linear-gradient(145deg, #c0c0c0, #404040)',
          display:'flex',alignItems:'center',justifyContent:'center',
          boxShadow:'0 0 40px rgba(255,255,255,0.06)',
          animation:'fadeSlideUp 0.6s cubic-bezier(0.16,1,0.3,1) both',
        }},
          React.createElement('div',{style:{width:16,height:16,borderRadius:4,background:'rgba(0,0,0,0.8)'}}),
        ),
        React.createElement('div', {style:{display:'flex',gap:5}},
          'DXSENSE'.split('').map((ch,i) =>
            React.createElement('span',{key:i,style:{
              fontSize:60,fontWeight:700,letterSpacing:6,color:'#f0f0f0',
              animation:'letterReveal 0.45s cubic-bezier(0.16,1,0.3,1) '+(0.05*i)+'s both',
              textShadow:'0 0 60px rgba(255,255,255,0.12)',
            }}, ch)
          )
        ),
        React.createElement('div', {style:{
          fontSize:11,letterSpacing:6,color:'rgba(255,255,255,0.25)',marginTop:18,
          animation:'fadeIn 0.5s ease 0.4s both',
        }}, 'O V E R L A Y   R E A D Y'),
      ),
    );
  }

  // ── EXIT RENDER ─────────────────────────────────────────────────────
  const showBye = phase >= 1 && phase < 3;
  const showShut = phase >= 3;
  const fading = phase >= 4;

  return React.createElement('div', {style:{
    position:'fixed',inset:0,zIndex:9999,background:'#000',
    overflow:'hidden',
    opacity:fading?0:1, transition:'opacity 0.6s ease',
  }},
    // BYE cinematic
    showBye && React.createElement('div', {style:{
      position:'absolute',inset:0,
      display:'flex',flexDirection:'column',alignItems:'center',justifyContent:'center',
      opacity:phase>=2?0:1, transition:'opacity 0.5s ease',
    }},
      React.createElement('div', {style:{
        fontSize:88,fontWeight:200,letterSpacing:20,color:'#f0f0f0',
        animation:'fadeSlideUp 0.8s cubic-bezier(0.16,1,0.3,1) both',
        textShadow:'0 0 50px rgba(255,255,255,0.1)',
      }}, 'BYE'),
      React.createElement('div', {style:{
        fontSize:13,letterSpacing:6,color:'rgba(255,255,255,0.25)',marginTop:20,
        animation:'fadeSlideUp 0.6s cubic-bezier(0.16,1,0.3,1) 0.2s both',
      }}, 'T H A N K S   F O R   U S I N G'),
      React.createElement('div', {style:{
        width:40,height:1,background:'rgba(255,255,255,0.08)',marginTop:28,borderRadius:1,
        animation:'fadeIn 0.8s ease 0.6s both',
      }}),
      React.createElement('div', {style:{
        fontSize:11,color:'rgba(255,255,255,0.1)',marginTop:16,letterSpacing:2,
        animation:'fadeIn 0.8s ease 0.8s both',
      }}, 'DXSense v0.1  \u00b7  dwrg  \u00b7  NeoX3'),
    ),

    // Shutdown log
    showShut && React.createElement('div', {style:{
      position:'absolute',inset:0,padding:'32px 48px',
      display:'flex',flexDirection:'column',justifyContent:'flex-end',
      animation:'fadeIn 0.4s ease both',
    }},
      React.createElement('div', {style:{fontFamily:'monospace',fontSize:12,lineHeight:1.8}},
        postLines.map((l,i) => {
          if(!l.t) return React.createElement('div',{key:i,style:{height:4}});
          return React.createElement('div',{key:i,style:{
            color:l.c||'#555',fontWeight:l.b?600:400,
            animation:'fadeIn 0.15s ease both',
          }},l.t);
        }),
        !fading && React.createElement('span',{style:{color:'#336699',fontFamily:'monospace',fontSize:12,opacity:cursor?1:0}},'\u2588'),
      ),
    ),
  );
}

// ── Toast ──────────────────────────────────────────────────────────────
function Toasts({ toasts }) {
  const t = useT();
  return React.createElement('div', {style:{position:'absolute',bottom:16,right:16,display:'flex',flexDirection:'column-reverse',gap:8,zIndex:100}},
    toasts.map(toast =>
      React.createElement('div', {key:toast.id, style:{
        background:'rgba(20,20,20,0.9)',backdropFilter:'blur(12px)',
        border:`1px solid rgba(255,255,255,0.08)`,
        borderRadius:t.rSm,padding:'8px 16px',fontSize:t.body,
        color:t.text,boxShadow:'0 4px 20px rgba(0,0,0,0.5)',
        animation:'fadeSlideUp 0.25s ease',
        opacity:toast.fading?0:1,transition:'opacity 0.3s',
      }}, toast.text)
    )
  );
}

Object.assign(window, {
  Glass, Sparkline, Tog, Section, Chip, Btn, Dot, Badge,
  CmdPalette, SplashScreen, Toasts,
});

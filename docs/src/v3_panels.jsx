// ── v3 Panels — glassmorphism, sparklines, premium data display ────────

// ── Overview: Hero Dashboard ───────────────────────────────────────────
function V3Overview() {
  const t = useT();
  const [fps, setFps] = useState(61.2);
  const [fpsHistory, setFpsHistory] = useState(()=>Array.from({length:20},()=>55+Math.random()*10));
  const [frameHistory] = useState(()=>Array.from({length:20},(_,i)=>14+Math.sin(i*0.5)*3+Math.random()*2));
  const [entityHistory] = useState(()=>Array.from({length:20},()=>Math.floor(8+Math.random()*8)));

  useEffect(() => {
    const iv = setInterval(() => {
      const f = 57+Math.random()*7;
      setFps(f);
      setFpsHistory(h => [...h.slice(-19), f]);
    }, 800);
    return ()=>clearInterval(iv);
  }, []);

  const cards = [
    {id:'match',icon:'mdi-gamepad-variant-outline',title:'MATCH',status:'good',value:'12 TRACKED',detail:'battle · S199 · watcher',spark:entityHistory,sparkColor:t.good},
    {id:'perf',icon:'mdi-speedometer',title:'PERF',status:fps>=55?'good':'warn',value:`${fps.toFixed(0)} FPS`,detail:`${(1000/fps).toFixed(1)} ms`,spark:fpsHistory,sparkColor:fps>=55?t.good:t.warn},
    {id:'bridge',icon:'mdi-console-network',title:'BRIDGE',status:'good',value:'READY',detail:'python + remote 9099',spark:null},
    {id:'hud',icon:'mdi-monitor-dashboard',title:'HUD',status:'good',value:'3 / 4',detail:'global on',spark:null},
    {id:'vuln',icon:'mdi-flask-outline',title:'VULN LAB',status:'idle',value:'SAFE',detail:'target 150 cached',spark:null},
    {id:'session',icon:'mdi-clock-outline',title:'SESSION',status:'accent',value:'5m 42s',detail:'since overlay start',spark:frameHistory,sparkColor:t.accent},
  ];

  return React.createElement('div', null,
    React.createElement(Section, {label:'LIVE DASHBOARD'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 20px'}}, 'Runtime snapshot of the active injection session.'),
    React.createElement('div', {style:{display:'grid',gridTemplateColumns:'repeat(auto-fill,minmax(230px,1fr))',gap:14}},
      cards.map(c => React.createElement(Glass, {key:c.id, glow:true, style:{height:120,padding:'14px 18px',display:'flex',flexDirection:'column',justifyContent:'space-between'}},
        // Top: icon + title + dot
        React.createElement('div', {style:{display:'flex',alignItems:'center',gap:8}},
          React.createElement('span', {style:{fontSize:13,color:t.textMuted}}, c.icon),
          React.createElement('span', {style:{fontSize:t.cap,fontWeight:600,color:t.textMuted,letterSpacing:0.5}}, c.title),
          React.createElement('span', {style:{marginLeft:'auto'}}),
          React.createElement(Dot, {s:c.status}),
        ),
        // Middle: value + sparkline
        React.createElement('div', {style:{display:'flex',alignItems:'center',justifyContent:'space-between'}},
          React.createElement('span', {style:{fontSize:22,fontWeight:600,color:t.text}}, c.value),
          c.spark && React.createElement(Sparkline, {data:c.spark, width:70, height:26, color:c.sparkColor}),
        ),
        // Bottom: detail
        React.createElement('div', {style:{fontSize:t.cap,color:t.textDim,overflow:'hidden',textOverflow:'ellipsis',whiteSpace:'nowrap'}}, c.detail),
      ))
    )
  );
}

// ── Hooks ──────────────────────────────────────────────────────────────
function V3Hooks() {
  const t = useT();
  const hooks = [
    {name:'IDXGISwapChain::Present',target:'dxgi.dll+0x5A20',s:'good'},
    {name:'IDXGISwapChain::ResizeBuffers',target:'dxgi.dll+0x5B80',s:'good'},
    {name:'WndProc (subclass)',target:'GWLP_WNDPROC',s:'good'},
    {name:'neox_engine!global_logger',target:'neox_engine.dll+0x30DB100',s:'idle'},
  ];
  return React.createElement('div', null,
    React.createElement(Section, {label:'INSTALLED HOOKS'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 16px'}}, 'MinHook RAII wrapper. Clean teardown on unload.'),
    React.createElement('div', {style:{display:'flex',flexDirection:'column',gap:8}},
      hooks.map((h,i) => React.createElement(Glass, {key:i, style:{padding:'14px 18px',display:'flex',alignItems:'center',justifyContent:'space-between'}},
        React.createElement('div', null,
          React.createElement('div', {style:{fontSize:t.ui,fontWeight:500,color:t.text}}, h.name),
          React.createElement('div', {style:{fontSize:t.cap,color:t.textDim,fontFamily:'monospace',marginTop:3}}, h.target),
        ),
        React.createElement(Badge, {s:h.s, label:h.s}),
      ))
    )
  );
}

// ── Entities ───────────────────────────────────────────────────────────
function V3Entities() {
  const t = useT();
  const cats = ['Survivor','Hunter','Avatar','CipherMachine','Gate','Chair','Prop','NPC'];
  const [af, setAf] = useState(new Set(cats));
  const [q, setQ] = useState('');
  const [expanded, setExpanded] = useState(null);
  const entities = [
    {kind:'Survivor',cls:'mobile.entity.SurvivorEntity',addr:'0x1A2F3C4D8E',hp:100,pos:'12.3, 45.6, 0.0',state:'running',team:'civil'},
    {kind:'Survivor',cls:'mobile.entity.SurvivorEntity',addr:'0x1A2F3C5120',hp:75,pos:'-8.1, 22.4, 0.0',state:'decoding',team:'civil'},
    {kind:'Survivor',cls:'mobile.entity.SurvivorEntity',addr:'0x1A2F3C6300',hp:50,pos:'30.5, -12.8, 0.0',state:'healing',team:'civil'},
    {kind:'Hunter',cls:'mobile.entity.HunterEntity',addr:'0x1B3E4D5F70',hp:999,pos:'0.0, 0.0, 0.0',state:'chasing',team:'hunter'},
    {kind:'CipherMachine',cls:'common.prop.CipherMachine',addr:'0x1C4F5E6A80',hp:null,pos:'15.0, 30.0, 0.0',state:'67%',team:null},
    {kind:'Gate',cls:'common.prop.DoorGate',addr:'0x1D5A6B7CA0',hp:null,pos:'50.0, 0.0, 0.0',state:'closed',team:null},
    {kind:'Chair',cls:'common.prop.RocketChair',addr:'0x1E6B7C8DB0',hp:null,pos:'5.0, -15.0, 0.0',state:null,team:null},
    {kind:'Prop',cls:'common.prop.Pallet',addr:'0x1F7C8D9EC0',hp:null,pos:'10.0, 20.0, 0.0',state:'standing',team:null},
  ];
  const fil = entities.filter(e => af.has(e.kind) && (!q || e.cls.toLowerCase().includes(q.toLowerCase())));
  const tog = c => { const n=new Set(af); n.has(c)?n.delete(c):n.add(c); setAf(n); };
  const kindColor = k => k==='Hunter'?t.bad:k==='Survivor'?t.good:t.textMuted;

  return React.createElement('div', null,
    React.createElement(Section, {label:'ENTITY SCANNER', detail:`${fil.length} tracked`}),
    React.createElement('div', {style:{display:'flex',gap:8,alignItems:'center',margin:'8px 0 12px',flexWrap:'wrap'}},
      React.createElement(Btn, {label:'↻ Refresh', variant:'ghost'}),
      React.createElement(Tog, {value:true,onChange:()=>{},label:'Auto'}),
      React.createElement('div', {style:{flex:1}}),
      React.createElement('input', {value:q,onChange:e=>setQ(e.target.value),placeholder:'Filter...',
        style:{background:'rgba(255,255,255,0.04)',border:`1px solid ${t.glassBorder}`,borderRadius:t.rSm,
        padding:'5px 12px',color:t.text,fontSize:12,outline:'none',width:160,fontFamily:'inherit',
        backdropFilter:t.blur}}),
    ),
    React.createElement('div', {style:{display:'flex',gap:4,flexWrap:'wrap',marginBottom:14}},
      cats.map(c => React.createElement(Chip, {key:c,label:c,active:af.has(c),onClick:()=>tog(c)}))
    ),
    // Entity cards instead of flat table
    React.createElement('div', {style:{display:'flex',flexDirection:'column',gap:6}},
      fil.map((e,i) => React.createElement(Glass, {key:i, style:{padding:'0',cursor:'pointer'},
        hover:true},
        React.createElement('div', {
          onClick:()=>setExpanded(expanded===i?null:i),
          style:{display:'grid',gridTemplateColumns:'90px 1fr 120px auto',alignItems:'center',padding:'10px 16px',gap:8}},
          React.createElement('span', {style:{color:kindColor(e.kind),fontWeight:600,fontSize:t.body}}, e.kind),
          React.createElement('span', {style:{color:t.textSec,fontSize:12}}, e.cls),
          React.createElement('span', {style:{color:t.textDim,fontFamily:'monospace',fontSize:11}}, e.addr),
          React.createElement('svg', {width:12,height:12,viewBox:'0 0 12 12',stroke:t.textDim,strokeWidth:1.5,fill:'none',
            style:{transform:expanded===i?'rotate(180deg)':'none',transition:'transform 0.2s'}},
            React.createElement('polyline', {points:'2,4 6,8 10,4'})),
        ),
        // Expanded detail card
        expanded===i && React.createElement('div', {style:{
          padding:'0 16px 14px',display:'grid',gridTemplateColumns:'1fr 1fr 1fr',gap:12,
          borderTop:`1px solid ${t.glassBorder}`,paddingTop:12,
          animation:'fadeSlideUp 0.2s ease',
        }},
          [{l:'Position',v:e.pos},{l:'HP',v:e.hp!=null?String(e.hp):'—'},{l:'State',v:e.state||'—'},
           {l:'Team',v:e.team||'—'},{l:'Address',v:e.addr},{l:'Class',v:e.cls.split('.').pop()}].map((d,j) =>
            React.createElement('div', {key:j},
              React.createElement('div', {style:{fontSize:t.cap-1,color:t.textDim,marginBottom:2}}, d.l),
              React.createElement('div', {style:{fontSize:t.body,color:t.text,fontFamily:d.l==='Address'?'monospace':'inherit'}}, d.v),
            )
          )
        ),
      ))
    )
  );
}

// ── HUD Panel ──────────────────────────────────────────────────────────
function V3Hud() {
  const t = useT();
  const [mods, setMods] = useState([
    {id:'stats',name:'Stats Overlay',desc:'FPS / frame time / counter.',on:true},
    {id:'crosshair',name:'Crosshair',desc:'Center crosshair with a few styles.',on:true},
    {id:'radar',name:'Radar',desc:'Top-down entity radar (relative mode).',on:false},
    {id:'esp',name:'ESP',desc:'World-space ESP overlay with silhouettes.',on:true},
  ]);
  const [g, setG] = useState(true);
  const toggle = id => setMods(m=>m.map(x=>x.id===id?{...x,on:!x.on}:x));

  return React.createElement('div', null,
    React.createElement('div', {style:{display:'flex',alignItems:'center',gap:t.md,marginBottom:t.xl}},
      React.createElement(Tog, {value:g,onChange:setG}),
      React.createElement('span', {style:{fontSize:t.ui,color:t.text,fontWeight:500}}, 'Global HUD'),
      React.createElement(Badge, {s:g?'good':'idle',label:g?'Rendering':'Muted'}),
      React.createElement('div', {style:{flex:1}}),
      React.createElement(Btn, {label:'Enter Edit Mode',variant:'ghost'}),
    ),
    React.createElement(Section, {label:'WIDGETS'}),
    React.createElement('div', {style:{display:'grid',gridTemplateColumns:'repeat(auto-fill,minmax(230px,1fr))',gap:12,marginTop:t.sm}},
      mods.map(m => React.createElement(Glass, {key:m.id, glow:m.on,
        style:{height:120,padding:'16px 20px',display:'flex',flexDirection:'column',justifyContent:'space-between',
          borderColor:m.on?t.accentEdge:undefined}},
        React.createElement('div', null,
          React.createElement('div', {style:{fontSize:t.head,fontWeight:600,color:m.on?t.text:t.textSec}}, m.name),
          React.createElement('div', {style:{fontSize:12,color:t.textDim,marginTop:4,lineHeight:1.4}}, m.desc),
        ),
        React.createElement('div', {style:{display:'flex',alignItems:'center',gap:t.md}},
          React.createElement(Tog, {value:m.on,onChange:()=>toggle(m.id)}),
          React.createElement('span', {style:{fontSize:12,color:t.textMuted}}, m.on?'Active':'Idle'),
        ),
      ))
    )
  );
}

// ── Python REPL ────────────────────────────────────────────────────────
function V3Repl() {
  const t = useT();
  const [input, setInput] = useState('');
  const [lines, setLines] = useState([
    {t:'s',text:'● interpreter attached'},
    {t:'i',text:'>>> import sys; print(sys.version)'},
    {t:'o',text:'3.10.8 (tags/v3.10.8:aaaf517, Oct 11 2022)'},
    {t:'i',text:'>>> import neox; print(sorted(dir(neox)))[:5]'},
    {t:'o',text:"['AudioManager', 'Camera', 'Entity', 'GameManager', 'Input']"},
  ]);
  const submit = () => { if(!input.trim())return; setLines(l=>[...l,{t:'i',text:'>>> '+input},{t:'o',text:'(executed)'}]); setInput(''); };

  return React.createElement('div', {style:{display:'flex',flexDirection:'column',minHeight:360}},
    React.createElement('div', {style:{display:'flex',gap:12,marginBottom:8,alignItems:'center'}},
      React.createElement(Dot, {s:'good',size:8}),
      React.createElement('span', {style:{color:t.textSec,fontSize:t.body}}, 'interpreter attached'),
      React.createElement('span', {style:{color:t.textDim,fontSize:t.cap,marginLeft:12}}, '↑↓ history · Ctrl+L clear'),
    ),
    React.createElement(Glass, {hover:false, style:{flex:1,padding:16,fontFamily:'monospace',fontSize:12,overflowY:'auto',marginBottom:8,minHeight:240}},
      lines.map((l,i) => React.createElement('div', {key:i, style:{
        color:l.t==='s'?t.good:l.t==='i'?t.text:t.textMuted,lineHeight:1.8,
      }}, l.text))
    ),
    React.createElement('input', {value:input,onChange:e=>setInput(e.target.value),
      onKeyDown:e=>e.key==='Enter'&&submit(),placeholder:'>>> ',
      style:{background:'rgba(255,255,255,0.03)',border:`1px solid ${t.glassBorder}`,borderRadius:t.rSm,
      padding:'8px 12px',color:t.text,fontFamily:'monospace',fontSize:t.body,outline:'none',width:'100%',
      backdropFilter:t.blur}}),
  );
}

// ── Quick Actions ──────────────────────────────────────────────────────
function V3QuickActions({ onToast }) {
  const t = useT();
  const actions = [
    {label:'sys.path',sub:'Python module search paths'},
    {label:'Engine modules',sub:'neox/mobile/common namespaces'},
    {label:'__main__ globals',sub:'Main interpreter names'},
    {label:'Interpreter info',sub:'CPython version + flags'},
    {label:'sys hooks',sub:'meta_path + path_hooks'},
    {label:'neox built-ins',sub:'import neox attributes'},
    {label:'Threads',sub:'Alive interpreter threads'},
    {label:'GC sample',sub:'Top 10 classes by count'},
  ];
  return React.createElement('div', null,
    React.createElement(Section, {label:'QUICK ACTIONS'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 14px'}}, 'Curated snippets for the game\'s Python interpreter.'),
    React.createElement('div', {style:{display:'grid',gridTemplateColumns:'repeat(auto-fill,minmax(220px,1fr))',gap:10}},
      actions.map((a,i) => React.createElement(Glass, {key:i, style:{padding:'14px 16px',display:'flex',flexDirection:'column',justifyContent:'space-between',minHeight:82}},
        React.createElement('div', null,
          React.createElement('div', {style:{fontSize:t.ui,fontWeight:500,color:t.text,marginBottom:3}}, a.label),
          React.createElement('div', {style:{fontSize:t.cap,color:t.textDim}}, a.sub),
        ),
        React.createElement('div', {style:{display:'flex',justifyContent:'flex-end',marginTop:8}},
          React.createElement(Btn, {label:'Run',variant:'ghost',onClick:()=>onToast&&onToast('snippet → REPL')}),
        ),
      ))
    )
  );
}

// ── Memory ─────────────────────────────────────────────────────────────
function V3Memory() {
  const t = useT();
  const rows = useMemo(()=>Array.from({length:14},(_,r)=>{
    const base=BigInt('0x7FF6A0000000')+BigInt(r*16);
    const bytes=Array.from({length:16},()=>Math.floor(Math.random()*256));
    return {addr:'0x'+base.toString(16).toUpperCase().padStart(16,'0'),
      hex:bytes.map(b=>b.toString(16).toUpperCase().padStart(2,'0')),
      ascii:bytes.map(b=>b>=32&&b<127?String.fromCharCode(b):'.').join('')};
  }),[]);
  return React.createElement('div', null,
    React.createElement(Section, {label:'MEMORY INSPECTOR'}),
    React.createElement('div', {style:{display:'flex',gap:8,margin:'8px 0 14px',alignItems:'center'}},
      React.createElement('input', {defaultValue:'0x7FF6A0000000',style:{background:'rgba(255,255,255,0.03)',border:`1px solid ${t.glassBorder}`,borderRadius:t.rSm,padding:'5px 12px',color:t.text,fontFamily:'monospace',fontSize:12,width:200,outline:'none'}}),
      React.createElement(Btn, {label:'Go',variant:'ghost'}),
      React.createElement(Btn, {label:'-0x100',variant:'ghost',style:{fontFamily:'monospace',fontSize:11,padding:'5px 8px'}}),
      React.createElement(Btn, {label:'+0x100',variant:'ghost',style:{fontFamily:'monospace',fontSize:11,padding:'5px 8px'}}),
    ),
    React.createElement(Glass, {hover:false, style:{padding:14,overflowX:'auto',maxHeight:320,overflowY:'auto'}},
      rows.map((r,i) => React.createElement('div', {key:i,style:{fontFamily:'monospace',fontSize:12,lineHeight:1.9,whiteSpace:'nowrap',display:'flex',gap:4}},
        React.createElement('span', {style:{color:t.accent,marginRight:8}}, r.addr),
        r.hex.map((b,j) => React.createElement('span', {key:j,style:{
          color:b==='00'?t.textDim:parseInt(b,16)>127?t.info:t.text,
          width:22,textAlign:'center',display:'inline-block',
          background:parseInt(b,16)===0?'transparent':'rgba(255,255,255,0.02)',
          borderRadius:2,
        }}, b)),
        React.createElement('span', {style:{color:t.textDim,marginLeft:8}}, r.ascii),
      ))
    )
  );
}

// ── RPC Tracer ─────────────────────────────────────────────────────────
function V3RpcTracer() {
  const t = useT();
  const [armed,setArmed]=useState(false);
  const [filter,setFilter]=useState('');
  const rows=[{tag:'async::mb_gate_service::seed_request',site:'asiocore.cpp:142',lvl:'info'},
    {tag:'async::scene_service::load_scene',site:'scene_mgr.cpp:89',lvl:'info'},
    {tag:'async::entity_service::spawn_entity',site:'entity_mgr.cpp:201',lvl:'warn'},
    {tag:'async::rpc::match_start',site:'match.cpp:55',lvl:'info'},
    {tag:'async::rpc::player_ready',site:'lobby.cpp:112',lvl:'info'},
    {tag:'async::net::heartbeat',site:'netcore.cpp:33',lvl:'info'}];
  const fil=rows.filter(r=>!filter||r.tag.includes(filter));
  return React.createElement('div', null,
    React.createElement(Section, {label:'RPC TRACER'}),
    React.createElement('div', {style:{display:'flex',gap:8,alignItems:'center',margin:'8px 0 14px',flexWrap:'wrap'}},
      React.createElement(Badge, {s:armed?'good':'idle',label:armed?'Armed':'Idle'}),
      armed?React.createElement(Btn,{label:'Pause',variant:'ghost',onClick:()=>setArmed(false)})
           :React.createElement(Btn,{label:'Arm hook',variant:'primary',onClick:()=>setArmed(true)}),
      React.createElement(Btn,{label:'Clear',variant:'ghost'}),
      React.createElement('div',{style:{flex:1}}),
      React.createElement('input',{value:filter,onChange:e=>setFilter(e.target.value),placeholder:'Filter...',
        style:{background:'rgba(255,255,255,0.03)',border:`1px solid ${t.glassBorder}`,borderRadius:t.rSm,padding:'5px 12px',color:t.text,fontSize:12,width:180,outline:'none',fontFamily:'inherit'}}),
    ),
    React.createElement(Glass, {hover:false, style:{overflow:'hidden'}},
      React.createElement('div', {style:{display:'grid',gridTemplateColumns:'40px 1fr 140px',padding:'8px 14px',borderBottom:`1px solid ${t.glassBorder}`}},
        ['#','Tag','Site'].map(h=>React.createElement('span',{key:h,style:{fontSize:t.cap,color:t.textDim,fontWeight:600}},h))),
      fil.map((r,i)=>React.createElement('div',{key:i,style:{display:'grid',gridTemplateColumns:'40px 1fr 140px',padding:'8px 14px',borderBottom:`1px solid rgba(255,255,255,0.03)`,fontSize:12,
        transition:'background 0.1s'},
        onMouseEnter:e=>{e.currentTarget.style.background='rgba(255,255,255,0.03)'},
        onMouseLeave:e=>{e.currentTarget.style.background='transparent'}},
        React.createElement('span',{style:{color:t.textDim}},i+1),
        React.createElement('span',{style:{color:t.text,fontFamily:'monospace',fontSize:11}},r.tag),
        React.createElement('span',{style:{color:t.textMuted}},r.site),
      ))
    )
  );
}

// ── Settings ───────────────────────────────────────────────────────────
function V3Settings({ onPreviewSplash }) {
  const t = useT();
  const [lang,setLang]=useState(0);
  return React.createElement('div', null,
    React.createElement(Section, {label:'CONFIGURATION'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 20px'}}, 'Overlay settings persisted to DXSense.json.'),
    React.createElement('div', {style:{fontSize:t.cap,color:t.textDim,marginBottom:8,letterSpacing:0.5}}, 'LANGUAGE'),
    React.createElement('div', {style:{display:'inline-flex',borderRadius:t.rSm,border:`1px solid ${t.glassBorder}`,overflow:'hidden',marginBottom:24}},
      ['English','简体中文'].map((l,i)=>React.createElement('button',{key:i,onClick:()=>setLang(i),style:{
        background:lang===i?'rgba(255,255,255,0.12)':'transparent',color:lang===i?t.text:t.textMuted,
        border:'none',padding:'6px 20px',fontSize:t.body,cursor:'pointer',fontFamily:'inherit',
        borderRight:i===0?`1px solid ${t.glassBorder}`:'none',transition:'all 0.15s',
        backdropFilter:lang===i?t.blur:'none',
      }},l))),
    React.createElement(Section, {label:'KEYBINDS'}),
    React.createElement(Glass, {hover:false, style:{padding:16,marginTop:8,marginBottom:24}},
      [{a:'overlay.toggle',k:'INSERT'},{a:'radar.zoom_in',k:'Ctrl+='},{a:'radar.zoom_out',k:'Ctrl+-'},{a:'command_palette',k:'Ctrl+K'}].map((b,i,arr)=>
        React.createElement('div',{key:i,style:{display:'flex',justifyContent:'space-between',padding:'10px 0',borderBottom:i<arr.length-1?`1px solid rgba(255,255,255,0.04)`:'none'}},
          React.createElement('span',{style:{color:t.text,fontSize:t.body}},b.a),
          React.createElement('span',{style:{color:t.accent,fontSize:12,fontFamily:'monospace',background:'rgba(255,255,255,0.06)',padding:'3px 12px',borderRadius:t.rXs}},b.k),
        ))),
    React.createElement('div', {style:{height:1,background:t.outlineVariant,margin:'20px 0'}}),
    React.createElement(Section, {label:'CONFIG'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:12,fontFamily:'monospace',margin:'8px 0 16px'}}, 'C:\\Games\\dwrg\\DXSense.json'),
    React.createElement(Btn, {label:'Reset all settings',variant:'danger'}),
  );
}

// ── Vuln Lab ───────────────────────────────────────────────────────────
function V3VulnLab() {
  const t = useT();
  const [armed,setArmed]=useState(false);
  return React.createElement('div', null,
    React.createElement(Section, {label:'VULN LAB'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 16px'}}, 'Passive skill injection for speed/ability modifications.'),
    React.createElement(Glass, {hover:false, glow:armed, style:{padding:'14px 18px',marginBottom:16,
      borderColor:armed?'rgba(248,113,113,0.3)':undefined,
      background:armed?'rgba(248,113,113,0.04)':undefined}},
      React.createElement('div', {style:{display:'flex',alignItems:'center',gap:12}},
        React.createElement(Tog, {value:armed,onChange:setArmed}),
        React.createElement('span', {style:{fontSize:t.ui,fontWeight:600,color:armed?t.bad:t.text}},
          armed?'ARMED — MODIFICATIONS ACTIVE':'Safe mode'),
        React.createElement('div', {style:{flex:1}}),
        React.createElement(Dot, {s:armed?'bad':'idle',size:9}),
      ),
    ),
    React.createElement('div', {style:{display:'flex',gap:8}},
      React.createElement(Btn, {label:'Apply',variant:'primary'}),
      React.createElement(Btn, {label:'Reset',variant:'danger'}),
    ),
  );
}

// ── Interaction Father ─────────────────────────────────────────────────
function V3Interaction() {
  const t = useT();
  const events = [
    {method:'on_attacked',actor:'HunterEntity#882',args:'dmg=20',ts:'0.4s'},
    {method:'on_decode_progress',actor:'CipherMachine#12',args:'delta=0.02',ts:'1.2s'},
    {method:'on_state_change',actor:'SurvivorEntity#441',args:'idle→running',ts:'2.8s'},
    {method:'on_interaction',actor:'Pallet#77',args:'type=drop',ts:'4.1s'},
  ];
  return React.createElement('div', null,
    React.createElement(Section, {label:'INTERACTION FATHER'}),
    React.createElement('p', {style:{color:t.textMuted,fontSize:t.body,margin:'4px 0 14px'}}, 'Live method-call hooks on game interaction layer.'),
    React.createElement('div', {style:{display:'flex',gap:8,marginBottom:14}},
      React.createElement(Btn, {label:'Install hooks',variant:'primary'}),
      React.createElement(Btn, {label:'Clear',variant:'ghost'}),
    ),
    React.createElement('div', {style:{display:'flex',flexDirection:'column',gap:6}},
      events.map((e,i) => React.createElement(Glass, {key:i, style:{padding:'10px 16px',display:'grid',gridTemplateColumns:'150px 170px 1fr 60px',alignItems:'center',gap:8}},
        React.createElement('span', {style:{color:t.accent,fontFamily:'monospace',fontSize:12}}, e.method),
        React.createElement('span', {style:{color:t.text,fontSize:12}}, e.actor),
        React.createElement('span', {style:{color:t.textMuted,fontSize:12}}, e.args),
        React.createElement('span', {style:{color:t.textDim,fontSize:t.cap,textAlign:'right'}}, e.ts),
      ))
    )
  );
}

Object.assign(window, {
  V3Overview, V3Hooks, V3Entities, V3Hud, V3Repl, V3QuickActions,
  V3Memory, V3RpcTracer, V3Settings, V3VulnLab, V3Interaction,
});

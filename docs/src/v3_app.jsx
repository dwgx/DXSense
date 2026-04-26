// ── v3 App Shell — glassmorphism, splash, command palette ──────────────

function DXSenseV3() {
  const [tweaks, setTweaks] = useState(() => {
    try { return {...TWEAK_DEFAULTS,...JSON.parse(localStorage.getItem('dxs3_tw')||'{}')}; }
    catch { return TWEAK_DEFAULTS; }
  });
  const [panel, setPanel] = useState(() => localStorage.getItem('dxs3_p') || 'overview');
  const [toasts, setToasts] = useState([]);
  const [showTweaks, setShowTweaks] = useState(false);
  const [cmdOpen, setCmdOpen] = useState(false);
  const [splashDone, setSplashDone] = useState(() => sessionStorage.getItem('dxs3_splash')==='1');
  const [splashMode, setSplashMode] = useState('intro');
  const [showSplashPreview, setShowSplashPreview] = useState(false);
  const [splashKey, setSplashKey] = useState(0);
  const theme = useMemo(() => makeTheme(tweaks), [tweaks]);

  useEffect(() => { localStorage.setItem('dxs3_p', panel); }, [panel]);
  useEffect(() => { localStorage.setItem('dxs3_tw', JSON.stringify(tweaks)); }, [tweaks]);

  const toast = useCallback(text => {
    const id = Date.now();
    setToasts(t => [...t, {id,text}]);
    setTimeout(() => setToasts(t => t.map(x => x.id===id?{...x,fading:true}:x)), 1600);
    setTimeout(() => setToasts(t => t.filter(x => x.id!==id)), 2000);
  }, []);

  const previewSplash = useCallback((mode) => {
    setSplashMode(mode);
    setSplashKey(k => k+1);
    setShowSplashPreview(true);
  }, []);

  const onSplashDone = useCallback(() => { setSplashDone(true); sessionStorage.setItem('dxs3_splash','1'); }, []);

  // Tweaks protocol
  useEffect(() => {
    const h = e => {
      if(!e.data?.type) return;
      if(e.data.type==='__activate_edit_mode') setShowTweaks(true);
      if(e.data.type==='__deactivate_edit_mode') setShowTweaks(false);
    };
    window.addEventListener('message', h);
    window.parent.postMessage({type:'__edit_mode_available'}, '*');
    return () => window.removeEventListener('message', h);
  }, []);

  const updateTw = (k, v) => {
    const next = {...tweaks, [k]:v};
    setTweaks(next);
    window.parent.postMessage({type:'__edit_mode_set_keys',edits:{[k]:v}}, '*');
  };

  const panels = [
    {id:'overview',title:'Overview',cat:'Core',icon:'mdi-view-dashboard-outline'},
    {id:'hooks',title:'Hooks',cat:'Core',icon:'mdi-hook'},
    {id:'entities',title:'Entities',cat:'Inspection',icon:'mdi-account-group-outline'},
    {id:'matrix',title:'Matrix',cat:'Inspection',icon:'mdi-grid'},
    {id:'raycast',title:'Raycast',cat:'Inspection',icon:'mdi-ray-start-arrow'},
    {id:'rpc_tracer',title:'RPC Tracer',cat:'Inspection',icon:'mdi-swap-horizontal'},
    {id:'ac_obs',title:'AC Observatory',cat:'Inspection',icon:'mdi-shield-search'},
    {id:'memory',title:'Memory',cat:'Inspection',icon:'mdi-memory'},
    {id:'repl',title:'Python REPL',cat:'Scripting',icon:'mdi-console'},
    {id:'quick',title:'Quick Actions',cat:'Scripting',icon:'mdi-lightning-bolt'},
    {id:'modules',title:'Modules',cat:'Functional',icon:'mdi-puzzle-outline'},
    {id:'hud',title:'HUD',cat:'Functional',icon:'mdi-monitor-dashboard'},
    {id:'velocity',title:'Velocity Lab',cat:'Lab',icon:'mdi-speedometer'},
    {id:'vuln',title:'Vuln Lab',cat:'Lab',icon:'mdi-flask-outline'},
    {id:'interact',title:'Interaction Father',cat:'Lab',icon:'mdi-gesture-tap'},
    {id:'settings',title:'Settings',cat:'',icon:'mdi-cog-outline'},
  ];

  const render = () => {
    switch(panel) {
      case 'overview': return React.createElement(V3Overview);
      case 'hooks': return React.createElement(V3Hooks);
      case 'entities': return React.createElement(V3Entities);
      case 'rpc_tracer': return React.createElement(V3RpcTracer);
      case 'memory': return React.createElement(V3Memory);
      case 'repl': return React.createElement(V3Repl);
      case 'quick': return React.createElement(V3QuickActions, {onToast:toast});
      case 'hud': return React.createElement(V3Hud);
      case 'vuln': return React.createElement(V3VulnLab);
      case 'interact': return React.createElement(V3Interaction);
      case 'settings': return React.createElement(V3Settings, {onPreviewSplash:previewSplash});
      case 'modules': return React.createElement('div', null,
        React.createElement(Section, {label:'FUNCTIONAL MODULES'}),
        React.createElement('p', {style:{color:theme.textMuted,fontSize:theme.body,marginTop:6}}, 'Placeholder for future modules. See HUD panel for widget management.'));
      default: return React.createElement('div', null,
        React.createElement(Section, {label:panels.find(p=>p.id===panel)?.title?.toUpperCase()||'PANEL'}),
        React.createElement('p', {style:{color:theme.textMuted,fontSize:theme.body,marginTop:6}}, 'Panel content — coming soon.'));
    }
  };

  const active = panels.find(p=>p.id===panel);
  const grouped = useMemo(() => {
    const m = new Map();
    panels.forEach(p => { const c=p.cat||''; if(!m.has(c))m.set(c,[]); m.get(c).push(p); });
    return m;
  }, []);

  // Sidebar hover tracking for animated indicator
  const [sidebarIndicator, setSidebarIndicator] = useState({y:0,h:36});
  const sidebarRef = useRef(null);
  const rowRefs = useRef({});

  useEffect(() => {
    const el = rowRefs.current[panel];
    if(el && sidebarRef.current) {
      const sRect = sidebarRef.current.getBoundingClientRect();
      const rRect = el.getBoundingClientRect();
      setSidebarIndicator({y:rRect.top-sRect.top, h:rRect.height});
    }
  }, [panel]);

  return React.createElement(ThemeCtx.Provider, {value:theme},
    // Splash — initial boot
    !splashDone && React.createElement(SplashScreen, {onDone:onSplashDone, mode:'intro'}),

    // Splash — preview from settings
    showSplashPreview && React.createElement(SplashScreen, {
      key: splashKey,
      onDone:()=>setShowSplashPreview(false),
      mode:splashMode,
    }),

    // Command Palette
    React.createElement(CmdPalette, {open:cmdOpen,
      onClose:a=>{if(a==='toggle')setCmdOpen(o=>!o);else setCmdOpen(false)},
      onSelect:setPanel, panels}),

    React.createElement('div', {style:{
      width:'100vw',height:'100vh',display:'flex',justifyContent:'center',alignItems:'center',
      background:theme.bg,
    }},
      // Background subtle noise texture
      React.createElement('div', {style:{position:'fixed',inset:0,
        background:`radial-gradient(ellipse at 30% 20%, rgba(255,255,255,0.015) 0%, transparent 60%),
                    radial-gradient(ellipse at 70% 80%, rgba(255,255,255,0.01) 0%, transparent 50%)`,
        zIndex:0}}),

      // Scrim
      React.createElement('div', {style:{position:'fixed',inset:0,background:'rgba(6,6,6,0.5)',zIndex:0}}),

      // Window
      React.createElement('div', {style:{
        width:1100,maxWidth:'96vw',height:720,maxHeight:'94vh',
        borderRadius:theme.rXl, display:'flex',flexDirection:'column',overflow:'hidden',
        background:'rgba(10,10,10,0.85)', backdropFilter:'blur(40px)',
        border:`1px solid rgba(255,255,255,0.06)`,
        boxShadow:`0 0 0 0.5px rgba(255,255,255,0.04), 0 24px 80px rgba(0,0,0,0.7), inset 0 1px 0 rgba(255,255,255,0.04)`,
        position:'relative',zIndex:1,
        animation: splashDone ? 'fadeSlideUp 0.4s ease both' : 'none',
      }},

        // ── Header ──
        React.createElement('div', {style:{
          height:theme.headerH, display:'flex',alignItems:'center',padding:'0 20px',
          background:'rgba(14,14,14,0.8)', backdropFilter:'blur(20px)',
          borderBottom:`1px solid rgba(255,255,255,0.05)`,flexShrink:0,
        }},
          // Brand mark
          React.createElement('div', {style:{
            width:20,height:20,borderRadius:5,
            background:'linear-gradient(135deg,#e0e0e0,#808080)',
            display:'flex',alignItems:'center',justifyContent:'center',marginRight:12,
            boxShadow:'0 0 12px rgba(255,255,255,0.08)',
          }},
            React.createElement('div', {style:{width:10,height:10,borderRadius:2,background:'rgba(0,0,0,0.7)'}}),
          ),
          React.createElement('div', null,
            React.createElement('div', {style:{fontSize:15,fontWeight:600,color:theme.text,lineHeight:1.2}}, 'DXSense'),
            React.createElement('div', {style:{fontSize:11,color:theme.textDim}}, 'NeoX3 runtime overlay · dwrg'),
          ),
          React.createElement('div', {style:{marginLeft:'auto',display:'flex',alignItems:'center',gap:14}},
            // Cmd+K hint
            React.createElement('div', {onClick:()=>setCmdOpen(true),
              style:{display:'flex',alignItems:'center',gap:6,padding:'4px 10px',
                borderRadius:theme.rSm,border:`1px solid ${theme.glassBorder}`,
                cursor:'pointer',transition:'all 0.15s',fontSize:12,color:theme.textDim},
              onMouseEnter:e=>{e.currentTarget.style.borderColor=theme.accentEdge;e.currentTarget.style.color=theme.textSec},
              onMouseLeave:e=>{e.currentTarget.style.borderColor=theme.glassBorder;e.currentTarget.style.color=theme.textDim},
            },
              React.createElement('span', null, '⌘K'),
              React.createElement('span', {style:{color:theme.textDim}}, 'Search'),
            ),
            React.createElement('span', {style:{fontSize:13,color:theme.textSec}}, '61 FPS'),
            // Close
            React.createElement('div', {
              onClick:()=>toast('Overlay hidden (INSERT to reopen)'),
              style:{width:26,height:26,borderRadius:theme.rSm,display:'flex',alignItems:'center',justifyContent:'center',cursor:'pointer',color:theme.textMuted,transition:'all 0.15s'},
              onMouseEnter:e=>{e.currentTarget.style.background='rgba(255,255,255,0.08)';e.currentTarget.style.color=theme.text},
              onMouseLeave:e=>{e.currentTarget.style.background='transparent';e.currentTarget.style.color=theme.textMuted},
            },
              React.createElement('svg', {width:11,height:11,viewBox:'0 0 12 12',stroke:'currentColor',strokeWidth:1.6,strokeLinecap:'round'},
                React.createElement('line', {x1:1,y1:1,x2:11,y2:11}),
                React.createElement('line', {x1:11,y1:1,x2:1,y2:11}),
              )
            ),
          ),
        ),

        // ── Body ──
        React.createElement('div', {style:{display:'flex',flex:1,overflow:'hidden'}},

          // ── Sidebar ──
          React.createElement('div', {ref:sidebarRef, style:{
            width:theme.sidebarW,padding:`${theme.md}px 0`,
            overflowY:'auto',flexShrink:0,position:'relative',
          }},
            // Animated indicator bar
            React.createElement('div', {style:{
              position:'absolute',left:6,width:2,borderRadius:1,
              background:theme.accentHot,
              top:sidebarIndicator.y+10,height:sidebarIndicator.h-20,
              transition:'top 0.25s cubic-bezier(0.4,0,0.2,1), height 0.2s ease',
              boxShadow:'0 0 8px rgba(255,255,255,0.2)',
            }}),

            React.createElement('div', {style:{height:theme.md}}),
            Array.from(grouped.entries()).map(([cat, items]) =>
              React.createElement('div', {key:cat||'root'},
                cat && React.createElement('div', {style:{
                  fontSize:11,color:theme.textDim,letterSpacing:0.6,
                  padding:`${theme.md}px ${theme.sm}px ${theme.xs}px ${theme.lg+4}px`,
                }}, cat),
                items.map(p => {
                  const act = panel===p.id;
                  return React.createElement('div', {
                    key:p.id, ref:el=>{rowRefs.current[p.id]=el},
                    onClick:()=>setPanel(p.id),
                    style:{
                      display:'flex',alignItems:'center',height:theme.rowH,
                      padding:`0 ${theme.lg}px`,cursor:'pointer',position:'relative',
                      borderRadius:`0 ${theme.rSm}px ${theme.rSm}px 0`,
                      marginRight:theme.sm,
                      background:act?'rgba(255,255,255,0.05)':'transparent',
                      transition:'background 0.15s',
                    },
                    onMouseEnter:e=>{if(!act)e.currentTarget.style.background='rgba(255,255,255,0.03)'},
                    onMouseLeave:e=>{if(!act)e.currentTarget.style.background='transparent'},
                  },
                    React.createElement('i', {className:`mdi ${p.icon}`, style:{
                      fontSize:16,marginRight:10,width:20,textAlign:'center',
                      color:act?theme.text:theme.textMuted,transition:'color 0.15s',
                    }}),
                    React.createElement('span', {style:{
                      fontSize:13,fontWeight:act?500:400,
                      color:act?theme.text:theme.textSec,transition:'color 0.15s',
                    }}, p.title)
                  );
                })
              )
            ),
          ),

          // ── Content ──
          React.createElement('div', {style:{
            flex:1,margin:`${theme.sm}px ${theme.sm}px ${theme.sm}px 0`,
            display:'flex',flexDirection:'column',
          }},
            React.createElement('div', {style:{
              flex:1, borderRadius:theme.r,
              background:'rgba(255,255,255,0.028)',
              border:`1px solid rgba(255,255,255,0.06)`,
              backdropFilter:'blur(8px)',
              padding:'40px 48px 36px',overflowY:'auto',
              boxShadow:'inset 0 1px 0 rgba(255,255,255,0.04)',
            }},
              active && React.createElement('div', {style:{marginBottom:theme.xl}},
                React.createElement('h2', {style:{fontSize:theme.title,fontWeight:600,color:theme.text,margin:0}}, active.title),
                React.createElement('div', {style:{fontSize:12,color:theme.textDim,marginTop:4}},
                  active.cat ? `${active.cat} / ${active.id}` : active.id),
              ),
              React.createElement('div', {key:panel, style:{animation:'fadeSlideUp 0.25s ease'}}, render())
            ),
          ),
        ),

        // Toasts
        React.createElement(Toasts, {toasts}),
      ),

      // ── Tweaks Panel ──
      showTweaks && React.createElement('div', {style:{
        position:'fixed',bottom:16,right:16,width:260,
        background:'rgba(16,16,16,0.92)',backdropFilter:'blur(20px)',
        borderRadius:theme.r,boxShadow:'0 4px 24px rgba(0,0,0,0.5)',
        padding:16,zIndex:999,border:`1px solid rgba(255,255,255,0.08)`,
      }},
        React.createElement('div', {style:{fontSize:14,fontWeight:600,color:theme.text,marginBottom:14}}, 'Tweaks'),
        [{k:'glassBlur',l:'Glass Blur',min:4,max:32,s:1,u:'px'},
         {k:'glassTint',l:'Glass Tint',min:2,max:15,s:1,u:'%'},
         {k:'sidebarWidth',l:'Sidebar Width',min:160,max:260,s:4,u:'px'},
         {k:'cornerRadius',l:'Corner Radius',min:6,max:22,s:1,u:'px'},
        ].map(s=>React.createElement('div',{key:s.k,style:{marginBottom:12}},
          React.createElement('label',{style:{fontSize:11,color:theme.textDim,display:'block',marginBottom:4}},`${s.l}: ${tweaks[s.k]}${s.u}`),
          React.createElement('input',{type:'range',min:s.min,max:s.max,step:s.s,value:tweaks[s.k],
            onChange:e=>updateTw(s.k,+e.target.value),
            style:{width:'100%',accentColor:theme.accent}}),
        )),
      ),
    )
  );
}

ReactDOM.createRoot(document.getElementById('root')).render(React.createElement(DXSenseV3));

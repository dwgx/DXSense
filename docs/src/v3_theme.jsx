const { useState, useEffect, useRef, useCallback, useMemo, createContext, useContext } = React;

const ThemeCtx = createContext(null);
const useT = () => useContext(ThemeCtx);

function makeTheme(tw) {
  const blur = tw.glassBlur||16;
  const tint = (tw.glassTint||6)/100;
  const r = tw.cornerRadius||14;
  return {
    // Glass surfaces
    glass: `rgba(255,255,255,${(tint*0.6).toFixed(3)})`,
    glassBorder: `rgba(255,255,255,${(tint*1.2).toFixed(3)})`,
    glassHover: `rgba(255,255,255,${(tint*1.8).toFixed(3)})`,
    glassStrong: `rgba(255,255,255,${(tint*2.5).toFixed(3)})`,
    blur: `blur(${blur}px)`,
    // Solids
    bg: '#060606',
    surfaceDim: '#0a0a0a',
    surface: '#0e0e0e',
    surfaceLow: '#121212',
    surfaceMid: '#181818',
    surfaceHigh: '#222222',
    surfaceHighest: '#2c2c2c',
    // Text
    text: '#f0f0f0',
    textSec: '#a0a0a0',
    textMuted: '#686868',
    textDim: '#404040',
    // Accent — pure white/silver
    accent: '#d4d4d4',
    accentHot: '#ffffff',
    accentSoft: 'rgba(255,255,255,0.08)',
    accentEdge: 'rgba(255,255,255,0.20)',
    // Semantic
    good:'#4ADE80', warn:'#FBBF24', bad:'#F87171', info:'#60A5FA',
    // Radius
    r, rSm:r*0.5, rXs:4, rXl:r*1.3,
    // Layout
    sidebarW: tw.sidebarWidth||200,
    headerH: 52,
    rowH: 38,
    // Spacing
    xs:4, sm:8, md:12, lg:16, xl:24, xxl:32,
    // Type
    cap:11, body:13, ui:14, head:17, title:22, hero:80,
  };
}

Object.assign(window, { ThemeCtx, useT, makeTheme });

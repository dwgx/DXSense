"""Thin JSON-RPC helper for the ida-pro-mcp plugin running inside IDA.

The plugin listens on http://127.0.0.1:13337/mcp with standard MCP
tools/list + tools/call semantics. This file exposes two primitives —
`call()` for a single tool call, and `pool()` for trivially parallel
batches — so the other analysis scripts can query IDA without re-writing
the urllib boilerplate every time.
"""
from __future__ import annotations
import json, urllib.request, urllib.error, time
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import Any, Iterable

ENDPOINT = "http://127.0.0.1:13337/mcp"
_req_id  = 0

class IdaRpcError(RuntimeError):
    pass

def _next_id() -> int:
    global _req_id
    _req_id += 1
    return _req_id

def _post(payload: dict, timeout: float = 60.0) -> dict:
    body = json.dumps(payload).encode()
    req  = urllib.request.Request(ENDPOINT, data=body,
                                  headers={'Content-Type': 'application/json'})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            raw = resp.read()
            return json.loads(raw.decode())
    except urllib.error.HTTPError as e:
        raise IdaRpcError(f"HTTP {e.code}: {e.read()[:200]!r}") from e
    except TimeoutError as e:
        raise IdaRpcError(f"timeout talking to IDA at {ENDPOINT}") from e

def list_tools() -> list[dict]:
    r = _post({'jsonrpc': '2.0', 'id': _next_id(),
               'method': 'tools/list', 'params': {}})
    if 'error' in r:
        raise IdaRpcError(f"tools/list error: {r['error']}")
    return r['result']['tools']

def call(tool: str, arguments: dict | None = None, timeout: float = 120.0) -> Any:
    """Invoke `tool` on the IDA plugin; return the structured result (or raise)."""
    r = _post({
        'jsonrpc': '2.0', 'id': _next_id(),
        'method':  'tools/call',
        'params':  {'name': tool, 'arguments': arguments or {}},
    }, timeout=timeout)
    if 'error' in r:
        raise IdaRpcError(f"{tool} error: {r['error']}")
    result = r.get('result') or {}
    # MCP tools can return either {content:[...]} OR {structuredContent:{...}}.
    if 'structuredContent' in result:
        return result['structuredContent']
    # Fallback: concatenate text content for tools that don't return structured.
    content = result.get('content') or []
    texts = [c.get('text') for c in content if c.get('type') == 'text']
    if texts:
        # Try to decode JSON-in-text (many IDA tools do this).
        joined = ''.join(t or '' for t in texts)
        try:
            return json.loads(joined)
        except Exception:
            return joined
    return result

def pool(calls: Iterable[tuple[str, dict]], workers: int = 8,
         timeout: float = 120.0) -> list[tuple[tuple[str, dict], Any]]:
    """Concurrent batch; yields (request, result_or_exception) in order."""
    calls = list(calls)
    out: list[Any] = [None] * len(calls)
    def run(i):
        tool, args = calls[i]
        try:    return i, call(tool, args, timeout)
        except Exception as e: return i, e
    with ThreadPoolExecutor(max_workers=workers) as ex:
        for fut in as_completed([ex.submit(run, i) for i in range(len(calls))]):
            i, res = fut.result()
            out[i] = res
    return list(zip(calls, out))

# ---- small convenience wrappers we'll call a lot ---------------------------
def health() -> dict:
    return call('server_health')

def warmup(wait_auto: bool = True, init_hexrays: bool = True,
           build_caches: bool = False) -> dict:
    return call('server_warmup',
                {'wait_auto_analysis': wait_auto,
                 'init_hexrays':       init_hexrays,
                 'build_caches':       build_caches},
                timeout=600.0)

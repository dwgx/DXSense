"""Scan neox_engine.dll exports for CPython C API entry points.

NeoX embeds CPython statically. If the engine re-exports PyRun_SimpleString /
PyImport_AddModule / etc., we can call them directly from an injected DLL or
Frida script without needing to scan for them in memory.

Output: list of exported Python C API symbols with RVAs.
"""
import re, sys
from pathlib import Path
import pefile

TARGET = Path(sys.argv[1] if len(sys.argv) > 1
              else r"D:\Project\DXSense\analysis\neox_engine.dll")

# Curated list of CPython 3.11 symbols that matter for embedding / introspection.
INTERESTING = {
    # Runtime init / GIL
    'Py_Initialize', 'Py_InitializeEx', 'Py_IsInitialized', 'Py_Finalize',
    'PyGILState_Ensure', 'PyGILState_Release', 'PyGILState_GetThisThreadState',
    'PyEval_InitThreads', 'PyEval_SaveThread', 'PyEval_RestoreThread',
    'PyThreadState_Get', 'PyThreadState_Swap', 'PyInterpreterState_Get',

    # Eval / run strings
    'PyRun_SimpleString', 'PyRun_SimpleStringFlags',
    'PyRun_String', 'PyRun_StringFlags', 'PyRun_File', 'PyRun_FileEx',
    'Py_CompileString', 'Py_CompileStringFlags',
    'PyEval_EvalCode', 'PyEval_EvalCodeEx',

    # Objects
    'PyObject_GetAttr', 'PyObject_GetAttrString', 'PyObject_SetAttrString',
    'PyObject_CallObject', 'PyObject_Call', 'PyObject_CallFunctionObjArgs',
    'PyObject_Str', 'PyObject_Repr', 'PyObject_Dir',
    'PyObject_IsInstance', 'PyObject_Type',

    # Modules / import
    'PyImport_AddModule', 'PyImport_ImportModule', 'PyImport_GetModuleDict',
    'PyImport_Import', 'PyImport_ReloadModule',

    # Strings
    'PyUnicode_FromString', 'PyUnicode_AsUTF8', 'PyUnicode_AsUTF8String',
    'PyUnicode_DecodeUTF8', 'PyBytes_AsString', 'PyBytes_FromString',

    # Dict / list
    'PyDict_New', 'PyDict_GetItemString', 'PyDict_SetItemString',
    'PyList_New', 'PyList_Append',

    # Error handling
    'PyErr_Print', 'PyErr_Fetch', 'PyErr_Occurred', 'PyErr_Clear',
    'PyErr_SetString',

    # Marshal (for bytecode)
    'PyMarshal_ReadObjectFromString', 'PyMarshal_WriteObjectToString',

    # Main module / dict
    'PyModule_GetDict', 'PyModule_AddObject',

    # Globals
    '_Py_NoneStruct', '_Py_TrueStruct', '_Py_FalseStruct',
    'PyExc_Exception', 'PyExc_RuntimeError',
}

# Broader regex to catch any Py*/_Py* we might want later.
PY_RE = re.compile(r'^_?(Py|_Py)[A-Z][A-Za-z_0-9]*$')

def main() -> int:
    pe = pefile.PE(str(TARGET))
    if not hasattr(pe, 'DIRECTORY_ENTRY_EXPORT'):
        print("no exports")
        return 1

    found_key = []     # curated
    found_all = []     # any Py*
    for s in pe.DIRECTORY_ENTRY_EXPORT.symbols:
        if not s.name: continue
        nm = s.name.decode('latin-1', 'replace')
        if PY_RE.match(nm):
            found_all.append((nm, s.address))
            if nm in INTERESTING or ('Py_' + nm.split('Py_', 1)[-1] in INTERESTING):
                found_key.append((nm, s.address))

    print(f"CPython C API exports found: {len(found_all)} total, "
          f"{len(found_key)} high-value hits\n")

    if found_key:
        print("--- High-value exports (for task E: injected Python REPL) ---")
        for nm, rva in sorted(found_key):
            print(f"  {nm:<40} RVA 0x{rva:08X}")
    else:
        print("(none of the curated interesting symbols are exported; "
              "Python C API is internal-only — Frida will need to resolve by "
              "pattern scan or PDB load instead)")

    # Sample of other Py* exports we can use later.
    others = [p for p in found_all if p[0] not in {n for n, _ in found_key}]
    if others:
        print(f"\n--- Other Py* / _Py* exports (sample, {len(others)} total) ---")
        for nm, rva in sorted(others)[:30]:
            print(f"  {nm:<40} RVA 0x{rva:08X}")
        if len(others) > 30:
            print(f"  ... ({len(others)-30} more)")
    return 0

if __name__ == '__main__':
    raise SystemExit(main())

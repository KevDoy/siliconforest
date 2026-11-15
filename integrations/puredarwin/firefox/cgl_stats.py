# SPDX-License-Identifier: MIT
"""Read cache counters from the parent's already-observed private CGL image."""
import json


def inspect(client, providers, output):
    parent = next(row for row in providers['processes'] if row['role'] == 'parent')
    paths = [line.rsplit('> ', 1)[-1] for line in parent['images'] if ' OpenGL ' in line]
    if len(paths) != 1 or not paths[0].startswith('/private/var/db/sffx/'):
        raise RuntimeError('Expected one already-loaded private CGL image')
    client.command('Marionette:SetContext', {'value': 'chrome'})
    try:
        result = client.command('WebDriver:ExecuteScript', dict(script='''const {ctypes}=
          ChromeUtils.importESModule("resource://gre/modules/ctypes.sys.mjs");
          const lib=ctypes.open(arguments[0]);
          try { const values=ctypes.uint64_t.array(3)();
            const get=lib.declare("CGLSFFXExportCacheStats",ctypes.default_abi,
              ctypes.void_t,ctypes.uint64_t.ptr);
            get(values);return {hits:Number(values[0].toString()),
              misses:Number(values[1].toString()),evictions:Number(values[2].toString())};
          } finally {lib.close();}''', args=[paths[0]], newSandbox=False))
    finally:
        client.command('Marionette:SetContext', {'value': 'content'})
    result.update(provider=paths[0], complete=result['hits'] > 0)
    output.write_text(json.dumps(result, indent=2)+'\n')
    return result

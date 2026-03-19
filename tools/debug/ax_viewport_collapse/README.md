# ax_viewport_collapse Debug Tools

Manual verification tools for the `ax_viewport_collapse` CefBrowserSettings feature.
These connect to a running cefclient via CDP to inspect the accessibility tree.

## Prerequisites

```
python3 -m pip install websocket-client
```

## Usage

### 1. Launch cefclient with remote debugging

```
cefclient.exe --remote-debugging-port=9222 --remote-allow-origins=* --ax-viewport-collapse
```

The `--ax-viewport-collapse` flag maps to `CefBrowserSettings.ax_viewport_collapse = STATE_ENABLED` in cefclient.

To load the test page directly:

```
cefclient.exe --remote-debugging-port=9222 --remote-allow-origins=* --ax-viewport-collapse --url=file:///<path>/test_viewport_collapse.html
```

### 2. Run the verification script

```
python3 verify_viewport_collapse.py
```

Or navigate to a URL first:

```
python3 verify_viewport_collapse.py --url https://example.com
```

### Options

| Flag | Description |
|------|-------------|
| `--host` | CDP host (default: `127.0.0.1`) |
| `--port` | CDP port (default: `9222`) |
| `--url` | Navigate to URL before fetching tree |
| `--raw` | Print full JSON response instead of summary |

### Comparing with/without collapse

Run two cefclient instances on different ports:

```
cefclient.exe --cache-path=<path1> --remote-debugging-port=9222 --remote-allow-origins=* --ax-viewport-collapse
cefclient.exe --cache-path=<path2> --remote-debugging-port=9223 --remote-allow-origins=*
```

```
python3 verify_viewport_collapse.py --port 9222 --raw > with.json
python3 verify_viewport_collapse.py --port 9223 --raw > without.json
diff with.json without.json
```

### Expected output (with collapse enabled)

```
AX Tree Summary
============================================================
Total nodes returned:  77
  Ignored:             3
  Full (with children):70
  Summary (collapsed): 4

Collapsed summary nodes (off-screen landmarks/headings):
------------------------------------------------------------
  region: "Content preferences"
  region: "Audio quality"
  region: "Display"
  region: "Storage"
```

Off-screen landmark sections appear as leaf nodes with role + name only. In-viewport nodes are fully serialized with children.

## Files

- `verify_viewport_collapse.py` — CDP script that calls `Accessibility.getFullAXTree()` and prints a summary or raw JSON
- `test_viewport_collapse.html` — Test page with 6 landmark sections, 2 in viewport and 4 pushed off-screen by a spacer

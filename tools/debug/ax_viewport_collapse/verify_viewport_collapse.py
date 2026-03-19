#!/usr/bin/env python3
"""
Verify --ax-viewport-collapse behavior via raw CDP.

Usage:
  1. Launch cefclient with:
     out\\Debug_GN_x64\\cefclient.exe --remote-debugging-port=9222 --ax-viewport-collapse

  2. Navigate to a page with scrollable content, e.g. the test page:
     out\\Debug_GN_x64\\cefclient.exe --remote-debugging-port=9222 --ax-viewport-collapse \\
       --url=file:///path/to/cef/tools/debug/ax_viewport_collapse/test_viewport_collapse.html

  3. Run this script:
     python cef/tools/debug/ax_viewport_collapse/verify_viewport_collapse.py [--port 9222]

  The script connects via CDP, optionally navigates, calls Accessibility.getFullAXTree(),
  and prints a summary showing which nodes were serialized as full, summary, or absent.
"""

import argparse
import json
import sys
import http.client
import websocket  # pip install websocket-client


def get_ws_url(host, port):
  """Get the first available CDP target's websocket URL."""
  conn = http.client.HTTPConnection(host, port)
  conn.request("GET", "/json")
  resp = conn.getresponse()
  targets = json.loads(resp.read())
  conn.close()
  for t in targets:
    if t.get("type") == "page" and "webSocketDebuggerUrl" in t:
      return t["webSocketDebuggerUrl"]
  raise RuntimeError(
      f"No page target found on {host}:{port}. Targets: {json.dumps(targets, indent=2)}"
  )


def cdp_send(ws, method, params=None, id_counter=[0]):
  id_counter[0] += 1
  msg = {"id": id_counter[0], "method": method, "params": params or {}}
  ws.send(json.dumps(msg))
  while True:
    resp = json.loads(ws.recv())
    if resp.get("id") == id_counter[0]:
      if "error" in resp:
        raise RuntimeError(f"CDP error: {resp['error']}")
      return resp.get("result", {})


def navigate_and_wait(ws, url):
  """Navigate to URL and wait for load."""
  cdp_send(ws, "Page.enable")
  cdp_send(ws, "Page.navigate", {"url": url})
  # Wait for loadEventFired
  while True:
    resp = json.loads(ws.recv())
    if resp.get("method") == "Page.loadEventFired":
      break


LANDMARK_ROLES = {
    "banner", "complementary", "contentinfo", "form", "main", "navigation",
    "region", "search"
}
HEADING_ROLES = {"heading"}
INTERACTIVE_ROLES = {
    "button", "checkbox", "link", "textbox", "combobox", "listbox", "menuitem",
    "radio", "slider", "switch", "tab", "searchbox", "spinbutton", "tree"
}


def classify_role(role_value):
  """Classify a CDP role string."""
  r = (role_value or "").lower()
  if r in LANDMARK_ROLES:
    return "landmark"
  if r in HEADING_ROLES:
    return "heading"
  if r in INTERACTIVE_ROLES:
    return "interactive"
  return "structural"


def analyze_tree(nodes):
  """Analyze the AX tree and print a summary."""
  total = len(nodes)
  ignored = 0
  full_nodes = []
  summary_nodes = []  # Has role+name but empty children

  for node in nodes:
    if node.get("ignored"):
      ignored += 1
      continue

    child_ids = node.get("childIds", [])
    role_obj = node.get("role", {})
    role_value = role_obj.get("value", "") if isinstance(role_obj, dict) else ""
    name_obj = node.get("name", {})
    name_value = name_obj.get("value", "") if isinstance(name_obj, dict) else ""
    properties = node.get("properties", [])

    has_name = bool(name_value)
    has_children = len(child_ids) > 0
    has_full_props = len(properties) > 2  # More than just level

    cat = classify_role(role_value)

    if cat in ("landmark", "heading") and has_name and not has_children:
      summary_nodes.append(node)
    else:
      full_nodes.append(node)

  print(f"\n{'='*60}")
  print(f"AX Tree Summary")
  print(f"{'='*60}")
  print(f"Total nodes returned:  {total}")
  print(f"  Ignored:             {ignored}")
  print(f"  Full (with children):{len(full_nodes)}")
  print(f"  Summary (collapsed): {len(summary_nodes)}")
  print()

  if summary_nodes:
    print("Collapsed summary nodes (off-screen landmarks/headings):")
    print("-" * 60)
    for node in summary_nodes:
      role_obj = node.get("role", {})
      role_val = role_obj.get("value", "?") if isinstance(role_obj,
                                                          dict) else "?"
      name_obj = node.get("name", {})
      name_val = name_obj.get("value", "") if isinstance(name_obj, dict) else ""
      props = node.get("properties", [])
      level_str = ""
      for p in props:
        if isinstance(p, dict) and p.get("name") == "level":
          lv = p.get("value", {})
          level_str = f" [level={lv.get('value', '?')}]"
      print(f"  {role_val}: \"{name_val}\"{level_str}")
    print()

  # Print the tree structure (first 50 non-ignored nodes)
  print("First 50 non-ignored nodes:")
  print("-" * 60)
  node_map = {n.get("nodeId"): n for n in nodes}
  count = 0
  for node in nodes:
    if node.get("ignored"):
      continue
    if count >= 50:
      print("  ... (truncated)")
      break
    count += 1

    role_obj = node.get("role", {})
    role_val = role_obj.get("value", "?") if isinstance(role_obj, dict) else "?"
    name_obj = node.get("name", {})
    name_val = name_obj.get("value", "") if isinstance(name_obj, dict) else ""
    child_ids = node.get("childIds", [])

    name_str = f" \"{name_val}\"" if name_val else ""
    children_str = f" [{len(child_ids)} children]" if child_ids else " [leaf]"
    is_summary = node in summary_nodes
    tag = " <-- COLLAPSED" if is_summary else ""

    print(f"  {role_val}{name_str}{children_str}{tag}")

  print()


def main():
  parser = argparse.ArgumentParser(
      description="Verify CDP accessibility viewport collapse")
  parser.add_argument("--host", default="127.0.0.1")
  parser.add_argument("--port", type=int, default=9222)
  parser.add_argument("--url", help="URL to navigate to before fetching tree")
  parser.add_argument(
      "--raw", action="store_true", help="Print raw JSON response")
  args = parser.parse_args()

  print(f"Connecting to CDP at {args.host}:{args.port}...")
  ws_url = get_ws_url(args.host, args.port)
  print(f"WebSocket: {ws_url}")

  ws = websocket.create_connection(ws_url)

  if args.url:
    print(f"Navigating to {args.url}...")
    navigate_and_wait(ws, args.url)
    print("Page loaded.")

  print("Enabling accessibility domain...")
  cdp_send(ws, "Accessibility.enable")

  print("Fetching full AX tree...")
  result = cdp_send(ws, "Accessibility.getFullAXTree")
  nodes = result.get("nodes", [])

  if args.raw:
    print(json.dumps(nodes, indent=2))
  else:
    analyze_tree(nodes)

  ws.close()


if __name__ == "__main__":
  main()

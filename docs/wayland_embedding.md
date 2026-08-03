This page describes the design for embedding CEF browser windows inside Wayland windows owned by a client application, and tracks the state of the implementation on this branch.

Upstream issue: [#2804 — Linux: Add support for embedded Ozone/Wayland windows](https://github.com/chromiumembedded/cef/issues/2804)

**Contents**

- [Problem](#problem)
- [Why X11 embedding does not translate](#why-x11-embedding-does-not-translate)
- [Design](#design)
- [Public API](#public-api)
- [Connection adoption](#connection-adoption)
- [The embedded window](#the-embedded-window)
- [Input](#input)
- [Popups](#popups)
- [What cannot be preserved](#what-cannot-be-preserved)
- [Implementation status](#implementation-status)

---

# Problem

CEF has three ways to host web content:

| Mode | Entry point | Wayland |
|---|---|---|
| Views | `CefBrowserView` + `CefWindow` | works |
| Windowless | `CefWindowInfo::SetAsWindowless` | works |
| Native windowed | `CefWindowInfo::SetAsChild` | **missing** |

Native windowed is the mode where the client application owns a native window and CEF creates a child window inside it. It is what OBS uses for browser docks in its own UI and, most likely, what the Steam client uses. On Linux it is implemented entirely by `CefWindowX11`, and there is no Wayland counterpart.

Before this branch, `CreateHostWindow()` selected that implementation on `BUILDFLAG(SUPPORTS_OZONE_X11)` alone. Because release binaries compile in both Ozone platforms, the X11 path also ran under Ozone/Wayland and produced either a stray XWayland window or nothing at all, followed by a crash in `WaylandWindowManager::GetWindow()` on the first event dispatch. That is fixed separately; this document is about supplying the missing mode.

# Why X11 embedding does not translate

An X11 `Window` is a 32-bit id allocated by the X server. It is global: any client, on any connection, in any process, can reference another client's window given only the number. `CefWindowX11` relies on this. It opens its own X connection through `x11::Connection::Get()` and still parents its window inside the client application's window, because `parent` in `x11::Connection::CreateWindow()` is just an integer.

A `wl_surface` is a protocol object bound to one `wl_display` connection. It is not an integer, not serializable, and does not cross a connection boundary — not even between two `wl_display`s inside the same process. `wl_subcompositor_get_subsurface()` requires the parent and child surfaces to belong to the same connection.

Everything below follows from that one fact.

# Design

## Public API

The parent surface is not a new field. `cef_window_info_t::parent_window` has always been an opaque native parent handle whose meaning depends on the platform — it is an `HWND` on Windows and an `NSView*` on macOS — so under Ozone/Wayland it names a `wl_surface`. The `unsigned long` spelling on Linux is an artifact of X11, and it holds a pointer on LP64. Existing embedders keep compiling and keep working.

The cost of that choice is the failure mode: an X11 Window passed while Wayland is active is dereferenced as a pointer. A valid XID such as `0x2600004` is numerically indistinguishable from an address, so this cannot be diagnosed, only documented. In practice the handle comes from the client's toolkit, which knows which platform it is on.

One member is appended to `cef_window_info_t`, guarded by `CEF_API_ADDED(CEF_EXPERIMENTAL)` so existing stable API versions keep their ABI:

```c
cef_xdg_surface_handle_t parent_xdg_surface;  // in, for popups
```

with a three-argument `CefWindowInfo::SetAsChild(parent, parent_xdg, bounds)` overload alongside the existing two-argument form. It may be null: toolkits differ in whether they expose the `xdg_surface` at all — winit does not — and refusing to create the browser would put embedding out of reach for those hosts. Without it the browser renders and takes input, and popups fall back to a degraded `wl_subsurface` form described under [Popups](#popups).

Appending is only half of ABI safety. `CefWindowInfoTraits::set()` copies the new member under `CEF_MEMBER_EXISTS`, because `size` is what distinguishes a smaller struct from a larger one at runtime; the `#if` alone is a compile-time test and would read past the end of an older client's allocation.

An earlier revision also returned the browser's own `wl_surface` as an output member. It was removed: a `wl_surface` is scoped to the connection that created it, which the embedder already owns, so the handle told the embedder nothing it could act on, while costing struct size and public API surface. What comes back in `cef_window_info_t::window` is a `gfx::AcceleratedWidget` — an id internal to the process, not a protocol object, unlike the X11 `Window` that field carries under Ozone/X11.

**The Ozone platform has to be selected explicitly.** Ozone picks X11 wherever `DISPLAY` is set, which under a Wayland session means XWayland, and then `parent_window` — a `wl_surface*` — is read as an X11 `Window`. That is the undefined-rather-than-diagnosed failure described above, reached without the embedder doing anything wrong. Pass `--ozone-platform=wayland` on the command line CEF receives rather than through `CefSettings`: subprocesses inherit `argv`, and they have to agree with the browser process about the platform.

`CefBrowserHost::SetWindowBounds()` is declared last in the class on purpose. The translator moves versioned members to the end of the generated C struct in declaration order, so putting it anywhere earlier would shift the existing experimental `set_ax_viewport_collapse` and silently break callers compiled against it.

The connection is *not* passed per browser. Chromium builds its `WaylandConnection` while initializing Ozone, which happens inside `CefInitialize`, long before any `CefWindowInfo` exists. Adoption is therefore a process-global, one-time decision, exposed separately:

```c
CEF_EXPORT void cef_set_wayland_display(cef_wayland_display_handle_t display);
CEF_EXPORT cef_wayland_display_handle_t cef_get_wayland_display(void);
```

`cef_set_wayland_display()` must be called before `CefInitialize`. The 2020 Igalia prototype put a `wayland_display_handle` on `CefWindowInfo`; that cannot work, because by the time a `CefWindowInfo` is available the connection has already been made.

The client passes its own connection because CEF has to join it. This is the inverse of the X11 contract, where CEF opens its own connection and the client passes only a window id.

## Connection adoption

`ui::WaylandConnection::Initialize()` calls `wl_display_connect()` unconditionally. It needs to accept an externally owned `wl_display*` instead.

Two event loops then share one connection, which libwayland does not support naively: whichever side calls `wl_display_dispatch()` first consumes the other side's events. The supported mechanism is:

* create a dedicated `wl_event_queue` for Chromium via `wl_display_create_queue()`;
* wrap every proxy Chromium creates with `wl_proxy_create_wrapper()` and assign it to that queue with `wl_proxy_set_queue()`, so that events for Chromium's objects are delivered to Chromium's queue;
* dispatch with `wl_display_dispatch_queue()` instead of `wl_display_dispatch()`;
* never call `wl_display_disconnect()` on an adopted connection.

Ownership rules for the adopted case:

* the client owns the connection and must outlive every browser using it;
* CEF must not roundtrip on the default queue, because the client may be blocked in its own dispatch;
* `wl_display_flush()` is shared and safe.

## The embedded window

`gfx::AcceleratedWidget` on Ozone/Wayland is not a `wl_surface`. It is an opaque id that `WaylandWindowManager` maps to a `WaylandWindow`. Any code that receives a widget — screen scale lookup, event dispatch, occlusion — goes through that map. So the embedded window cannot be synthesized from a foreign handle; it has to be a real `WaylandWindow` that registers itself.

The new window type:

* is created from `PlatformWindowInitProperties` carrying the client's parent `wl_surface`;
* creates its own `wl_surface` and turns it into a subsurface of the parent via `wl_subcompositor_get_subsurface()`;
* positions itself with `wl_subsurface_set_position()` using the requested bounds, relative to the parent surface origin;
* uses desynchronized mode (`wl_subsurface_set_desync()`) so that browser frames are not gated on the client committing its parent surface;
* registers with `WaylandWindowManager` like any other `WaylandWindow`, which is what makes widget lookup work and is precisely what was crashing before.

There is no CEF-side counterpart of `CefWindowX11`, and there does not need to be. `CefWindowX11` exists because `DesktopWindowTreeHostLinux` cannot be reparented into a foreign X11 window, so CEF interposes a window of its own between the two. On Wayland the embedded window *is* the `DesktopWindowTreeHostLinux`'s platform window, so CEF only has to name the parent surface and let views build the rest.

Naming it is the trick. views has no vocabulary for a native parent other than `gfx::AcceleratedWidget`, and `DesktopWindowTreeHostLinux` already forwards `views::Widget::InitParams::parent_widget` into `PlatformWindowInitProperties`. Registering the client's `wl_surface` with `WaylandWindowManager` yields such a widget, so nothing in views changes.

One trap: `WaylandWindow::Create()` cannot decide this from `PlatformWindowType`. views maps every `Widget::InitParams::Type` it does not recognise to `kPopup`, and `TYPE_CONTROL` -- which is what an embedded browser uses -- is one of those. The foreign-parent check therefore runs before the type switch.

## Input

Pointer input works: `wl_pointer.enter` is delivered per surface, and a subsurface is a surface, so it receives pointer events normally.

Keyboard is more interesting. `wl_keyboard.enter` is delivered only to the surface that holds the seat's keyboard focus, and a `wl_subsurface` never holds it — the client's toplevel does. There is no protocol mechanism for a subsurface to take keyboard focus.

Issue #2804 concluded from this that every embedder would have to intercept `wl_keyboard` and replay events through `CefBrowserHost::SendKeyEvent`. That turns out not to be necessary. Chromium is already on the client's connection and already receives those events; it was discarding them because `RootWindowFromWlSurface()` could not resolve the client's toplevel to any window it knew. Teaching that lookup about registered foreign surfaces routes keyboard focus to the embedded window, and typing works with no embedder code at all.

The same lookup closes a memory-safety hole. It reads a `wl_surface`'s user data and casts it to `ui::WaylandSurface`; on a surface owned by the embedder that pointer belongs to their toolkit. Nothing crashed only because the sample never sets user data — GTK and Qt both do.

Splitting the lookup is also what makes drag and drop attribute correctly, which was not obvious in advance. The embedder and Chromium each create a `wl_data_device` from the same `wl_seat` on the same connection, and both receive `wl_data_device.enter` for a drag anywhere over the window; nothing in the protocol arbitrates between them. What decides is the `wl_surface` the event carries. Because `RootWindowFromWlSurface()` resolves only Chromium's own surfaces and returns null for the embedder's, Chromium ignores drags over the host's chrome and claims the ones over the browser's subsurface, while the host does the reverse. Had the keyboard fallback been left in the shared lookup, a drag over the host's own UI would have resolved to the embedded browser and been delivered twice.

Text input goes the same way, and is routed the same way: `zwp_text_input_v3` is per seat and follows the focused surface, so its enter/leave name the embedder's toplevel and resolve through the same lookup as the keyboard.

What that does *not* buy is correctness across a focus change. Those protocol events fire once and are not repeated when the host moves between two browsers inside one window, so `Activate()` has to move the recorded text-input focus itself, alongside the keyboard focus. And routing alone is not enough either: `Activate()` must also reach `PlatformWindowDelegate::OnActivationChanged()`, because the composition state lives on the views side and `InputMethod::OnFocus`/`OnBlur` are delivered from there. An earlier revision of this branch moved the Ozone-level focus without notifying the delegate, which would have left the IME attached to whichever browser was focused first.

Verified end to end with ibus and Intelligent Pinyin: preedit renders underlined inside the field, the candidate window is placed next to the caret rather than at the surface origin, and the commit lands in the input. The candidate window placement was the part expected to break, since it is positioned from `set_cursor_rectangle` coordinates and the window they describe is a subsurface; it turns out correct, because those coordinates are computed against the same window whose bounds the embedder supplies.

Dead keys are not evidence for any of this. xkbcommon composes them inside Chromium from the keymap, with `zwp_text_input_v3` uninvolved, so they work whether or not text input is routed at all.

## Popups

`xdg_popup` requires an `xdg_surface` as its parent. A `wl_subsurface` has no `xdg_surface` role, so context menus, `<select>` dropdowns, tooltips, autocomplete and date pickers have nothing to anchor to.

There is no protocol that fixes this, and it is worth being precise about why, because two mechanisms look like they might. `xdg_surface.get_popup` does accept a null parent — but only "if a parent surface is specified using some other protocol", and that protocol is `xdg_foreign`, whose `zxdg_imported_v2.set_parent_of` rejects anything that is not "an xdg_toplevel equivalent". Neither reaches a subsurface.

Two options remain, both lossy:

**(a) Anchor on the client's toplevel.** The client passes its `xdg_surface` alongside the parent `wl_surface`. CEF creates popups as children of it and measures anchor rectangles in that surface's coordinates. Positioning depends on the client keeping CEF's idea of the embedded bounds current through `CefBrowserHost::SetWindowBounds()`; a stale value puts menus in the wrong place.

**(b) Emulate popups with more subsurfaces.** Loses pointer grab, loses click-outside dismissal, loses `xdg_positioner` edge flipping, and clips the popup to the client window bounds.

This branch implements (a) wherever it can, because (b) is not really a menu. What makes (a) workable is that the embedder and CEF share one connection, so the embedder's `xdg_surface` is simply an object that can be handed over.

(b) is nonetheless implemented too, as the fallback for an embedder that has no `xdg_surface` to hand over — winit is one. The alternative would be to fail, and failing is not available: `WaylandWindow::Create()` returning null makes views destroy the widget inside `Widget::Show()`, and `Widget::HandleWidgetDestroyed()` `CHECK`s on exactly that. The choice is not between a good menu and no menu; it is between a degraded menu and an aborted process.

The degradation is real and worth stating plainly: a dropdown near the bottom edge of the host window is clipped rather than flipped, and clicking outside it does not dismiss it. `cef_window_info_t::parent_xdg_surface` documents this, and both CEF and Ozone log a warning naming the limitation when the fallback is taken. An embedder whose toolkit exposes the `xdg_surface` should always pass it.

# What cannot be preserved

`CefWindowX11` carries thirteen responsibilities. Seven have no Wayland equivalent:

| X11 | Wayland | Disposition |
|---|---|---|
| parent at creation | `wl_subcompositor_get_subsurface` | works, same connection required |
| `WM_CLIENT_MACHINE`, `_NET_WM_PID`, title | — | gone, no functional loss |
| size hints + map | subsurface + parent commit | works |
| `XdndProxy` | `wl_data_device` | works; the drag is attributed by the `wl_surface` the enter event carries, so no proxying is needed |
| `Focus()` | — | blocked by protocol, becomes client's job |
| `Close()` via `WM_DELETE_WINDOW` | — | becomes client's job |
| `SetBounds()` | `wl_subsurface_set_position` | works |
| `GetBoundsInScreen()` | — | impossible by design, no global coordinates |
| `ConfigureNotify` | `xdg_toplevel.configure` on the client | becomes client's job |
| `_NET_WM_PING` | — | gone |
| `FocusIn`/`FocusOut` | — | blocked by protocol |
| `_NET_WM_STATE` min/max | `xdg_toplevel` states on the client | becomes client's job |
| `TopLevelAlwaysOnTop()` | — | not queryable |

The pattern: on X11 CEF observes the window system passively. On Wayland the client has to push that state in. That is why this cannot be a drop-in replacement, and why embedders will need code changes even after it lands.

# Implementation status

| # | Item | Where | State |
|---|---|---|---|
| 1 | Runtime Ozone platform selection; fail cleanly instead of crashing | CEF | **done** |
| 2 | Keyboard translation stops using X11 keysyms under other platforms | CEF | **done** |
| 3 | `parent_window` carries a `wl_surface`; `parent_xdg_surface`, `cef_set_wayland_display` | CEF | **done** |
| 4 | `WaylandConnection` adopts an external `wl_display` | Chromium patch | **done** |
| 5 | `WaylandEmbeddedWindow` + foreign-surface registry | Chromium patch | **done** |
| 6 | `CreateHostWindow()`/`CloseHostWindow()` build and tear down the embedded window | CEF | **done** |
| 7 | Popups anchored on the embedder's `xdg_surface` | Chromium patch | **done** |
| 8 | Keyboard focus routed to the embedded window | Chromium patch | **done** |
| 9 | `CefBrowserHost::SetWindowBounds()` for host-driven geometry | CEF | **done** |
| 10 | Sample exercising all of it (`tests/cefembed_wayland`) | CEF | **done** |
| 11 | IME: `zwp_text_input_v3` routed and focus moved with the browser | Chromium patch | **done** |
| 12 | Drag and drop into and out of the embedded surface | Chromium patch | **done** |
| 13 | HiDPI and fractional scale | Chromium patch + embedder | **done** |
| 14 | `cefclient` gains a Wayland windowing path (today it is GTK over X11) | CEF | not started |

Items 4, 5, 7, 8 and 11 live in `patch/patches/ozone_wayland_embed_2804.patch` rather than in CEF source, because they are changes to Chromium. That patch is why this cannot be done in CEF alone.

Item 14 is the only one left, and it is not about embedding: `cefclient`'s Linux windowing path is GTK over X11 and hands CEF an XID (`GDK_WINDOW_XID` in `root_window_gtk.cc` and `browser_window_std_gtk.cc`, plus `temp_window_x11.cc`), so there is nothing there to pass a `wl_surface` through. That is why the sample in `tests/cefembed_wayland` exists at all.

Item 13 is listed against the embedder as well as the patch, because half of it is a contract rather than code. Ozone tracks *changes* on its own: the embedded window's surface receives `wl_surface.enter`/`leave` and binds `wp_fractional_scale_v1` like any other, so `WaylandWindow::UpdateWindowScale()` follows a move between outputs and the browser renders sharp at the new scale without the embedder doing anything.

The initial value is the part that needed work, because there is no change to observe. A window with a parent inherits the parent's scale; `WaylandWindow::Initialize()` takes `state->window_scale` as final and does not call `UpdateWindowScale()` afterwards. An embedded window has no parent, so a browser created while the host already sits on a fractional output would render at the primary display's scale and stay blurry until dragged to another output and back -- only then does an event arrive. It seeds itself from a sibling already embedded in the same surface instead, which is on the same output by construction; the primary display remains the fallback when there is no sibling.

What the embedder does have to get right is the units it passes to `CefBrowserHost::SetWindowBounds()`. Those are DIP, because they end up in `wl_subsurface.set_position`, which is surface-local. A client whose own layout is in device pixels must divide by its scale factor first. The two are equal at scale 1, so an embedder that gets this wrong sees nothing until the window meets a fractional-scale output, at which point the browser is displaced by the scale factor and can leave the screen entirely.

## Verification status

Run on Fedora 44, GNOME 49 Wayland, against a local build of this branch (Chromium 151.0.7922.0), driving `tests/cefembed_wayland` by hand.

| Claim | Status |
|---|---|
| Ozone/Wayland compiles with the patch | yes |
| API additions break no existing API hash | yes — `2 hashes checked and match`, `116 patches total (0 failed)` |
| The client's connection is adopted | yes — Chromium's surface takes protocol id 50 on the connection whose host surface is id 3; a separate connection would have restarted at 1 |
| Web content renders inside the client's window | yes |
| Pointer input reaches the browser | yes |
| `<select>` dropdowns open, correctly positioned | yes |
| Context menus open | yes |
| Keyboard input reaches the browser | yes, with no embedder involvement |
| Resizing tracks the host's layout | yes, driven by `CefBrowserHost::SetWindowBounds()` |
| The X11 path still works | yes, unchanged behaviour with the same binary |
| Closing the browser completes | yes |
| IME / composition | yes — ibus with Intelligent Pinyin: underlined preedit inside the field, candidate window correctly placed next to the caret, commit lands in the input. Identical in the embedded, X11 and Views paths |
| Drag and drop | yes — files dropped into the browser, and text and files dragged out of it to other applications, with drops over the host's own chrome still going to the host |
| HiDPI / fractional scale | yes — starting on either kind of output and dragging between a 1x and a fractional-scale one, in both directions: correct position, sharp rendering. Scale changes while a popup is open remain untested |
| Multiple embedded browsers in one window | yes — created, moved, hidden, closed independently, and keyboard focus follows both `SetFocus()` and a click |

Known rough edge: during an interactive resize the browser's contents lag the window frame by a frame or so. `wl_subsurface.set_position` is double-buffered on the *parent* surface, so a move only lands when the embedder commits, and the two surfaces are submitted in separate compositor frames with no atomicity between them. `WaylandBubble` carries the same TODO upstream (crbug.com/329145822). Fixing it needs explicit synchronisation, not a change here.

### ANGLE-Wayland is not involved

Worth recording because the assumption is widespread: this works without the ANGLE Wayland backend. The ANGLE in this checkout has only `renderer/vulkan/linux/wayland/WindowSurfaceVkWayland`, the Vulkan path that predates it; `renderer/gl/egl/` contains no Wayland backend at all.

In the default multi-process configuration the GPU process has no `WaylandConnection`. It renders into GBM/dmabuf and the browser process presents through `zwp_linux_dmabuf_v1`, which is what the connection actually binds. `EGL_PLATFORM_WAYLAND_EXT` is never reached. The ANGLE Wayland backend only matters for `--single-process`/`--in-process-gpu` or embedded targets with no DRM render node.

### Traps found while bringing this up

Recorded because each cost real time and none is obvious from the code.

* **`WAYLAND_DEBUG=1` shows none of Chromium's requests.** The client and libcef each link their own static copy of libwayland, and the debug flag is initialised inside `wl_display_connect()`. Only the client calls it. Use logging inside Ozone instead.
* **`Widget::InitParams::TYPE_CONTROL` becomes `PlatformWindowType::kPopup`.** views maps every type it does not recognise to `kPopup`, so an embedded browser never arrives as `kWindow`. Selecting the embedded window type from `properties.type` silently produces a separate toplevel instead.
* **A wl_subsurface has no close protocol.** `CloseHostWindow()` must close the `views::Widget` directly. Waiting for a window-manager close, the way the X11 path does, hangs `CloseBrowser()` forever and leaves a live browser for `CefShutdown()` to destroy the connection underneath.
* **GNOME does not advertise `zxdg_decoration_manager_v1`.** An embedder gets no titlebar, no close button and no resize grips unless it draws and drives them itself, and no cursor at all unless it sets one.
* **Partial `wl_pointer_listener` initialisers abort the process.** libwayland calls every slot the negotiated version implies.
* **A client that configures a Stable API version cannot see any of this.** `cef_api_hash()` decides which struct layout libcef publishes. Under 15101 `cef_browser_host_t` ends at `get_runtime_style` and every `CEF_API_ADDED(CEF_EXPERIMENTAL)` member is absent, so `SetWindowBounds()` is simply not there -- 584 bytes published against the 600 a caller expecting it would compute. That is the versioning system working correctly, but it means an embedder must opt into the experimental API to use any of this before it is assigned a version.

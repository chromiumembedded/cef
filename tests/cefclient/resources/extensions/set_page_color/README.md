# Color Extension

Demonstrates basic extension app loading and integration by using a popup to change the page color.

## Usage

Run `cefclient --load-extension=set_page_color`.

When using the Views framework (`--use-views`) an extension icon will be added to the control bar and clicking the icon will open the extension window. When not using the Views framework an extension window will be opened automatically on application start.

## Implementation

Based on the [set_page_color](https://developer.chrome.com/extensions/samples#search:browser%20action%20with%20a%20popup) example extension.

Calls:

 * [tabs.executeScript](https://developer.chrome.com/extensions/tabs#method-executeScript)

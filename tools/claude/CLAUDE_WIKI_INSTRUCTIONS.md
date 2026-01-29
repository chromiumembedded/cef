# Bitbucket Wiki Formatting Instructions for Claude Agents

This document provides formatting guidelines for Claude agents when creating or editing CEF wiki pages hosted on Bitbucket.

## Important: Bitbucket-Specific Markdown Rendering

Bitbucket's wiki renderer has specific requirements that differ from standard Markdown. Following these rules is essential for proper rendering.

## Automated Formatting Tool

**Before manually applying formatting rules**, use the automated formatter to fix common issues.

**Option 1: Using fix_style.py (recommended):**

```bash
# From chromium/src/cef/tools directory
python3 fix_style.py claude/CLAUDE_*.md
python3 fix_style.py claude/specific_file.md
```

**Option 2: Using format_wiki.py directly:**

```bash
# Check if file needs formatting
python3 cef/tools/claude/format_wiki.py filename.md --check

# Apply formatting automatically
python3 cef/tools/claude/format_wiki.py filename.md --in-place

# Preview changes (output to stdout)
python3 cef/tools/claude/format_wiki.py filename.md
```

**What the tool fixes:**

- Adds blank lines before lists after paragraphs/subsections
- Fixes list indentation to 4-space multiples
- Fixes code fence indentation in lists
- Preserves content inside code blocks

**What the tool does NOT fix:**

- Wiki page links (Rule 3) - must be checked manually
- Content-level issues (spelling, grammar, accuracy)
- Section header hierarchy

**When to use:**

- After creating or editing any wiki page markdown file
- Before final manual review and verification
- To quickly identify formatting issues

## Core Formatting Rules

### 1. Newlines Before Lists

**Rule:** Always include a blank line before starting a list after paragraph text.

**Why:** Without the blank line, Bitbucket won't render the list correctly - it will appear as plain text.

**Applies to:**

- Transitioning from paragraph text to a list
- Indented paragraphs followed by indented lists
- After subsection headers (like `**Subsection:**`)

**Does NOT apply to:**

- Sub-list items directly under a parent list item (adding a blank line breaks nesting)

**Examples:**

```markdown
✅ CORRECT - Top-level list after paragraph:
Some paragraph text.

- List item  ← blank line required above
- Another item

✅ CORRECT - Nested list with indented paragraph:
1. Parent item
    Some indented paragraph.

    - Nested list item  ← blank line required (after paragraph)
    - Another item

✅ CORRECT - Sub-items directly under parent:
1. Parent item
    - Sub-item  ← NO blank line (directly under parent)
    - Another sub-item

❌ INCORRECT - Missing blank line:
Some paragraph text.
- List item  ← will not render as a list
- Another item

❌ INCORRECT - Blank line breaks nesting:
1. Parent item

    - Sub-item  ← blank line above breaks nesting
```

### 2. List Indentation

**Rule:** Use **4 spaces** for sub-list items, not 2 or 3.

**Why:** Bitbucket's wiki renderer requires exactly 4 spaces for proper nesting.

**Applies to:**

- Both numbered and bulleted sub-items
- All levels of nesting (4 spaces, 8 spaces, 12 spaces, etc.)

**Does NOT apply to:**

- Code blocks within markdown documents (use standard 2-space C++ indentation)

**Examples:**

```markdown
✅ CORRECT - 4-space indentation:
1. Main item
    - Sub-item (4 spaces)
    - Another sub-item
        - Nested sub-item (8 spaces)

❌ INCORRECT - 2-space indentation:
1. Main item
  - Sub-item (2 spaces - won't nest properly)
  - Another sub-item
```

### 3. Wiki Page Links

**Rule:** Use full Bitbucket wiki URLs, not relative paths.

**Why:** Relative paths may not work correctly in all contexts.

**Format:**

- Page link: `[LinkText](https://bitbucket.org/chromiumembedded/cef/wiki/PageName)`
- Section anchor: `[LinkText](https://bitbucket.org/chromiumembedded/cef/wiki/PageName#markdown-header-section-name)`

**Anchor format:**

- Prefix: `#markdown-header-`
- Section name: lowercase with hyphens instead of spaces
- Example: `#markdown-header-inter-process-communication-ipc`

**Examples:**

```markdown
✅ CORRECT - Full URL:
See [Tutorial](https://github.com/chromiumembedded/cef/blob/master/docs/tutorial.md) for details.
See [GeneralUsage IPC section](https://github.com/chromiumembedded/cef/blob/master/docs/general_usage.md#inter-process-communication-ipc).

❌ INCORRECT - Relative path:
See [Tutorial](Tutorial.md) for details.
See [GeneralUsage IPC section](GeneralUsage.md#inter-process-communication-ipc).
```

### 4. Code Block Indentation

**Rule:** Code blocks within markdown should use appropriate language-specific indentation (e.g., 2 spaces for C++).

**Why:** Wiki formatting (4-space list indentation) is separate from code formatting.

**Example:**

```markdown
✅ CORRECT - Separate formatting concerns:
1. Step one (4 spaces for list nesting)
    - Description text

    ```cpp
    // C++ code uses 2-space indentation
    void Function() {
      if (condition) {
        DoSomething();
      }
    }
    ```
```

## Section Header Hierarchy

Use consistent header levels for wiki pages:

```markdown
# Page Title (h1)

## Major Section (h2)

### Subsection (h3)

#### Detail Section (h4)
```

For inline subsection headers within text, use bold:

```markdown
**Subsection name:**

Description text.
```

## Common Patterns

### Tutorial Steps

```markdown
# Step 1: Title

**Goal:** Brief description.

## What We're Adding

Description of new functionality.

## Modified Files

### path/to/file.h

Code example with explanation.

## Build and Run

Build instructions.

## What's Happening

Explanation of concepts.
```

### Lists with Code Examples

```markdown
**New concepts:**

1. **Concept name:**
    - Explanation point
    - Another point
    - Code example:
      ```cpp
      code_here();
      ```
```

## Verification Checklist

Before submitting wiki changes:

1. **Run the automated formatter:**

    ```bash
    # From chromium/src/cef/tools directory
    python3 fix_style.py claude/filename.md
    ```

2. **Manually verify:**
    - [ ] All wiki links use full Bitbucket URLs (tool does not check this)
    - [ ] Code blocks use language-appropriate indentation (not forced to 4 spaces)
    - [ ] Section headers follow consistent hierarchy
    - [ ] Content is accurate and complete

3. **Automated checks (already fixed by tool):**
    - [ ] Blank lines before all top-level lists (after paragraph text)
    - [ ] NO blank lines before sub-items directly under parent items
    - [ ] All sub-items use 4-space indentation

## User Testing Guidance

When you complete wiki edits, inform the user they should preview the changes in Bitbucket's wiki editor to verify:

- Lists render correctly (not as plain text)
- Nested lists show proper indentation
- Links navigate to the correct pages and sections
- Code blocks are properly formatted

You cannot preview wiki pages directly, so rely on following these formatting rules precisely.

## Additional Resources

- CEF Wiki Home: https://github.com/chromiumembedded/cef/blob/master/docs/README.md
- Bitbucket Wiki Markdown: https://confluence.atlassian.com/bitbucketserver/markdown-syntax-guide-776639995.html

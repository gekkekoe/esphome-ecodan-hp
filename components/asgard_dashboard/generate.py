import gzip
import os
import re
import subprocess
import tempfile

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

def _extract_js(html):
    """Extract JS from inline <script> tags (no src attribute)."""
    pattern = r'<script(?![^>]*\bsrc=)[^>]*>([\s\S]*?)</script>'
    return "\n;\n".join(re.findall(pattern, html))


def check_js_syntax(source_file):
    """Syntax-check dashboard JS with node --check so a JS syntax error
    (e.g. unbalanced braces) fails the build instead of serving a broken
    dashboard. Skipped (with a warning) if node is not installed."""
    path = os.path.join(SCRIPT_DIR, source_file)
    if not os.path.exists(path):
        print(f"Error: {path} not found!")
        return False
    with open(path, 'r', encoding='utf-8') as f:
        html = f.read()
    js = _extract_js(html)
    if not js.strip():
        return True
    try:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.js',
                                         delete=False) as tmp:
            tmp.write(js)
            tmp_path = tmp.name
        try:
            r = subprocess.run(["node", "--check", tmp_path],
                               capture_output=True, text=True)
        finally:
            os.unlink(tmp_path)
    except FileNotFoundError:
        print(f"Warning: node not found — skipping {source_file} syntax check")
        return True
    if r.returncode != 0:
        print(f"Dashboard JS syntax check FAILED ({source_file}):")
        print(r.stderr)
        return False
    return True


def generate_header(source_file, output_file, array_name):
    source_file = os.path.join(SCRIPT_DIR, source_file)
    output_file = os.path.join(SCRIPT_DIR, output_file)

    if not os.path.exists(source_file):
        print(f"Error: {source_file} not found!")
        return

    with open(source_file, 'r', encoding='utf-8') as f:
        content_str = f.read()

    content_str = re.sub(r'<!--[\s\S]*?-->', '', content_str)
    content_str = re.sub(r'/\*[\s\S]*?\*/', '', content_str)
    content_str = re.sub(r'^\s*//.*\n', '', content_str, flags=re.MULTILINE)

    content = content_str.encode('utf-8')

    compressed = gzip.compress(content, compresslevel=9)
    
    hex_array = []
    for i, byte in enumerate(compressed):
        hex_array.append(f"0x{byte:02x}")

    rows = []
    for i in range(0, len(hex_array), 16):
        rows.append("  " + ", ".join(hex_array[i:i+16]))

    rows_joined = ",\n".join(rows)

    header_content = f"""#pragma once
#include <stdint.h>
#include <stddef.h>

namespace esphome {{
namespace asgard_dashboard {{

static const uint8_t {array_name}[] = {{
{rows_joined}
}};
static const size_t {array_name}_LEN = {len(compressed)};

}} // namespace asgard_dashboard
}} // namespace esphome
"""

    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Success! {output_file} generated. Size reduced from {len(content)} to {len(compressed)} bytes.")

if __name__ == "__main__":
    # Syntax-check before generating, so a broken JS fails the build
    if not check_js_syntax("dashboard_source.html"):
        raise SystemExit(1)
    if not check_js_syntax("setup.html"):
        raise SystemExit(1)
    generate_header("dashboard_source.html", "dashboard_html.h", "DASHBOARD_HTML_GZ")
    generate_header("setup.html", "setup_html.h", "SETUP_HTML_GZ")
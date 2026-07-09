import gzip
import os
import re

def generate_header(source_file, output_file, array_name):
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

    header_content = f"""#pragma once
#include <stdint.h>
#include <stddef.h>

namespace esphome {{
namespace asgard_dashboard {{

static const uint8_t {array_name}[] = {{
{",\n".join(rows)}
}};
static const size_t {array_name}_LEN = {len(compressed)};

}} // namespace asgard_dashboard
}} // namespace esphome
"""

    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Success! {output_file} generated. Size reduced from {len(content)} to {len(compressed)} bytes.")

if __name__ == "__main__":
    # Ensure you name your source file "setup.html" in the same folder
    generate_header("dashboard_source.html", "dashboard_html.h", "DASHBOARD_HTML_GZ")
    generate_header("setup.html", "setup_html.h", "SETUP_HTML_GZ")
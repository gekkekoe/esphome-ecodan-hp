import gzip
import os

def generate_header(source_file, output_file):
    if not os.path.exists(source_file):
        print(f"Error: {source_file} niet gevonden!")
        return

    with open(source_file, 'rb') as f:
        content = f.read()

    compressed = gzip.compress(content, compresslevel=9)
    
    hex_array = []
    for i, byte in enumerate(compressed):
        hex_array.append(f"0x{byte:02x}")

    rows = []
    for i in range(0, len(hex_array), 16):
        rows.append("  " + ", ".join(hex_array[i:i+16]))

    header_content = f"""// Auto-generated gzipped dashboard HTML
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace esphome {{
namespace asgard_dashboard {{

static const uint8_t DASHBOARD_HTML_GZ[] = {{
{",\n".join(rows)}
}};
static const size_t DASHBOARD_HTML_GZ_LEN = {len(compressed)};

}} // namespace asgard_dashboard
}} // namespace esphome
"""

    with open(output_file, 'w') as f:
        f.write(header_content)
    
    print(f"Succes! {output_file} gegenereerd ({len(content)} -> {len(compressed)} bytes)")

if __name__ == "__main__":
    generate_header("dashboard_source.html", "dashboard_html.h")

import urllib.request
import gzip

files = {
    "CHARTJS": "https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js",
    "HAMMERJS": "https://cdn.jsdelivr.net/npm/hammerjs@2.0.8/hammer.min.js",
    "ZOOMJS": "https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1/dist/chartjs-plugin-zoom.min.js"
}

with open("dashboard_js.h", "w") as f:
    f.write("#pragma once\n\n")
    
    for name, url in files.items():
        print(f"Downloading {name}...")
        req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
        js_data = urllib.request.urlopen(req).read()
        
        gz_data = gzip.compress(js_data)
        hex_array = ', '.join([f'0x{b:02x}' for b in gz_data])
        f.write(f"const size_t {name}_GZ_LEN = {len(gz_data)};\n")
        f.write(f"const uint8_t {name}_GZ[] PROGMEM = {{\n  {hex_array}\n}};\n\n")

print("Successfully generated dashboard_js.h!")
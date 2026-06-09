import re
import gzip

html = open('index.html', 'r', encoding='utf-8').read()

# Remove the chart update logic inside ws.onmessage
html = re.sub(r'if\s*\(sensorChart\)\s*\{[^\}]+\}\s*\}\s*\}', r'}\n                    }', html, flags=re.DOTALL)

# Remove canvas and chart container
html = re.sub(r'<div class="card">\s*<h2>Telemetria</h2>.*?<canvas id="sensorChart"></canvas>\s*</div>\s*</div>', '', html, flags=re.DOTALL)

open('index.html', 'w', encoding='utf-8').write(html)

c = gzip.compress(html.encode('utf-8'))
s = '#pragma once\n#include <pgmspace.h>\nconst uint8_t index_html_gz[] PROGMEM = {\n'
s += ', '.join(['0x%02X'%b for b in c])
s += '\n};\n'
open('include/WebUI_HTML.h', 'w').write(s)

import gzip
import re

html = open('index.html', 'r', encoding='utf-8').read()

# Remove google fonts
html = re.sub(r'<link rel="preconnect" href="https://fonts\.googleapis\.com">\n\s*', '', html)
html = re.sub(r'<link rel="preconnect" href="https://fonts\.gstatic\.com" crossorigin>\n\s*', '', html)
html = re.sub(r'<link href="https://fonts\.googleapis\.com.*?rel="stylesheet">\n\s*', '', html)

# Remove chartjs CDN link
html = re.sub(r'<script src="https://cdn\.jsdelivr\.net/npm/chart\.js"></script>\n\s*', '', html)

# Read downloaded chart.js
chart_js = open('chart.min.js', 'r', encoding='utf-8').read()

# Inject chart.js in the head
html = html.replace('</title>', '</title>\n    <script>' + chart_js + '</script>')

open('index_embedded.html', 'w', encoding='utf-8').write(html)

c = gzip.compress(html.encode('utf-8'))
s = '#pragma once\n#include <pgmspace.h>\nconst uint8_t index_html_gz[] PROGMEM = {\n'
s += ', '.join(['0x%02X'%b for b in c])
s += '\n};\n'
open('include/WebUI_HTML.h', 'w').write(s)
print('Done!')

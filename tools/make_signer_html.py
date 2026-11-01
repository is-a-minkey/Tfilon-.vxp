#!/usr/bin/env python3
"""Embed an (unsigned) .vxp into signer_template.html -> a single offline page that signs it for a SIM.
usage: make_signer_html.py Tfilon.vxp Tfilon_signer.html"""
import base64, os, sys
here = os.path.dirname(os.path.abspath(__file__))
tpl = open(os.path.join(here, 'signer_template.html'), encoding='utf-8').read()
b64 = base64.b64encode(open(sys.argv[1], 'rb').read()).decode('ascii')
open(sys.argv[2], 'w', encoding='utf-8').write(tpl.replace('__VXP_B64__', b64))
print("wrote", sys.argv[2], os.path.getsize(sys.argv[2]), "bytes")

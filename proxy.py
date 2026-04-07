import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
import ssl
import re

class ProtosProxy(BaseHTTPRequestHandler):
    def do_GET(self):
        url = self.path[1:]
        if not url: url = "example.com"
        if not url.startswith('http'): url = 'https://' + url
            
        print(f"-> O PROTOS pediu para ler: {url}")
        
        try:
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            
            req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'})
            with urllib.request.urlopen(req, context=ctx) as response:
                html = response.read().decode('utf-8', errors='ignore')
            
            html = re.sub(r'<script.*?</script>', '', html, flags=re.DOTALL | re.IGNORECASE)
            html = re.sub(r'<style.*?</style>', '', html, flags=re.DOTALL | re.IGNORECASE)
            html = re.sub(r'<svg.*?</svg>', '', html, flags=re.DOTALL | re.IGNORECASE)
            html = re.sub(r'<nav.*?</nav>', '', html, flags=re.DOTALL | re.IGNORECASE)
            html = re.sub(r'', '', html, flags=re.DOTALL)
            
            html = re.sub(r'<input[^>]*type=[\'"]?hidden[\'"]?[^>]*>', '', html, flags=re.IGNORECASE)
            
            html = re.sub(r'<input[^>]*>', '<input>Pesquisa...</input>', html, flags=re.IGNORECASE)
            html = re.sub(r'<img[^>]*>', '<img>Imagem (Esfera)</img>', html, flags=re.IGNORECASE)
            
            html = re.sub(r'<(h[1-6]|a|button)[^>]*>', r'<\1>', html, flags=re.IGNORECASE)
            html = re.sub(r'</(h[1-6]|a|button)>', r'</\1>', html, flags=re.IGNORECASE)
            
            html = re.sub(r'<div[^>]*>', '\n', html, flags=re.IGNORECASE)
            html = re.sub(r'<p[^>]*>', '\n', html, flags=re.IGNORECASE)
            html = re.sub(r'<br[^>]*>', '\n', html, flags=re.IGNORECASE)
            html = re.sub(r'<li[^>]*>', '\n* ', html, flags=re.IGNORECASE)
            
            html = re.sub(r'\n\s*\n', '\n', html)
            
            html_bytes = html.encode('utf-8')
            
            self.send_response(200)
            self.send_header('Content-type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(html_bytes)))
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(html_bytes)
            print("-> Limpeza concluida! HTML enviado com sucesso.")
            
        except Exception as e:
            self.send_response(500)
            self.send_header('Content-type', 'text/html')
            self.send_header('Connection', 'close')
            self.end_headers()
            self.wfile.write(f"<h1>Erro</h1>\n{str(e)}".encode('utf-8'))

print("Motor de Renderizacao PROTOS a escuta na porta 8080...")
HTTPServer(('0.0.0.0', 8080), ProtosProxy).serve_forever()
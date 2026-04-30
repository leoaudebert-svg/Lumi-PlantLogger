from pathlib import Path

Import("env")

PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
BUILD_DIR = Path(env.subst("$BUILD_DIR"))
FRONTEND_DIR = PROJECT_DIR / "Frontend"
OUT_DIR = BUILD_DIR / "generated"
OUT_FILE = OUT_DIR / "dashboard_page.h"


def read_text(path):
    return path.read_text(encoding="utf-8")


def inject_assets():
    html_path = FRONTEND_DIR / "index.html"
    css_path = FRONTEND_DIR / "style.css"
    js_path = FRONTEND_DIR / "app.js"

    html = read_text(html_path)
    css = read_text(css_path)
    js = read_text(js_path)

    html = html.replace("<!-- PLANTLOGGER_STYLE -->", f"<style>\n{css}\n</style>")
    html = html.replace("<!-- PLANTLOGGER_SCRIPT -->", f"<script>\n{js}\n</script>")

    delimiter = "PLDASH"
    if delimiter in html:
        raise RuntimeError(f"Generated dashboard contains reserved delimiter {delimiter}")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    header = (
        "#pragma once\n"
        "#include <Arduino.h>\n\n"
        f"static const char HTML_PAGE[] PROGMEM = R\"{delimiter}({html}){delimiter}\";\n"
    )

    if OUT_FILE.exists() and OUT_FILE.read_text(encoding="utf-8") == header:
        return
    OUT_FILE.write_text(header, encoding="utf-8", newline="\n")


inject_assets()
env.Append(CPPPATH=[str(OUT_DIR)])

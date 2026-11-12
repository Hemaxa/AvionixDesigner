#!/usr/bin/env python3
"""Проверки состава сборки, комментариев типов и ресурсов без сторонних библиотек.

Это структурная проверка, а не анализ достижимости C++/Qt-кода: сигналы,
виртуальные методы и динамические пути требуют сборки и проверки вручную.
"""

from pathlib import Path
import re
import sys
import xml.etree.ElementTree as ET


def main():
    root = Path(__file__).resolve().parent.parent
    sources = sorted(p for p in (root / "src").rglob("*") if p.suffix in {".h", ".cpp"})
    errors = []
    cmake = re.sub(r"#[^\n]*", "", (root / "CMakeLists.txt").read_text())
    listed = set(re.findall(r"\bsrc/[\w/]+\.(?:h|cpp)\b", cmake))
    actual = {p.relative_to(root).as_posix() for p in sources}
    for name in sorted(actual - listed):
        errors.append(f"Исходник отсутствует в CMakeLists.txt: {name}")
    for name in sorted(listed - actual):
        errors.append(f"CMakeLists.txt ссылается на отсутствующий файл: {name}")

    definitions = re.compile(r"^\s*(?:class|struct)\s+([\w:]+)\b[^;{]*(?:\{|$)")
    type_count = 0
    for source in sources:
        lines = source.read_text().splitlines()
        for index, line in enumerate(lines):
            match = definitions.match(line)
            if not match:
                continue
            type_count += 1
            if not any("//" in previous or "*/" in previous for previous in lines[max(0, index - 3):index]):
                errors.append(f"Нет пояснения к {match[1]}: {source.relative_to(root)}:{index + 1}")

    references = "\n".join(p.read_text() for p in sources)
    references += "\n" + (root / "res/themes/AvionixDesignerTheme.qss").read_text()
    patterns = []
    for reference in re.findall(r":/[\w%./-]+", references):
        pattern = re.escape(reference)
        patterns.append(re.compile(re.sub(r"%[1-9]", r"[^/]+", pattern) + "$"))
    resources = ET.parse(root / "res/resources.qrc")
    resource_count = 0
    for group in resources.getroot().findall("qresource"):
        for entry in group.findall("file"):
            resource_count += 1
            path = root / "res" / entry.text
            if not path.is_file():
                errors.append(f"Отсутствует ресурс: {path.relative_to(root)}")
            url = ":" + group.get("prefix", "").rstrip("/") + "/" + entry.get("alias", entry.text)
            if not any(pattern.fullmatch(url) for pattern in patterns):
                errors.append(f"Ресурс не упомянут в коде или динамическом шаблоне: {url}")

    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Проверено: {len(sources)} исходников, {type_count} описаний типов, {resource_count} ресурсов.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

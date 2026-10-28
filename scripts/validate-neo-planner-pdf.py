#!/usr/bin/env python3
"""Check actual planner PDF output from the disposable native UI acceptance run."""
import argparse
import hashlib
import json
from pathlib import Path
import re

from pypdf import PdfReader


def validate(output):
    results = []
    for name in ("lsp-preview", "lsp-saved"):
        folder = output / name
        pdfs, texts = list(folder.glob("subsurface-neo-plan-*.pdf")), list(folder.glob("subsurface-neo-plan-*.txt"))
        if len(pdfs) != 1 or len(texts) != 1:
            raise RuntimeError(f"Expected one actual TXT/PDF pair in {folder}")
        pdf, text = pdfs[0], texts[0].read_text(encoding="utf-8")
        reader = PdfReader(pdf)
        if len(reader.pages) < 2:
            raise RuntimeError(f"{name}: profile page was lost; expected profile plus schedule pages")
        profile = reader.pages[0].extract_text() or ""
        if "SUBSURFACE NEO PLAN PROFILE" not in profile:
            raise RuntimeError(f"{name}: first page is not the calculated profile")
        if "Surface GF at calculated checkpoints" not in profile or "Tissue loading" in profile:
            raise RuntimeError(f"{name}: LSP analysis must show its actual checkpoint fields only")
        actual = "\n".join(page.extract_text() or "" for page in reader.pages[1:])
        normalize = lambda value: re.sub(r"\s+", "", value)
        normalized = normalize(actual)
        missing = [line for line in text.splitlines() if line.strip() and normalize(line) not in normalized]
        if missing:
            raise RuntimeError(f"{name}: {len(missing)} TXT lines are missing from the PDF schedule")
        results.append({"file": pdf.name, "sha256": hashlib.sha256(pdf.read_bytes()).hexdigest(),
                        "pages": len(reader.pages), "profilePage": True, "allScheduleTextPreserved": True})
    (output / "pdf-validation.json").write_text(json.dumps(results, indent=2), encoding="utf-8")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    print(json.dumps(validate(parser.parse_args().output.resolve()), indent=2))

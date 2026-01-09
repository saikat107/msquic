import xml.etree.ElementTree as ET
from collections import defaultdict
import os
import sys

xml_file = sys.argv[1]
tree = ET.parse(xml_file)
root = tree.getroot()

files = defaultdict(dict)

for cls in root.iter("class"):
    filename = cls.attrib["filename"]

    # Normalize path for VS Code (important on Windows/WSL)
    filename = filename.replace("\\", "/")
    if not filename.startswith("/"):
        filename = "C/" + filename

    for line in cls.iter("line"):
        lineno = int(line.attrib["number"])
        hits = int(line.attrib.get("hits", "0"))
        files[filename][lineno] = hits

for filename, lines in files.items():
    print(f"SF:{filename}")
    for lineno in sorted(lines):
        print(f"DA:{lineno},{lines[lineno]}")
    print("end_of_record")

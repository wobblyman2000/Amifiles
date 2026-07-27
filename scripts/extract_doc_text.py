import sys
import os
import zipfile
import re
import xml.etree.ElementTree as ET

def extract_docx(path):
    try:
        with zipfile.ZipFile(path) as z:
            xml_content = z.read('word/document.xml')
            root = ET.fromstring(xml_content)
            texts = []
            # Find all <w:t> elements which contain text
            for el in root.iter():
                if el.tag.endswith('}t'):
                    texts.append(el.text or "")
                elif el.tag.endswith('}p'):
                    texts.append("\n")
            return "".join(texts)
    except Exception as e:
        return f"Error extracting DOCX: {e}"

def extract_odt(path):
    try:
        with zipfile.ZipFile(path) as z:
            xml_content = z.read('content.xml')
            root = ET.fromstring(xml_content)
            texts = []
            for el in root.iter():
                if el.tag.endswith('}p') or el.tag.endswith('}h'):
                    texts.append("".join(el.itertext()))
            return "\n".join(texts)
    except Exception as e:
        return f"Error extracting ODT: {e}"

def extract_rtf(path):
    try:
        with open(path, 'r', errors='ignore') as f:
            content = f.read()
        # Simple RTF text stripper regex
        pattern = re.compile(r'\\([a-z]{1,32})(-?\d+)? ?|\\\'[0-9a-f]{2}|\\\{|\\\}|;|([^\\{}]+)', re.IGNORECASE)
        text = []
        for match in pattern.finditer(content):
            word, arg, char = match.groups()
            if char:
                text.append(char)
        return "".join(text).strip()
    except Exception as e:
        return f"Error extracting RTF: {e}"

def main():
    if len(sys.argv) < 2:
        print("Usage: python extract_doc_text.py <path_to_file>")
        sys.exit(1)
        
    path = sys.argv[1]
    if not os.path.exists(path):
        print(f"File not found: {path}")
        sys.exit(1)
        
    ext = os.path.splitext(path)[1].lower()
    if ext == '.docx':
        print(extract_docx(path))
    elif ext == '.odt':
        print(extract_odt(path))
    elif ext == '.rtf':
        print(extract_rtf(path))
    else:
        print(f"Unsupported document format: {ext}")
        sys.exit(1)

if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""
PDF/DOCX Reader for Cursor IDE

Simple tool to read PDF and DOCX files. I built this because I needed to process
documents in Cursor IDE but couldn't find a good solution.

Usage:
    python pdf_docx_reader.py <file_path> [--output-format json|text]
    python pdf_docx_reader.py --help
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, List, Optional, Union

try:
    import PyPDF2
    import pdfplumber
except ImportError:
    PyPDF2 = None
    pdfplumber = None

try:
    from docx import Document
except ImportError:
    Document = None


class FileReaderError(Exception):
    """Custom exception for file reading errors."""
    pass


class PDFReader:
    """PDF file reader - uses pdfplumber first, falls back to PyPDF2 if needed."""
    
    def __init__(self):
        if not pdfplumber and not PyPDF2:
            raise FileReaderError(
                "PDF reading libraries not available. Install with: pip install pdfplumber PyPDF2"
            )
    
    def read_pdf(self, file_path: Union[str, Path]) -> Dict[str, any]:
        """Read PDF file and extract text content with metadata."""
        file_path = Path(file_path)
        
        if not file_path.exists():
            raise FileReaderError(f"PDF file not found: {file_path}")
        
        if not file_path.suffix.lower() == '.pdf':
            raise FileReaderError(f"File is not a PDF: {file_path}")
        
        result = {
            "file_path": str(file_path),
            "file_type": "PDF",
            "pages": [],
            "full_text": "",
            "metadata": {},
            "page_count": 0
        }
        
        # Try pdfplumber first - it's usually better at text extraction
        if pdfplumber:
            try:
                with pdfplumber.open(file_path) as pdf:
                    result["page_count"] = len(pdf.pages)
                    
                    for page_num, page in enumerate(pdf.pages, 1):
                        page_text = page.extract_text() or ""
                        page_data = {
                            "page_number": page_num,
                            "text": page_text,
                            "char_count": len(page_text)
                        }
                        result["pages"].append(page_data)
                        result["full_text"] += page_text + "\n"
                    
                    # Extract metadata
                    if hasattr(pdf, 'metadata') and pdf.metadata:
                        result["metadata"] = {
                            "title": pdf.metadata.get("Title", ""),
                            "author": pdf.metadata.get("Author", ""),
                            "subject": pdf.metadata.get("Subject", ""),
                            "creator": pdf.metadata.get("Creator", ""),
                            "producer": pdf.metadata.get("Producer", ""),
                            "creation_date": str(pdf.metadata.get("CreationDate", "")),
                            "modification_date": str(pdf.metadata.get("ModDate", ""))
                        }
                
                return result
                
            except Exception as e:
                print(f"pdfplumber failed, trying PyPDF2: {e}")
        
        # Fallback to PyPDF2 if pdfplumber failed
        if PyPDF2:
            try:
                with open(file_path, 'rb') as file:
                    pdf_reader = PyPDF2.PdfReader(file)
                    result["page_count"] = len(pdf_reader.pages)
                    
                    for page_num, page in enumerate(pdf_reader.pages, 1):
                        page_text = page.extract_text() or ""
                        page_data = {
                            "page_number": page_num,
                            "text": page_text,
                            "char_count": len(page_text)
                        }
                        result["pages"].append(page_data)
                        result["full_text"] += page_text + "\n"
                    
                    # Extract metadata
                    if pdf_reader.metadata:
                        result["metadata"] = {
                            "title": pdf_reader.metadata.get("/Title", ""),
                            "author": pdf_reader.metadata.get("/Author", ""),
                            "subject": pdf_reader.metadata.get("/Subject", ""),
                            "creator": pdf_reader.metadata.get("/Creator", ""),
                            "producer": pdf_reader.metadata.get("/Producer", ""),
                            "creation_date": str(pdf_reader.metadata.get("/CreationDate", "")),
                            "modification_date": str(pdf_reader.metadata.get("/ModDate", ""))
                        }
                
                return result
                
            except Exception as e:
                raise FileReaderError(f"Failed to read PDF with PyPDF2: {e}")
        
        raise FileReaderError("No PDF reading library available")


class DOCXReader:
    """DOCX file reader using python-docx library."""
    
    def __init__(self):
        if not Document:
            raise FileReaderError(
                "DOCX reading library not available. Install with: pip install python-docx"
            )
    
    def read_docx(self, file_path: Union[str, Path]) -> Dict[str, any]:
        """Read DOCX file and extract text content with metadata."""
        file_path = Path(file_path)
        
        if not file_path.exists():
            raise FileReaderError(f"DOCX file not found: {file_path}")
        
        if not file_path.suffix.lower() == '.docx':
            raise FileReaderError(f"File is not a DOCX: {file_path}")
        
        try:
            doc = Document(file_path)
            
            result = {
                "file_path": str(file_path),
                "file_type": "DOCX",
                "paragraphs": [],
                "full_text": "",
                "metadata": {},
                "paragraph_count": 0
            }
            
            # Extract paragraphs
            for para_num, paragraph in enumerate(doc.paragraphs, 1):
                para_text = paragraph.text.strip()
                if para_text:  # Only include non-empty paragraphs
                    para_data = {
                        "paragraph_number": para_num,
                        "text": para_text,
                        "char_count": len(para_text),
                        "style": paragraph.style.name if paragraph.style else "Normal"
                    }
                    result["paragraphs"].append(para_data)
                    result["full_text"] += para_text + "\n"
            
            result["paragraph_count"] = len(result["paragraphs"])
            
            # Extract metadata
            core_props = doc.core_properties
            result["metadata"] = {
                "title": core_props.title or "",
                "author": core_props.author or "",
                "subject": core_props.subject or "",
                "keywords": core_props.keywords or "",
                "comments": core_props.comments or "",
                "last_modified_by": core_props.last_modified_by or "",
                "created": str(core_props.created) if core_props.created else "",
                "modified": str(core_props.modified) if core_props.modified else "",
                "revision": core_props.revision or 0
            }
            
            return result
            
        except Exception as e:
            raise FileReaderError(f"Failed to read DOCX file: {e}")


class FileReader:
    """Unified file reader for PDF and DOCX files."""
    
    def __init__(self):
        self.pdf_reader = PDFReader() if pdfplumber or PyPDF2 else None
        self.docx_reader = DOCXReader() if Document else None
    
    def read_file(self, file_path: Union[str, Path]) -> Dict[str, any]:
        """Read a file and return its content based on file extension."""
        file_path = Path(file_path)
        extension = file_path.suffix.lower()
        
        if extension == '.pdf':
            if not self.pdf_reader:
                raise FileReaderError("PDF reader not available. Install required libraries.")
            return self.pdf_reader.read_pdf(file_path)
        
        elif extension == '.docx':
            if not self.docx_reader:
                raise FileReaderError("DOCX reader not available. Install required libraries.")
            return self.docx_reader.read_docx(file_path)
        
        else:
            raise FileReaderError(f"Unsupported file type: {extension}. Supported types: .pdf, .docx")
    
    def get_supported_extensions(self) -> List[str]:
        """Get list of supported file extensions."""
        extensions = []
        if self.pdf_reader:
            extensions.append('.pdf')
        if self.docx_reader:
            extensions.append('.docx')
        return extensions


def format_output(data: Dict[str, any], output_format: str = "json") -> str:
    """Format the output data based on the specified format."""
    if output_format == "json":
        return json.dumps(data, indent=2, ensure_ascii=False)
    elif output_format == "text":
        output = f"File: {data['file_path']}\n"
        output += f"Type: {data['file_type']}\n"
        
        if data['file_type'] == 'PDF':
            output += f"Pages: {data['page_count']}\n"
        elif data['file_type'] == 'DOCX':
            output += f"Paragraphs: {data['paragraph_count']}\n"
        
        output += f"\nMetadata:\n"
        for key, value in data['metadata'].items():
            if value:
                output += f"  {key}: {value}\n"
        
        output += f"\nContent:\n{data['full_text']}"
        return output
    else:
        raise ValueError(f"Unsupported output format: {output_format}")


def main():
    """Main CLI function."""
    parser = argparse.ArgumentParser(
        description="Read PDF and DOCX files and extract text content for AI agents",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python pdf_docx_reader.py document.pdf
  python pdf_docx_reader.py document.docx --output-format text
  python pdf_docx_reader.py document.pdf --output-format json > output.json
        """
    )
    
    parser.add_argument(
        "file_path",
        help="Path to the PDF or DOCX file to read"
    )
    
    parser.add_argument(
        "--output-format",
        choices=["json", "text"],
        default="json",
        help="Output format: json (default) or text"
    )
    
    parser.add_argument(
        "--version",
        action="version",
        version="PDF/DOCX Reader Tool v1.0.0"
    )
    
    args = parser.parse_args()
    
    try:
        reader = FileReader()
        
        # Check if file exists
        file_path = Path(args.file_path)
        if not file_path.exists():
            print(f"Error: File not found: {file_path}", file=sys.stderr)
            sys.exit(1)
        
        # Check if file type is supported
        if file_path.suffix.lower() not in reader.get_supported_extensions():
            print(f"Error: Unsupported file type: {file_path.suffix}", file=sys.stderr)
            print(f"Supported types: {', '.join(reader.get_supported_extensions())}", file=sys.stderr)
            sys.exit(1)
        
        # Read the file
        data = reader.read_file(file_path)
        
        # Format and output the result
        output = format_output(data, args.output_format)
        print(output)
        
    except FileReaderError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""
Setup script for PDF/DOCX Reader
"""

from setuptools import setup, find_packages

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

with open("requirements.txt", "r", encoding="utf-8") as fh:
    requirements = [line.strip() for line in fh if line.strip() and not line.startswith("#")]

setup(
    name="pdf-docx-reader",
    version="1.0.0",
    author="param",
    description="Simple tool to read PDF and DOCX files",
    long_description=long_description,
    long_description_content_type="text/markdown",
    py_modules=["pdf_docx_reader"],
    python_requires=">=3.7",
    install_requires=requirements,
    entry_points={
        "console_scripts": [
            "pdf-docx-reader=pdf_docx_reader:main",
        ],
    },
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Operating System :: OS Independent",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Topic :: Text Processing",
        "Topic :: Utilities",
    ],
    keywords="pdf docx text extraction ai agents cursor ide",
    project_urls={
        "Bug Reports": "https://github.com/yourusername/pdf-docx-reader/issues",
        "Source": "https://github.com/yourusername/pdf-docx-reader",
    },
)

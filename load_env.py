"""
Load environment variables from .env file for PlatformIO
This script is executed before the build process starts
"""

Import("env")
import os

try:
    from dotenv import load_dotenv

    # Load .env file from project root
    load_dotenv()
    print("✓ Loaded environment variables from .env file")
except ImportError:
    print("⚠ python-dotenv not installed, using system environment variables")
    print("  Install with: pip install python-dotenv")
except Exception as e:
    print(f"⚠ Could not load .env file: {e}")

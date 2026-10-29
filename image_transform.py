import argparse
from pathlib import Path

from PIL import Image


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    parser.add_argument("targetname")
    args = parser.parse_args()
    input_path = Path(args.filename)
    output_path = Path(args.targetname)
    with Image.open(input_path) as image:
        image.save(output_path)

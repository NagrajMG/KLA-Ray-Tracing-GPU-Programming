import argparse
from pathlib import Path

from PIL import Image


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    args = parser.parse_args()
    input_path = Path(args.filename)
    with Image.open(input_path) as image:
        image.save(input_path.with_suffix(".png"))

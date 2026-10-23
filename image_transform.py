from PIL import Image
import argparse
from pathlib import Path

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("filename")
    args = parser.parse_args()
    input_path = Path(args.filename)
    output_path = Path("images") / str(input_path.with_suffix(".png"))
    with Image.open(input_path) as image:
        image.save(str(output_path))

from PIL import Image

with Image.open("image.ppm") as image:
    image.save("image.png", "PNG")


    


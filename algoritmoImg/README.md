# LSB-3 Steganography Project

This project implements a steganographic algorithm using Least Significant Bit (LSB) encoding to embed messages in images. It utilizes the stb_image.h library for image handling.

## Project Structure

```
lsb3-steganography
├── src
│   ├── main.c              # Entry point of the application
│   ├── lsb3.c              # Implementation of the LSB-3 algorithm
│   ├── lsb3.h              # Header file for LSB-3 functions
│   ├── image_utils.c       # Utility functions for image I/O
│   ├── image_utils.h       # Header file for image utility functions
│   └── stb_image.h         # stb_image library for loading images
├── include
│   └── stb_image_write.h   # stb_image_write library for saving images
├── Makefile                # Build instructions for the project
└── README.md               # Documentation for the project
```

## Requirements

- C compiler (e.g., GCC)
- stb_image.h and stb_image_write.h libraries

## Building the Project

To build the project, navigate to the project directory and run the following command:

```
make
```

This will compile the source files and create an executable.

## Running the Application

After building the project, you can run the application with the following command:

```
./lsb3-steganography <image_path> <message> <output_path>
```

- `<image_path>`: Path to the input image file.
- `<message>`: The message you want to embed in the image.
- `<output_path>`: Path where the modified image will be saved.

## Extracting Messages

To extract a message from an image, use the following command:

```
./lsb3-steganography <image_path> <message_length>
```

- `<image_path>`: Path to the image file containing the embedded message.
- `<message_length>`: The length of the message to extract.

## Usage Example

1. Embed a message:

```
./lsb3-steganography input.png "Hello, World!" output.png
```

2. Extract a message:

```
./lsb3-steganography output.png 13
```

## License

This project is licensed under the MIT License. See the LICENSE file for more details.
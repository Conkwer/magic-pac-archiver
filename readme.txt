---------------------------------------------------------------
Magic PAC Archiver
---------------------------------------------------------------

This tool allows you to unpack and pack PAC archives from Dreamcast games like MTGDC. 
It supports handling `.pvr` textures and `.sdcp` audio files with the correct extensions.

---------------------------------------------------------------
Features
---------------------------------------------------------------

- Unpacks `.pvr` textures and `.sdcp` audio files with the correct extensions.
- Creates an `info.json` file during unpacking to maintain file order and structure.
- Packs files back into a PAC archive using the `info.json` file if available.

---------------------------------------------------------------
Requirements:
---------------------------------------------------------------

- You can run the exe on Win7 x86 or higher 
- You can use Python 3.12 or higher

---------------------------------------------------------------
How to Unpack:
---------------------------------------------------------------

# Simple way:

Drag and drop example.pac onto example.cmd. Do not use paths with UTF-8 symbols.


# Command Line:

To unpack a PAC file, use the following command:
pac_archiver.exe unpack example.pac -d ./uncompressed


# Using Python:

python pac_archiver.py unpack example.pac -d ./uncompressed

If you do not specify an output directory, the files will be unpacked into a directory with the same name as the input file:
python pac_archiver.py unpack example.pac

# Using Precompiled Binaries:

pac_archiver.exe unpack example.pac

---------------------------------------------------------------
How to pack:
---------------------------------------------------------------

# Simple way:

Drag and drop "example" folder onto "example.cmd". Do not use paths with UTF-8 symbols.

# Using Python:

python pac_archiver.py pack ./uncompressed/example example.new.pac

# Using Precompiled Binaries:

pac_archiver.exe pack ./uncompressed/example example.new.pac

If an info.json file is present in the input folder, it will be used to maintain the correct file order and structure.
info.json is optional, if that file will be not found the script will pack the files based on there names, ignoring everything except: bin, pvr, sdcp, nzip.

---------------------------------------------------------------
Examples
---------------------------------------------------------------

Unpacking (using Python):
python pac_archiver.py unpack example.pac -d ./uncompressed
python pac_archiver.py unpack example.pac

Unpacking (using precompiled binaries):
pac_archiver.exe unpack example.pac -d ./uncompressed
pac_archiver.exe unpack example.pac

Packing (using Python):
python pac_archiver.py pack ./uncompressed/example example.new.pac

Packing (using precompiled binaries):
pac_archiver.exe pack ./uncompressed/example example.new.pac

---------------------------------------------------------------
Notes for Developers
---------------------------------------------------------------

The source code is included in the sources directory.
The scripts are designed to be run with Python 3.6 or higher.
You can compile the scripts into executables using pyinstaller.

---------------------------------------------------------------
License
---------------------------------------------------------------

This project is licensed under the MIT License.
Conkwer, 2024.

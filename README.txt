Lempel-Ziv-Welch(LZW) Encoder/Decoder						     

The program encodes/decodes any type of file using LZW algorithm without any loss of data
Language used: C

To create an executable C file:
	gcc -o LZW LZW.c

To encode a file:
	./LZW path/to/original.file e

To decode a file:
	./LZW path/to/encoded.file.LZW d

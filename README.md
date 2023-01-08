ptxt: Fast Text-to-PDF Converter
================================
The *ptxt* utility is a text-to-PDF converter aiming to:

- be blazing fast 🚀
- be light weight 🪶
- have a low memory footprint 💪
- support interactive conversion 📜

To reach these goals, *ptxt* uses an ad-hoc solution instead of depending on a full-feature PDF library or rendering engine.

News about this project will be published at [Twitter](https://twitter.com/dongyx2).

Output Style
------------
By default, the program produces a document as if written on a typewriter. 

The following options are configurable:

Option | Short | Long | Default
-|-|-|-
Number of columns | `-c` | `--columns`   | `80`
Number of rows    | `-r` | `--rows`      | `60`
Tab stop          | `-t` | `--tab-stop`  | `8`
Left margin       | `-x` | `--offx`      | `1in (72pt)`
Top margin        | `-t` | `--offy`      | `1in (72pt)`
Font size         | `-s` | `--font-size` | `10pt`
Font style        | `-f` | `--font`      | `mono`

Limitations
-----------
Some features are desired, but are not implemented yet:
- Allow font embedding.
- Right and bottom margins (requires font library.)
- Automatic column and row detection (requires font library.)

Examples
-----

- `ptxt file`

	Convert *file* to PDF using default options. The result is printed 
	to stdout. If *file* is omitted, stdin is used. The special `-` 
	also specifies stdin but with block buffering instead (useful if 
	you're piping large amounts of data.)

- `nl -b a ptxt.c | ptxt - -s 8 -c 72 -r 55 -f mono -t 4 -x 1.5in -y 1.5in -o ptxt.pdf`

	Number all lines in *ptxt.c* and convert to PDF using block buffered 
	stdin, 8pt font, 72 columns, 55 rows, standard monospaced font (Courier), 
	4-space tab stop, 1.5 inch left and top margins, outputting to 
	*ptxt.pdf*.

- `ptxt --help`

	Print the usage.

- `ptxt --version`

	Print version information.

See **ptxt(1)** for more information.

Installation
------------

	$ make NDEBUG=1
	$ sudo make install

To uninstall,

	$ sudo make uninstall

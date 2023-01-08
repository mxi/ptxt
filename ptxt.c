/* cl:	catalog object	(#0)
 * pr:	page tree root	(#1)
 * pg:	page object	(#PG_BASE ~ #PG_BASE+NPAGE-1)
 * pc:	page content	(#PC_BASE ~ #PC_BASE+NPAGE-1)
 * cs:	content stream
 * sl:	stream length	(#SL_BASE ~ #SL_BASE+NPAGE-1)
 */
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>

#define PROGNAME "ptxt"
#define VERSION "0.2.0 <https://github.com/dongyx/ptxt>"
#define AUTHORS "Written by DONG Yuxuan <https://www.dyx.name>\n" \
	            "Patched by Maxim Kasyanenko <https://github.com/mxi>\n"
#define NPAGE 32768
#define PG_BASE 32768
#define PC_BASE (PG_BASE+NPAGE)
#define SL_BASE (PC_BASE+NPAGE)
#define BUFSZIN 8192
#define BUFSZOUT 8192

#define NCOL 80
#define NROW 60
#define OFFX 72
#define OFFY 72
#define TABSTOP 8
#define FONTSIZE 10

#define align(val, bound) (((val+bound-1)/bound)*bound)
#define if_option(short, long) \
	if (strcmp(argv[i], short) == 0 || strcmp(argv[i], long) == 0)

int ineof();
int inc();
unsigned inln();
void outf(char const *fmt, ...);
void outc(int ch);
void outstr(char const *str);
void outname(char const *name);
void errf(char const *fmt, ...);
int genpdf();
int parse_int(char const *name, int *out, char const *str);
int parse_size(char const *name, int *out, char const *str);
void usage();
void version();

/* options */
enum {
	FONT_SANS,
	FONT_SERIF,
	FONT_MONO,
};
int ncol, nrow;
int offx, offy;
int tabstop;
int fontsize;
int fontidx;

/* state */
char const *fontnames[] = { "Helvetica", "Times-Roman", "Courier" }; /* FIXME: allow embedding other fonts other than "standard 14." */
FILE *fin, *fout;
int finmode, foutmode;
char bufin[BUFSZIN], bufout[BUFSZOUT];
int nread, nwrote, npage; /* FIXME: won't be large enough on 32-bit systems where int is 16 bits. */
int cl_pos, pr_pos, pg_pos[NPAGE], pc_pos[NPAGE], sl_pos[NPAGE];
int xref_pos;
char *line; /* malloc'd, sizeof = ncol+1 */

int main(int argc, char **argv)
{
	int i;
	int exitcode;

	fin = stdin;
	finmode = _IOLBF;
	fout = stdout;
	foutmode = _IOLBF;
	exitcode = EXIT_SUCCESS;

	/* default options */
	ncol = NCOL;
	nrow = NROW;
	offx = OFFX;
	offy = OFFY;
	tabstop = TABSTOP;
	fontsize = FONTSIZE;
	fontidx = FONT_MONO;

	for (i = 1; i < argc; ++i) {
		/* terminal switches */
		if_option ("-h", "--help") {
			usage();
			goto done;
		}
		if_option ("-v", "--version") {
			version();
			goto done;
		}
		/* options */
		if_option ("-c", "--columns") {
			if (!parse_int("Columns", &ncol, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (ncol <= 0) {
				errf("Columns must be a positive number, not \"%d\".\n", ncol);
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-r", "--rows") {
			if (!parse_int("Rows", &nrow, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (nrow <= 0) {
				errf("Rows must be a positive number, not \"%d\".\n", nrow);
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-t", "--tab-stop") {
			if (!parse_int("Tabstop", &tabstop, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (tabstop <= 0) {
				errf("Tabstop must be positive, not \"%d\".\n", tabstop);
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-x", "--offx") {
			if (!parse_size("X-offset", &offx, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option("-y", "--offy") {
			if (!parse_size("Y-offset", &offy, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-s", "--font-size") {
			if (!parse_size("Font size", &fontsize, argv[i+1])) {
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (fontsize <= 0) {
				errf("Font size must be positive.");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-f", "--font") {
			char const *font;
			if ((font = argv[i+1]) == NULL || !*font) {
				errf("Font type expected.\n");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (strcmp(font, "sans") == 0) {
				fontidx = FONT_SANS;
			}
			else if (strcmp(font, "serif") == 0) {
				fontidx = FONT_SERIF;
			}
			else if (strcmp(font, "mono") == 0) {
				fontidx = FONT_MONO;
			}
			else {
				errf("Unknown font type \"%s\".\n", font);
				errf("NOTE: only \"sans\", \"serif\", and \"mono\" supported "
					 "at the moment.\n");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if_option ("-o", "--output") {
			char const *of;
			if ((of = argv[i+1]) == NULL || !*of) {
				errf("Output file expected.\n");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if (strcmp(of, "-") == 0) {
				fout = stdout;
				foutmode = _IOFBF;
			}
			else if ((fout = fopen(of, "wb")) == NULL) {
				errf("Failed to open \"%s\" for output.\n", of);
				exitcode = EXIT_FAILURE;
				goto done;
			}
			++i;
		}
		else if (strcmp(argv[i], "-") == 0) { /* dash only (use stdin) */
			if (fin != stdin) {
				errf("Input file already opened.");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			fin = stdin;
			finmode = _IOFBF;
		}
		else if (*argv[i] == '-') { /* dash followed by something else */
			errf("Unknown option \"%s\"\n", argv[i]);
			exitcode = EXIT_FAILURE;
			usage();
			goto done;
		}
		else {
			if (fin != stdin) {
				errf("Input file already opened.");
				exitcode = EXIT_FAILURE;
				goto done;
			}
			if ((fin = fopen(argv[i], "r")) == NULL) {
				errf("Failed to open \"%s\" for input.\n", argv[i]);
				exitcode = EXIT_FAILURE;
				goto done;
			}
		}
	}

	if (setvbuf(fin, bufin, finmode, BUFSZIN) != 0) {
		errf("Failed to set input buffer.");
		exitcode = EXIT_FAILURE;
		goto done;
	}
	if (setvbuf(fout, bufout, foutmode, BUFSZOUT) != 0) {
		errf("Failed to set output buffer.");
		exitcode = EXIT_FAILURE;
		goto done;
	}
	if ((line = malloc(ncol+1)) == NULL) {
		errf("Failed to allocate %d byte line buffer.\n", ncol+1);
		goto done;
	}
	exitcode = genpdf();

done:
	if (fin) {
		fclose(fin);
	}
	if (fout) {
		fclose(fout);
	}
	if (line) {
		free(line);
	}
	return exitcode;
}

/* Returns non-zero if input is finished. It's a separate function in case
 * pushback buffer is added later. */
int ineof()
{
	return feof(fin);
}

/* Returns the next character in the input, or EOF. It's a separate function in 
 * case pushback buffer is added later. */
int inc()
{
	int ch;

	if ((ch = fgetc(fin)) != EOF) {
		++nread;
	}
	return ch;
}

unsigned inln()
{
	int i;
	unsigned n;

	n = nread;
	i = 0;
	while (i < ncol) {
		int ch, stop, pad;

		switch ((ch = inc())) {
			case EOF: 
			case '\n': {
				goto done;
			} break;

			/* TODO: indent using text operators */
			case '\t': {
				stop = align(i+1, tabstop);
				pad = ncol < stop ? ncol : stop;
				while (i < pad) {
					line[i++] = ' ';
				}
			} break;

			default: {
				line[i++] = ch;
			} break;
		}
	}

done:
	line[i] = 0;
	return nread - n;
}

void outf(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	nwrote += vfprintf(fout, fmt, ap);
	va_end(ap);
}

void outc(int ch)
{
	nwrote += fputc(ch, fout);
}

void outstr(char const *s)
{
	while (*s) {
		switch(*s) {
			case '(':  outf("\\(");  break;
			case ')':  outf("\\)");  break;
			case '\\': outf("\\\\"); break;
			case '\t': outf("\\t");  break;
			default:   outc(*s);     break;
		}
		++s;
	}
}

void outname(char const *s)
{
	while (*s) {
		/* pdf spec section 7.3.5 Name Objects, where the range is inclusive or 
		 * not is ambiguous, so we choose the strict range. */
		if (33 < *s && *s < 126) {
			outc(*s);
		}
		else {
			outf("#%02x", *s);
		}
		++s;
	}
}

int genpdf(FILE *in)
{
	int i, exitcode;

	exitcode = EXIT_SUCCESS;
	nwrote = 0;
	npage = 0;

	/* magic */
	outf("%%PDF-1.4\n");

	/* catalog */
	cl_pos = nwrote;
	outf("1 0 obj\n");
	outf("<< /Type /Catalog /Pages 2 0 R >>\n");
	outf("endobj\n");

	do {
		unsigned cs_begin, cs_end;
		/* page header */
		pg_pos[npage] = nwrote;
		outf("%d 0 obj\n", PG_BASE+npage);
		outf("<< /Type /Page\n");
		outf("/Parent 2 0 R\n");
		outf("/Contents %d 0 R >>\n", PC_BASE+npage);
		outf("endobj\n");

		/* page content */
		pc_pos[npage] = nwrote;
		outf("%d 0 obj\n", PC_BASE+npage);
		outf("<< /Length %d 0 R >>\n", SL_BASE+npage);
		outf("stream\n");
		cs_begin = nwrote;
		{
			int row, ch;
			outf("BT\n");
			outf("11 TL\n");
			outf("%d %d Td\n", offx, 842-offy); /* FIXME: hardcoded page size (8x11in) */
			outf("/F0 %d Tf\n", fontsize);
			row = 0;
			while (0 < inln()) {
				outf("T* (");
				outstr(line);
				outf(") Tj\n");
				if (++row == nrow) {
					break;
				}
			}
			outf("ET");
		}
		cs_end = nwrote;
		outf("\nendstream\n");
		outf("endobj\n");

		/* stream length (ref'd above)*/
		sl_pos[npage] = nwrote;
		outf("%d 0 obj\n", SL_BASE+npage);
		outf("%d\n", cs_end - cs_begin);
		outf("endobj\n");
	} 
	while (++npage < NPAGE && !ineof());

	if (!ineof()) {
		errf("Too many pages (limit is %d.)\n", NPAGE);
		exitcode = EXIT_FAILURE;
		goto done;
	}

	/* page tree root */
	assert(FONT_SANS <= fontidx && fontidx <= FONT_MONO);
	pr_pos = nwrote;
	outf("2 0 obj\n");
	outf("<< /Type /Pages\n");
	outf("/Kids [\n");
	for (i = 0; i < npage; i++) {
		outf("%d 0 R\n", PG_BASE+i);
	}
	outf("]\n");
	outf("/Count %d\n", npage);
	outf("/MediaBox [0 0 595 842]\n");
	outf("/Resources << /Font << /F0 <<\n");
	outf("/Type /Font\n");
	outf("/Subtype /TrueType\n");
	outf("/BaseFont /%s\n", fontnames[fontidx]);
	outf(">> >> >>\n");	/* end of resources dict */
	outf(">>\n");
	outf("endobj\n");

	/* xref */
	xref_pos = nwrote;
	outf("xref\n");
	outf("0 3\n");
	outf("0000000000 65535 f \n");
	outf("%010d 00000 n \n", cl_pos);
	outf("%010d 00000 n \n", pr_pos);

	outf("%d %d\n", PG_BASE, npage);
	for (i = 0; i < npage; i++) {
		outf("%010d 00000 n \n", pg_pos[i]);
	}

	outf("%d %d\n", PC_BASE, npage);
	for (i = 0; i < npage; i++) {
		outf("%010d 00000 n \n", pc_pos[i]);
	}

	outf("%d %d\n", SL_BASE, npage);
	for (i = 0; i < npage; i++) {
		outf("%010d 00000 n \n", sl_pos[i]);
	}

	/* tail */
	outf("trailer\n<< /Size %d /Root 1 0 R >>\n", SL_BASE+npage);
	outf("startxref\n");
	outf("%d\n", xref_pos);
	outf("%%EOF\n");

done:
	return exitcode;
}

void errf(char const *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

/* Parses an integer better than atoi into `out`. Returns 1 on success, 0 on
 * failure. */
int parse_int(char const *name, int *out, char const *str)
{
	enum { FAILURE, SUCCESS};
	int status;
	long n;
	char *end;

	status = SUCCESS;
	if (!str || !*str) {
		errf("%s requires an argument.\n", name);
		status = FAILURE;
		goto done;
	}

	n = strtol(str, &end, 0);
	if (errno == ERANGE) {
		errf("%s \"%s\" is too extreme.\n", name, str);
		status = FAILURE;
		goto done;
	}
	if (*end != 0) {
		errf("%s \"%s\" is not a valid integer.\n", name, str);
		status = FAILURE;
		goto done;
	}
	*out = (int) n;

done:
	return status;
}

/* Parses and converts the size in `str` to `out` in pt units. Returns 1 on 
 * success, 0 on failure. */
int parse_size(char const *name, int *out, char const *str)
{
	enum { FAILURE, SUCCESS };
	int status, sz;
	double raw;
	char *end;

	status = SUCCESS;
	if (!str || !*str) {
		errf("%s requires an argument.\n", name);
		status = FAILURE;
		goto done;
	}

	raw = strtod(str, &end);
	if (errno == ERANGE) {
		errf("%s \"%s\" is too extreme.\n", name, str);
		status = FAILURE;
		goto done;
	}
	if (str == end) {
		errf("%s \"%s\" is not numeric.\n", name, str);
		status = FAILURE;
		goto done;
	}
	/* units */
	if (strcasecmp(end, "pt") == 0) {
		/* do nothing, it's the default */
	}
	else if (strcasecmp(end, "in") == 0) {
		raw *= 72.;
	}
	else if (strcasecmp(end, "cm") == 0) {
		raw *= 28.346457;
	}
	else if (strcasecmp(end, "mm") == 0) {
		raw *= 2.8346457;
	}
	else if (*end != 0) {
		errf("%s \"%s\" units not recognized.\n", name, str);
		status = FAILURE;
		goto done;
	}

	/* round to the nearest one's w/o math.h */
	sz = (int) (raw * 10.);
	sz = (sz+5)/10;
	*out = sz;

done:
	return status;
}

void usage() 
{
	errf(
	"usage: ptxt [-c|--columns NATURAL]\n"
	"            [-r|--rows NATURAL]\n"
	"            [-t|--tab-stop NATURAL]\n"
	"            [-x|--offx SIZE]\n"
	"            [-y|--offy SIZE]\n"
	"            [-s|--font-size SIZE]\n"
	"            [-f|--font FONT]\n"
	"            [-o|--output FILE]\n"
	"            [-v|--version]\n"
	"            [-h|--help]\n"
	"            [FILE]\n"
	"\n"
	"NATURAL := Positive integer\n"
	"NUMBER := Non-negative real\n"
	"SIZE := NUMBER{pt|in|cm|mm}\n"
	"FONT := {sans|serif|mono}\n"
	"PATH := Filesystem path string\n"
	"FILE := {PATH|-}\n"
	);
}

void
version()
{
	errf("%s %s\n%s\n", PROGNAME, VERSION, AUTHORS);
}

/* vi:set sw=4 sts=4 ts=4 noet: */

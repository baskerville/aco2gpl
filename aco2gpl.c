/* aco2gpl.c - Converts a Photoshop palette to a GIMP palette
 *
 * Copyright(C) 2006 Salvatore Sanfilippo <antirez at gmail dot com>
 * All Rights Reserved.
 *
 * This software is released under the GPL license version 2
 * Read the LICENSE file in this software distribution for
 * more information.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct acoentry {
	unsigned char r, g, b;
	char *name; /* NULL if no name is available */
};

struct aco {
	int ver;	/* ACO file version, 1 or 2 */
	int len;	/* number of colors */
	struct acoentry *color; /* array of colors as acoentry structures */
};

/* proper error handling is not for lazy people ;) */
void *acomalloc(size_t len)
{
	void *ptr = malloc(len);
	if (ptr == NULL) {
		fprintf(stderr, "Out of memory!\n");
		exit(1);
	}
	return ptr;
}

/* Read a 16bit word in big endian from 'fp' and return it
 * converted in the host byte order as usigned int.
 * On end of file -1 is returned. */
int readword(FILE *fp)
{
	unsigned char buf[2];
	unsigned int w;
	int retval;

	retval = fread(buf, 1, 2, fp);
	if (retval == 0)
		return -1;
	w = buf[1] | buf[0] << 8;
	return w;
}

/* Version of readword() that exists with an error message
 * if an EOF occurs */
int mustreadword(FILE *fp) {
	int w;

	w = readword(fp);
	if (w == -1) {
		fprintf(stderr, "Unexpected end of file!\n");
		exit(1);
	}
	return w;
}

void gengpl(struct aco *aco)
{
	int j;
	int cols = 4;

	/* Print header */
	printf("GIMP Palette\nName: Untitled\nColumns: 16\n#");

	for (j = 0; j < aco->len; j += cols) {
		int i;
		for (i = 0; i < cols; i++) {
			int r, g, b;
			char *name;

			if (j+i == aco->len) break;
			r = aco->color[j+i].r;
			g = aco->color[j+i].g;
			b = aco->color[j+i].b;
			name = aco->color[j+i].name;
			printf("\n%d %d %d %s", r, g, b, name);
		}
	}
}

/* Convert the color read from 'fp' according to the .aco file version.
 * On success zero is returned and the pass-by-reference parameters
 * populated, otherwise non-zero is returned.
 *
 * The color name is stored in the 'name' buffer that can't
 * hold more than 'buflen' bytes including the nul-term. */
int convertcolor(FILE *fp, int ver, int *r, int *g, int *b,
                 char *name, int buflen)
{
	int cspace = mustreadword(fp);
	int namelen;

	if (cspace != 0) {
		int j;
		for (j = 0; j < 4; j++) mustreadword(fp);
		if (ver == 2) {
			mustreadword(fp);
			namelen = mustreadword(fp);
			for (j = 0; j < namelen; j++)
				mustreadword(fp);
		}
		fprintf(stderr, "Non RGB color (colorspace %d) skipped\n", cspace);
		return 1;
	}

	/* data in common between version 1 and 2 record */
	
	*r = mustreadword(fp)/256;
	*g = mustreadword(fp)/256;
	*b = mustreadword(fp)/256;
	mustreadword(fp); /* just skip this word, (Z not used for RGB) */
	if (ver == 1) return 0;

	/* version 2 specific data (name) */

	mustreadword(fp); /* just skip this word, don't know what it's used for */
	/* Color name, only for version 1 */
	namelen = mustreadword(fp);
	namelen--;
	while(namelen > 0) {
		int c = mustreadword(fp);
		
		if (c > 0xff) /* To handle utf-16 here is an overkill ... */
			c = ' ';
		if (buflen > 1) {
			*name++ = c;
			buflen--;
		}
		namelen--;
	}
	*name='\0';
	mustreadword(fp); /* Skip the nul term */
	return 0;
}

/* Read an ACO file from 'infp' FILE and return
 * the structure describing the palette.
 *
 * On initial end of file NULL is returned.
 * That's not a real library to read this format but just
 * an hack in order to write this convertion utility, so
 * on error we just exit(1) brutally after priting an error. */
struct aco *readaco(FILE *infp)
{
	int ver;
	int colors;
	int j;
	struct aco *aco;

	/* Read file version */
	ver = readword(infp);
	if (ver == -1) return NULL;
	fprintf(stderr, "reading ACO stream version:");
	if (ver == 1) {
		fprintf(stderr, " 1 (photoshop < 7.0)\n");
	} else if (ver == 2) {
		fprintf(stderr, " 2 (photoshop >= 7.0)\n");
	} else {
		fprintf(stderr, "Unknown ACO file version %d. Exiting...\n", ver);
		exit(1);
	}

	/* Read number of colors in this file */
	colors = readword(infp);
	fprintf(stderr, "%d colors in this file\n", colors);

	/* Allocate memory */
	aco = acomalloc(sizeof(*aco));
	aco->len = colors;
	aco->color = acomalloc(sizeof(struct acoentry)*aco->len);
	aco->ver = ver;

	/* Convert every color inside */
	for (j = 0; j < colors; j++) {
		int r,g,b;
		char name[256];

		if (convertcolor(infp, ver, &r, &g, &b, name, 256)) continue;
		aco->color[j].r = r;
		aco->color[j].g = g;
		aco->color[j].b = b;
		aco->color[j].name = NULL;
		if (ver == 2)
			aco->color[j].name = strdup(name); /* NULL means no name anyway */
	}
	return aco;
}

int main(void)
{
	struct aco *aco1, *aco2;

	aco1 = readaco(stdin);
	aco2 = readaco(stdin);
	fprintf(stderr, "Generating GPL...\n");
	if (aco2)
		gengpl(aco2);
	else if (aco1)
		gengpl(aco1);
	else
		fprintf(stderr, "No data!");
	fprintf(stderr, "Done.\n");
	return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
A postprocessor for KISSlicer ( http://kisslicer.com/ ) to show its output in
color when viewed in CraftWare ( http://www.craftunique.com/craftware ) by
Palasik Sandor.

KISSlicer must be set up to use craftmap in the Printer/Firmware tab in the
Post-Process line like this:

c:\full\path\of\program\craftmap.exe "<FILE>"

The program reads GCODE lines one at a time, and inserts a CraftWare comment
after some KISSlicer comments like this:

; 'Support Interface Path', 1.9 [feed mm/s], 30.0 [head mm/s]

The inserted CraftWare comments are:

;segType:HShell
;segType:Infill
;segType:InnerHair
;segType:Loop
;segType:OuterHair
;segType:Perimeter
;segType:Raft
;segType:Skirt
;segType:SoftSupport
;segType:Support

Now updated with 'bang removal', changes the feedrate of short segments to a
preset value.

The feedrate can be set by -f switch, the minimum segment length can be set by
-l.

The defaults are:

  -f900 -l2

Segments shorter than 2 millimeters will be written with F900 feedrate.
*/

struct COMMENT_MAP {
	const char *kiss;
	const char *craft;
	};

COMMENT_MAP comment_map[] = {
 { "Crown", "InnerHair" },
 { "Loop", "Loop" },
 { "Perimeter", "Perimeter" },
 { "Pillar", "Raft" },
 { "Prime Pillar", "Skirt" },
 { "Raft", "Raft" },
 { "Skirt", "Skirt" },
 { "Solid", "HShell" },
 { "Sparse Infill", "Infill" },
 { "Stacked Sparse Infill", "Infill" },
 { "Support (may Stack)", "Support" },
 { "Support Interface", "SoftSupport" },
};

#define DIM(x) (sizeof(x)/sizeof(x[0]))

char outname[0x400];

char linebuffer[0x4000];

FILE *input,*output;

double X,Y,F,pX,pY,written_F;

double min_F = 900;

double min_len = 2;
double min_len_sq;

void bang_removal()
{
	char *p;
	char *fstart = 0;
	char *fend = 0;
	int have_EZ = 0;

	X = pX;
	Y = pY;

	for (p = linebuffer; *p && *p != ';' && *p != '\n';) {
		switch(*p++) {
		case 'X':
			X = strtod(p,&p);
			break;

		case 'Y':
			Y = strtod(p,&p);
			break;

		case 'E':
		case 'Z':
			have_EZ = 1;
			break;

		case 'F':
			fstart = p-1;
			F = strtod(p,&p);
			fend = p;
			}
		}

	double dx = X-pX;
	double dy = Y-pY;

	pX = X;
	pY = Y;

	double lsq = dx*dx+dy*dy;

	double wrF = F;

	char fbuffer[32];

	if (lsq < min_len_sq) {
		if (min_F < F) {
			if (have_EZ && lsq == 0.0) {
				sprintf(fbuffer,"G1 F%g\n",min_F);

				if (written_F != min_F)
					fputs(fbuffer,output);

				fputs(linebuffer,output);

				fputs(fbuffer,output);

				written_F = min_F;

				return;
				}
			else
				wrF = min_F;
			}
		}

	if (fstart > linebuffer && fstart[-1] == ' ')
		fstart--;

	if (fstart && wrF == written_F)
		memmove(fstart,fend,strlen(fend)+1);
	else {
		if (fstart)
			written_F = F;
		else
			fstart = fend = p;

		if (wrF != written_F) {
			sprintf(fbuffer," F%g",wrF);

			int l = strlen(fbuffer);

			memmove(fstart+l,fend,strlen(fend)+1);
			memmove(fstart,fbuffer,l);

			written_F = wrF;
			}
		}

	fputs(linebuffer,output);
}

void insert_color_comments(char *inname)
{
	min_len_sq = min_len*min_len;

	input = fopen(inname,"r");

	if (! input) {
		printf("Cannot open %s\n",inname);

		return;
		}

	setvbuf(input,0,_IOFBF,0x4000);

	strcpy(outname,inname);

	strcat(outname,".$$$");

	output = fopen(outname,"w");

	if (! output) {
		printf("Cannot create %s\n",outname);
		return;
		}

	int translated = 0;

	while (fgets(linebuffer,sizeof(linebuffer)-1,input)) {
		const char *segtype = 0;
		const char *tail = 0;

		if (linebuffer[0] == ';' &&
		    linebuffer[1] == ' ' &&
		    linebuffer[2] == '\'' &&
		    (tail = strstr(linebuffer," Path\', ")) != 0) {
			size_t pl = (int)(tail-(linebuffer+3));

			for (int i=0; i<DIM(comment_map); i++) {
				const char *f = comment_map[i].kiss;

				if (strlen(f) != pl)
					continue;

				if (memcmp(linebuffer+3,f,pl) == 0) {
					segtype = comment_map[i].craft;

					break;
					}
				}
			}

 // allow reprocess
		if (translated && linebuffer[0] == ';' && memcmp(linebuffer,";segType:",9) == 0)
			continue;

 // G0 and G1 lines handled separately
		if (linebuffer[0] == 'G' && 
		    (linebuffer[1] == '0' || linebuffer[1] == '1') &&
		    (linebuffer[2] < '0' || linebuffer[2] > '9'))
			bang_removal();
		else
			fputs(linebuffer,output);

		if (segtype) {
			fprintf(output,";segType:%s\n",segtype);
			translated++;
			}
		}

	fclose(input);

	if (ferror(output)) {
		printf("Error writing %s\n",outname);

		return;
		}

	fclose(output);

	remove(inname);

	rename(outname,inname);
}

int main(int argc,char **argv)
{
	if (argc < 2) {
		printf(	"Usage: craftmap gcode_file ...\n"
			"\n"
			"feedrate for short segments: -f#\n"
			"length of short segments: -l#\n");

		return 1;
		}

	for (int i=1; i<argc; i++) {
		if (argv[i][0] == '-') {
			if (argv[i][1] == 'f')
				min_F = atof(argv[i]+2);

			if (argv[i][1] == 'l')
				double min_len = atof(argv[i]+2);
			}
		else
			insert_color_comments(argv[i]);
		}

	return 0;
}

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
 { "Prime Pillar", "Raft" },
 { "Raft", "Raft" },
 { "Skirt", "Skirt" },
 { "Solid", "HShell" },
 { "Sparse Infill", "Infill" },
 { "Stacked Sparse Infill", "Infill" },
 { "Support (may Stack)", "Support" },
 { "Support Interface", "SoftSupport" },
// { "Destring/Wipe/Jump", "" },
// { "Jump", "" },
};

#define DIM(x) (sizeof(x)/sizeof(x[0]))

char outname[0x400];

char linebuffer[0x4000];

FILE *input,*output;

void insert_color_comments(char *inname)
{
	input = fopen(inname,"r");

	if (! input) {
		printf("Cannot open %s\n",inname);

		return;
		}

	setvbuf(input,0,_IOFBF,0x4000);

	strcpy(outname,inname);

	char *ext = strrchr(outname,'.');
	char *tmp = ".$$$";

	if (ext)
		strcpy(ext,tmp);
	else
		strcat(outname,tmp);

	output = fopen(outname,"w");

	if (! output) {
		printf("Cannot create %s\n",outname);
		return;
		}

	while (fgets(linebuffer,sizeof(linebuffer),input)) {
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

		fputs(linebuffer,output);

		if (segtype)
			fprintf(output,";segType:%s\n",segtype);
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
		printf(	"Usage: craftmap gcode_file ...\n");

		return 1;
		}

	for (int i=1; i<argc; i++)
		insert_color_comments(argv[i]);

	return 0;
}

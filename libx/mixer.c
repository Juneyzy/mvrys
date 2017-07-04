#include "sysdefs.h"
#include "stdform.h"
#include "G711.h"

#define	ALAW_LEFT_CHANNEL		"vt/6285627239968080371-up.wav"
#define	ALAW_RIGHT_CHANNEL		"vt/6285627239968080371-down.wav"

void mixer_test ()
{
	
	const char *lalaw = ALAW_LEFT_CHANNEL, *ralaw = ALAW_RIGHT_CHANNEL;
	char mixfile[32] = "mix.wav", c;
	struct stat s;
	int l = 0, left = 0;
	FILE *fpl, *fpr, *fpmix;
	struct xlaw_head_t	xlaw_hdr;
	
	fpl = fopen (lalaw, "r");
	fpr = fopen (ralaw, "r");

	fpmix = fopen (mixfile, "w+");

	stat(lalaw, &s);
	l += s.st_size;

	stat(ralaw, &s);
	l += s.st_size;

	alaw_head_init (&xlaw_hdr, l, 8000, 2, 8);
	audio_head (mixfile, (void *)&xlaw_hdr);

	fwrite (&xlaw_hdr, sizeof(xlaw_hdr), 1, fpmix);
	while (1)  {
		if (!left) {
			if (1 == fread (&c, sizeof(char), 1, fpl)) {
				fwrite (&c, sizeof(char), 1, fpmix);
				left = 1;
				continue;
			}

			break;
		}

		if (left) {
			if (1 == fread (&c, sizeof(char), 1, fpr)) {
				fwrite (&c, sizeof (char), 1, fpmix);
				left = 0;
				continue;
			}

			break;
		}
	}
	
	fclose (fpl);
	fclose (fpr);
	fclose (fpmix);

	stat (mixfile, &s);
	printf ("Mix file size = %d\n", (int)s.st_size);
	
}


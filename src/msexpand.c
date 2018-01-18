/*
 *  msexpand: Microsoft "compress.exe/expand.exe" compatible decompressor
 *
 *  Copyright (c) 2000 Martin Hinner <mhi@penguin.cz>
 *  Algorithm & data structures by M. Winterhoff <100326.2776@compuserve.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdint.h>
#include <getopt.h>

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define WORDS_BIGENDIAN
#endif

extern char *version_string;

#define N 4096
#define F 16

int
expand (FILE *in, char *inname, FILE *out, char *outname)
{
  int bits, ch, i, j, len, mask;
  unsigned char *buffer;

  uint32_t magic1;
  uint32_t magic2;
  uint32_t magic3;
  uint16_t reserved;
  uint32_t filesize;

  if (fread (&magic1, 1, sizeof (magic1), in) != sizeof (magic1))
    {
      perror (inname);
      return -1;
    }

#ifdef WORDS_BIGENDIAN
  if (magic1 == 0x535A4444L)
#else
  if (magic1 == 0x44445A53L)
#endif
    {
      if (fread (&magic2, 1, sizeof (magic2), in) != sizeof (magic2))
	{
	  perror (inname);
	  return -1;
	}

      if (fread (&reserved, 1, sizeof (reserved), in) != sizeof (reserved))
	{
	  perror (inname);
	  return -1;
	}

      if (fread (&filesize, 1, sizeof (filesize), in) != sizeof (filesize))
	{
	  perror (inname);
	  return -1;
	}
#ifdef WORDS_BIGENDIAN
      if (magic2 != 0x88F02733L)
#else
      if (magic2 != 0x3327F088L)
#endif
	{
	  fprintf (stderr, "%s: This is not a MS-compressed file\n", inname);
	  return -1;
	}
    }
  else
#ifdef WORDS_BIGENDIAN
  if (magic1 == 0x4B57414AL)
#else
  if (magic1 == 0x4A41574BL)
#endif
    {
      if (fread (&magic2, 1, sizeof (magic2), in) != sizeof (magic2))
	{
	  perror (inname);
	  return -1;
	}

      if (fread (&magic3, 1, sizeof (magic3), in) != sizeof (magic3))
	{
	  perror (inname);
	  return -1;
	}

      if (fread (&reserved, 1, sizeof (reserved), in) != sizeof (reserved))
	{
	  perror (inname);
	  return -1;
	}

#ifdef WORDS_BIGENDIAN
      if (magic2 != 0x88F027D1L || magic3 != 0x03001200L)
#else
      if (magic2 != 0xD127F088L || magic3 != 0x00120003L)
#endif
	{
	  fprintf (stderr, "%s: This is not a MS-compressed file\n", inname);
	  return -1;
	}
       fprintf (stderr, "%s: Unsupported version 6.22\n", inname);
       return -1;
    }
  else
    {
      fprintf (stderr, "%s: This is not a MS-compressed file\n", inname);
      return -1;
    }


  buffer = malloc (N);
  if (!buffer)
    {
      fprintf (stderr, "%s:No memory\n", inname);
      return -1;
    }

  memset (buffer, ' ', N);

  i = N - F;
  while (1)
    {
      bits = getc (in);
      if (bits == EOF)
	break;

      for (mask = 0x01; mask & 0xFF; mask <<= 1)
	{
	  if (!(bits & mask))
	    {
	      j = getc (in);
	      if (j == EOF)
		break;
	      len = getc (in);
	      j += (len & 0xF0) << 4;
	      len = (len & 15) + 3;
	      while (len--)
		{
		  buffer[i] = buffer[j];
		  if (putc (buffer[i], out) == EOF)
		    {
		      perror (outname);
		      return -1;
		    }
		  j++;
		  j %= N;
		  i++;
		  i %= N;
		}
	    }
	  else
	    {
	      ch = getc (in);
	      if (ch == EOF)
		break;
	      buffer[i] = ch;
	      if (putc (buffer[i], out) == EOF)
		{
		  perror (outname);
		  return -1;
		}
	      i++;
	      i %= N;
	    }
	}
    }

  free (buffer);
  return 0;
}

void
usage (char *progname)
{
  printf ("Usage: %s [-h] [-V] [file ...]\n"
	  " -h --help        give this help\n"
	  " -V --version     display version number\n"
	  " file...          files to decompress. If none given, use"
	  " standard input.\n"
	  "\n"
	  "Report bugs to <mhi@penguin.cz>\n", progname);
  exit (0);
}

int
main (int argc, char **argv)
{
  FILE *in, *out;
  char *argv0;
  int c;
  char name[0x100];

  argv0 = argv[0];

  while ((c = getopt (argc, argv, "hV")) != -1)
    {
      switch (c)
	{
	case 'h':
	  usage (argv0);
	case 'V':
          printf ("msexpand version %s " __DATE__ "\n",
                        version_string);
	  return 0;
	default:
	  usage (argv0);
	}
    }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    {
      if (isatty (STDIN_FILENO))
	usage (argv0);
      if (expand (stdin, "STDIN", stdout, "STDOUT") < 0)
	return 1;
      return 0;
    }

  while (argc)
    {
      if (argv[0][strlen (argv[0]) - 1] != '_')
	{
	  fprintf (stderr, "%s: Doesn't end with underscore -- ignored\n", argv[0]);
	  argc--;
	  argv++;
	  continue;
	}

      in = fopen (argv[0], "r");
      if (!in)
	{
	  perror (argv[0]);
	  return 1;
	}

      strcpy (name, argv[0]);
      name[strlen (name) - 1] = 0;

      out = fopen (name, "wbx");
      if (!out)
	{
	  perror (name);
	  return 1;
	}

      expand (in, argv[0], out, name);
      fclose (in);
      fclose (out);

      argc--;
      argv++;
    }

  return 0;
}

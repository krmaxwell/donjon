/*
 * Copyright (C) 1999  John Olsson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/* Fractal Worldmap Generator Version 2.2
 *
 * Creator: John Olsson
 * Thanks to Carl Burke for interesting discussions and suggestions of
 * how to speed up the generation! :)
 *
 * This program is provided as is, and it's basically a "hack". So if you
 * want a better userinterface, you will have to provide it by yourself!
 * 
 * For ideas about how to implement different projections, you can always
 * look in WorldMapGenerator.c (the CGI program that generates the gifs
 * on my www-page (http://www.lysator.liu.se/~johol/fwmg/fwmg.html).
 *
 * Please visit my WWW-pages located at: http://www.lysator.liu.se/~johol/
 * You can send E-Mail to this adress: johol@lysator.liu.se
 *
 * I compile this program with: gcc -O3 worldgen.c -lm -o gengif
 * 
 * This program will write the GIF-file to a file which you are
 * prompted to specify.
 *
 * To change size of the generated picture, change the default values
 * of the variables XRange och YRange.
 *
 * You use this program at your own risk! :)
 *
 *
 * When you run the program you are prompted to input three values:
 *
 * Seed:             This the "seed" used to initialize the random number
 *                   generator. So if you use the same seed, you'll get the
 *                   same sequence of random numbers...
 *
 * Number of faults: This is how many iterations the program will do.
 *                   If you want to know how it works, just enter 1, 2, 3,...
 *                   etc. number of iterations and compare the different
 *                   GIF-files.
 *
 * PercentWater:          This should be a value between 0 and 100 (you can
 *                   input 1000 also, but I don't know what the program
 *                   is up to then! :) The number tells the "ratio"
 *                   between water and land. If you want a world with
 *                   just a few islands, input a large value (EG. 80 or
 *                   above), if you want a world with nearly no oceans,
 *                   a value near 0 would do that.
 *
 */

#include    <limits.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <unistd.h>
#include    <time.h>
#include    <string.h>
#include    <math.h>


/* These define:s are for the GIF-saver... */
/* a code_int must be able to hold 2**BITS values of type int, and also -1 */
typedef int             code_int;

#ifdef SIGNED_COMPARE_SLOW
typedef unsigned long int count_int;
typedef unsigned short int count_short;
#else /*SIGNED_COMPARE_SLOW*/
typedef long int          count_int;
#endif /*SIGNED_COMPARE_SLOW*/

static void BumpPixel ( void );
static int GIFNextPixel ( void );
static void GIFEncode (FILE* fp, int GWidth, int GHeight, int GInterlace, int Background, int BitsPerPixel, int Red[], int Green[], int Blue[]);
static void Putword ( int w, FILE* fp );
static void compress ( int init_bits, FILE* outfile );
static void output ( code_int code );
static void cl_block ( void );
static void cl_hash ( count_int hsize );
static void writeerr ( void );
static void char_init ( void );
static void char_out ( int c );
static void flush_char ( void );


/* My own definitions */

#ifndef PI
#define PI        3.141593
#endif

/* This value holds the maximum value rand() can generate
 *
 * RAND_MAX *might* be defined in stdlib.h, if it's not
 * you *might* have to change the definition of MAX_RAND...
 */
#ifdef RAND_MAX
#define MAX_RAND  RAND_MAX
#else
#define MAX_RAND  0x7FFFFFFF
#endif

/* Function that generates the worldmap */
void GenerateWorldMap();

/* 4-connective floodfill algorithm which I use for constructing
 *  the ice-caps.*/
void FloodFill4(int x, int y, int OldColor);

int             *WorldMapArray;
int             XRange = 320;
int             YRange = 160;
int             Histogram[256];
int             FilledPixels;
int             Red[49]   = {0,0,0,0,0,0,0,0,34,68,102,119,136,153,170,187,
			     0,34,34,119,187,255,238,221,204,187,170,153,
			     136,119,85,68,
			     255,250,245,240,235,230,225,220,215,210,205,200,
			     195,190,185,180,175};
int             Green[49] = {0,0,17,51,85,119,153,204,221,238,255,255,255,
			     255,255,255,68,102,136,170,221,187,170,136,
			     136,102,85,85,68,51,51,34,
			     255,250,245,240,235,230,225,220,215,210,205,200,
			     195,190,185,180,175};
int             Blue[49]  = {0,68,102,136,170,187,221,255,255,255,255,255,
			     255,255,255,255,0,0,0,0,0,34,34,34,34,34,34,
			     34,34,34,17,0,
			     255,250,245,240,235,230,225,220,215,210,205,200,
			     195,190,185,180,175};

float           YRangeDiv2, YRangeDivPI;
float           *SinIterPhi;


void main(int argc, char **argv)
{
  int       NumberOfFaults=0, a, j, i, Color, MaxZ=1, MinZ=-1;
  int       row, TwoColorMode=0;
  int       index2;
  unsigned  Seed=0;
  int       Threshold, Count;
  int       PercentWater, PercentIce, Cur;
  char SaveName[256];  /* 255 character filenames should be enough? */
  char SaveFile[256];  /* SaveName + .gif */
  FILE * Save;
  
  WorldMapArray = (int *) malloc(XRange*YRange*sizeof(int));
  if (WorldMapArray == NULL)
  {
    fprintf(stderr, "I can't allocate enough memory!\n");
  }

  SinIterPhi = (float *) malloc(2*XRange*sizeof(float));
  if (SinIterPhi == NULL)
  {
    fprintf(stderr, "I can't allocate enough memory!\n");
  }
  
  for (i=0; i<XRange; i++)
  {
    SinIterPhi[i] = SinIterPhi[i+XRange] = (float)sin(i*2*PI/XRange);
  }

  fprintf(stderr, "Seed: ");
  scanf("%d", &Seed);
  fprintf(stderr, "Number of faults: ");
  scanf("%d", &NumberOfFaults);
  fprintf(stderr, "Percent water: ");
  scanf("%d", &PercentWater);
  fprintf(stderr, "Percent ice: ");
  scanf("%d", &PercentIce);

  fprintf(stderr, "Save as (.GIF will be appended): ");
  scanf("%8s", SaveName);
  
  srand(Seed);

  for (j=0, row=0; j<XRange; j++)
  {
    WorldMapArray[row] = 0;
    for (i=1; i<YRange; i++) WorldMapArray[i+row] = INT_MIN;
    row += YRange;
  }

  /* Define some "constants" which we use frequently */
  YRangeDiv2  = YRange/2;
  YRangeDivPI = YRange/PI;

  /* Generate the map! */
  for (a=0; a<NumberOfFaults; a++)
  {
    GenerateWorldMap();
  }
  
  /* Copy data (I have only calculated faults for 1/2 the image.
   * I can do this due to symmetry... :) */
  index2 = (XRange/2)*YRange;
  for (j=0, row=0; j<XRange/2; j++)
  {
    for (i=1; i<YRange; i++)                    /* fix */
    {
      WorldMapArray[row+index2+YRange-i] = WorldMapArray[row+i];
    }
    row += YRange;
  }

  /* Reconstruct the real WorldMap from the WorldMapArray and FaultArray */
  for (j=0, row=0; j<XRange; j++)
  {
    /* We have to start somewhere, and the top row was initialized to 0,
     * but it might have changed during the iterations... */
    Color = WorldMapArray[row];
    for (i=1; i<YRange; i++)
    {
      /* We "fill" all positions with values != INT_MIN with Color */
      Cur = WorldMapArray[row+i];
      if (Cur != INT_MIN)
      {
	Color += Cur;
      }
      WorldMapArray[row+i] = Color;
    }
    row += YRange;
  }
  
  /* Compute MAX and MIN values in WorldMapArray */
  for (j=0; j<XRange*YRange; j++)
  {
    Color = WorldMapArray[j];
    if (Color > MaxZ) MaxZ = Color;
    if (Color < MinZ) MinZ = Color;
  }
  
  /* Compute color-histogram of WorldMapArray.
   * This histogram is a very crude aproximation, since all pixels are
   * considered of the same size... I will try to change this in a
   * later version of this program. */
  for (j=0, row=0; j<XRange; j++)
  {
    for (i=0; i<YRange; i++)
    {
      Color = WorldMapArray[row+i];
      Color = (int)(((float)(Color - MinZ + 1) / (float)(MaxZ-MinZ+1))*30)+1;
      Histogram[Color]++;
    }
    row += YRange;
  }

  /* Threshold now holds how many pixels PercentWater means */
  Threshold = PercentWater*XRange*YRange/100;

  /* "Integrate" the histogram to decide where to put sea-level */
  for (j=0, Count=0;j<256;j++)
  {
    Count += Histogram[j];
    if (Count > Threshold) break;
  }
  
  /* Threshold now holds where sea-level is */
  Threshold = j*(MaxZ - MinZ + 1)/30 + MinZ;

  if (TwoColorMode)
  {
    for (j=0, row=0; j<XRange; j++)
    {
      for (i=0; i<YRange; i++)
      {
	Color = WorldMapArray[row+i];
	if (Color < Threshold)
	  WorldMapArray[row+i] = 3;
	else
	  WorldMapArray[row+i] = 20;
      }
      row += YRange;
    }
  }
  else
  {
    /* Scale WorldMapArray to colorrange in a way that gives you
     * a certain Ocean/Land ratio */
    for (j=0, row=0; j<XRange; j++)
    {
      for (i=0; i<YRange; i++)
      {
	Color = WorldMapArray[row+i];
	
	if (Color < Threshold)
	  Color = (int)(((float)(Color - MinZ) / (float)(Threshold - MinZ))*15)+1;
	else
	  Color = (int)(((float)(Color - Threshold) / (float)(MaxZ - Threshold))*15)+16;
	
	/* Just in case... I DON't want the GIF-saver to flip out! :) */
	if (Color < 1) Color=1;
	if (Color > 255) Color=31;
	WorldMapArray[row+i] = Color;
      }
      row += YRange;
    }

    /* "Recycle" Threshold variable, and, eh, the variable still has something
     * like the same meaning... :) */
    Threshold = PercentIce*XRange*YRange/100;

    if ((Threshold <= 0) || (Threshold > XRange*YRange)) goto Finished;

    FilledPixels = 0;
    /* i==y, j==x */
    for (i=0; i<YRange; i++)
    {
      for (j=0, row=0; j<XRange; j++)
      {
	Color = WorldMapArray[row+i];
	if (Color < 32) FloodFill4(j,i,Color);
	/* FilledPixels is a global variable which FloodFill4 modifies...
         * I know it's ugly, but as it is now, this is a hack! :)
         */
	if (FilledPixels > Threshold) goto NorthPoleFinished;
        row += YRange;
      }
    }
    
NorthPoleFinished:
    FilledPixels=0;
    /* i==y, j==x */
    for (i = (YRange - 1); i>0; i--)            /* fix */
    {
      for (j=0, row=0; j<XRange; j++)
      {
	Color = WorldMapArray[row+i];
	if (Color < 32) FloodFill4(j,i,Color);
	/* FilledPixels is a global variable which FloodFill4 modifies...
         * I know it's ugly, but as it is now, this is a hack! :)
         */
	if (FilledPixels > Threshold) goto Finished;
        row += YRange;
      }
    }
Finished:;
  }
  
  /* append .gif to SaveFile */
  sprintf(SaveFile, "%s.gif", SaveName);
  /* open binary SaveFile */
  Save = fopen(SaveFile, "wb");
  /* Write GIF to savefile */
  
  GIFEncode(Save, XRange, YRange, 1, 0, 8, Red, Green, Blue);
  
  fprintf(stderr, "Map created, saved as %s.\n", SaveFile);
  
  free(WorldMapArray);

  exit(0);
}

void FloodFill4(int x, int y, int OldColor)
{
  if (WorldMapArray[x*YRange+y] == OldColor)
  {
    if (WorldMapArray[x*YRange+y] < 16)
      WorldMapArray[x*YRange+y] = 32;
    else
      WorldMapArray[x*YRange+y] += 17;

    FilledPixels++;
    if (y-1 > 0)      FloodFill4(  x, y-1, OldColor);
    if (y+1 < YRange) FloodFill4(  x, y+1, OldColor);
    if (x-1 < 0)
      FloodFill4(XRange-1, y, OldColor);        /* fix */
    else
      FloodFill4(   x-1, y, OldColor);
    
    if (x+1 >= XRange)                          /* fix */
      FloodFill4(     0, y, OldColor);
    else
      FloodFill4(   x+1, y, OldColor);
  }
}

void GenerateWorldMap()
{
  float         Alpha, Beta;
  float         TanB;
  float         Result, Delta;
  int           i, row, N2;
  int           Theta, Phi, Xsi;
  unsigned int  flag1;


  /* I have to do this because of a bug in rand() in Solaris 1...
   * Here's what the man-page says:
   *
   * "The low bits of the numbers generated are not  very  random;
   *  use  the  middle  bits.  In particular the lowest bit alter-
   *  nates between 0 and 1."
   *
   * So I can't optimize this, but you might if you don't have the
   * same bug... */
  flag1 = rand() & 1; /*(int)((((float) rand())/MAX_RAND) + 0.5);*/
  
  /* Create a random greatcircle...
   * Start with an equator and rotate it */
  Alpha = (((float) rand())/MAX_RAND-0.5)*PI; /* Rotate around x-axis */
  Beta  = (((float) rand())/MAX_RAND-0.5)*PI; /* Rotate around y-axis */

  TanB  = tan(acos(cos(Alpha)*cos(Beta)));
  
  row  = 0;
  Xsi  = (int)(XRange/2-(XRange/PI)*Beta);
  
  for (Phi=0; Phi<XRange/2; Phi++)
  {
    Theta = (int)(YRangeDivPI*atan(*(SinIterPhi+Xsi-Phi+XRange)*TanB))+YRangeDiv2;

    if (flag1)
    {
      /* Rise northen hemisphere <=> lower southern */
      if (WorldMapArray[row+Theta] != INT_MIN)
	WorldMapArray[row+Theta]--;
      else
	WorldMapArray[row+Theta] = -1;
    }
    else
    {
      /* Rise southern hemisphere */
      if (WorldMapArray[row+Theta] != INT_MIN)
	WorldMapArray[row+Theta]++;
      else
	WorldMapArray[row+Theta] = 1;
    }
    row += YRange;
  }
}


/*****************************************************************************
 *
 * GIFENCODE.C    - GIF Image compression interface
 *
 * GIFEncode( FName, GHeight, GWidth, GInterlace, Background,
 *            BitsPerPixel, Red, Green, Blue, GetPixel )
 *
 *****************************************************************************/

#define TRUE 1
#define FALSE 0

static int Width, Height;
static int curx, cury;
static long CountDown;
static int Pass = 0;
static int Interlace;

/*
 * Bump the 'curx' and 'cury' to point to the next pixel
 */
static void
BumpPixel()
{
        /*
         * Bump the current X position
         */
        ++curx;

        /*
         * If we are at the end of a scan line, set curx back to the beginning
         * If we are interlaced, bump the cury to the appropriate spot,
         * otherwise, just increment it.
         */
        if( curx == Width ) {
                curx = 0;

                if( !Interlace )
                        ++cury;
                else {
                     switch( Pass ) {

                       case 0:
                          cury += 8;
                          if( cury >= Height ) {
                                ++Pass;
                                cury = 4;
                          }
                          break;

                       case 1:
                          cury += 8;
                          if( cury >= Height ) {
                                ++Pass;
                                cury = 2;
                          }
                          break;

                       case 2:
                          cury += 4;
                          if( cury >= Height ) {
                             ++Pass;
                             cury = 1;
                          }
                          break;

                       case 3:
                          cury += 2;
                          break;
                        }
                }
        }
}

/*
 * Return the next pixel from the image
 */
static int
GIFNextPixel( void )
{
        int r;

        if( CountDown == 0 )
                return EOF;

        --CountDown;

        r = WorldMapArray[ curx*YRange+cury ];

        BumpPixel();

        return r;
}

/* public */

static void
GIFEncode( fp, GWidth, GHeight, GInterlace, Background,
           BitsPerPixel, Red, Green, Blue)

FILE* fp;
int GWidth, GHeight;
int GInterlace;
int Background;
int BitsPerPixel;
int Red[], Green[], Blue[];
{
        int B;
        int RWidth, RHeight;
        int LeftOfs, TopOfs;
        int Resolution;
        int ColorMapSize;
        int InitCodeSize;
        int i;

        Interlace = GInterlace;

        ColorMapSize = 1 << BitsPerPixel;

        RWidth = Width = GWidth;
        RHeight = Height = GHeight;
        LeftOfs = TopOfs = 0;

        Resolution = BitsPerPixel;

        /*
         * Calculate number of bits we are expecting
         */
        CountDown = (long)Width * (long)Height;

        /*
         * Indicate which pass we are on (if interlace)
         */
        Pass = 0;

        /*
         * The initial code size
         */
        if( BitsPerPixel <= 1 )
                InitCodeSize = 2;
        else
                InitCodeSize = BitsPerPixel;

        /*
         * Set up the current x and y position
         */
        curx = cury = 0;

        /*
         * Write the Magic header
         */
        fwrite( "GIF87a", 1, 6, fp );

        /*
         * Write out the screen width and height
         */
        Putword( RWidth, fp );
        Putword( RHeight, fp );

        /*
         * Indicate that there is a global colour map
         */
        B = 0x80;       /* Yes, there is a color map */

        /*
         * OR in the resolution
         */
        B |= (Resolution - 1) << 5;

        /*
         * OR in the Bits per Pixel
         */
        B |= (BitsPerPixel - 1);

        /*
         * Write it out
         */
        fputc( B, fp );

        /*
         * Write out the Background colour
         */
        fputc( Background, fp );

        /*
         * Byte of 0's (future expansion)
         */
        fputc( 0, fp );

        /*
         * Write out the Global Colour Map
         */
        for( i=0; i<ColorMapSize; ++i ) {
                fputc( Red[i], fp );
                fputc( Green[i], fp );
                fputc( Blue[i], fp );
        }

        /*
         * Write an Image separator
         */
        fputc( ',', fp );

        /*
         * Write the Image header
         */

        Putword( LeftOfs, fp );
        Putword( TopOfs, fp );
        Putword( Width, fp );
        Putword( Height, fp );

        /*
         * Write out whether or not the image is interlaced
         */
        if( Interlace )
                fputc( 0x40, fp );
        else
                fputc( 0x00, fp );

        /*
         * Write out the initial code size
         */
        fputc( InitCodeSize, fp );

        /*
         * Go and actually compress the data
         */
        compress( InitCodeSize+1, fp);

        /*
         * Write out a Zero-length packet (to end the series)
         */
        fputc( 0, fp );

        /*
         * Write the GIF file terminator
         */
        fputc( ';', fp );

        /*
         * And close the file
         */
        fclose( fp );
}

/*
 * Write out a word to the GIF file
 */
static void
Putword( w, fp )
int w;
FILE* fp;
{
        fputc( w & 0xff, fp );
        fputc( (w / 256) & 0xff, fp );
}


/***************************************************************************
 *
 *  GIFCOMPR.C       - GIF Image compression routines
 *
 *  Lempel-Ziv compression based on 'compress'.  GIF modifications by
 *  David Rowley (mgardi@watdcsu.waterloo.edu)
 *
 ***************************************************************************/

/*
 * General DEFINEs
 */

#define BITS    12

#define HSIZE  5003            /* 80% occupancy */

#ifdef NO_UCHAR
 typedef char   char_type;
#else /*NO_UCHAR*/
 typedef        unsigned char   char_type;
#endif /*NO_UCHAR*/

/*
 *
 * GIF Image compression - modified 'compress'
 *
 * Based on: compress.c - File compression ala IEEE Computer, June 1984.
 *
 * By Authors:  Spencer W. Thomas       (decvax!harpo!utah-cs!utah-gr!thomas)
 *              Jim McKie               (decvax!mcvax!jim)
 *              Steve Davies            (decvax!vax135!petsd!peora!srd)
 *              Ken Turkowski           (decvax!decwrl!turtlevax!ken)
 *              James A. Woods          (decvax!ihnp4!ames!jaw)
 *              Joe Orost               (decvax!vax135!petsd!joe)
 *
 */
#include <ctype.h>

#define ARGVAL() (*++(*argv) || (--argc && *++argv))

static int n_bits;                        /* number of bits/code */
static int maxbits = BITS;                /* user settable max # bits/code */
static code_int maxcode;                  /* maximum code, given n_bits */
static code_int maxmaxcode = (code_int)1 << BITS; /* should NEVER generate this code */
#ifdef COMPATIBLE               /* But wrong! */
# define MAXCODE(n_bits)        ((code_int) 1 << (n_bits) - 1)
#else /*COMPATIBLE*/
# define MAXCODE(n_bits)        (((code_int) 1 << (n_bits)) - 1)
#endif /*COMPATIBLE*/

static count_int htab [HSIZE];
static unsigned short codetab [HSIZE];
#define HashTabOf(i)       htab[i]
#define CodeTabOf(i)    codetab[i]

static code_int hsize = HSIZE;                 /* for dynamic table sizing */

/*
 * To save much memory, we overlay the table used by compress() with those
 * used by decompress().  The tab_prefix table is the same size and type
 * as the codetab.  The tab_suffix table needs 2**BITS characters.  We
 * get this from the beginning of htab.  The output stack uses the rest
 * of htab, and contains characters.  There is plenty of room for any
 * possible stack (stack used to be 8000 characters).
 */

#define tab_prefixof(i) CodeTabOf(i)
#define tab_suffixof(i)        ((char_type*)(htab))[i]
#define de_stack               ((char_type*)&tab_suffixof((code_int)1<<BITS))

static code_int free_ent = 0;                  /* first unused entry */

/*
 * block compression parameters -- after all codes are used up,
 * and compression rate changes, start over.
 */
static int clear_flg = 0;

static int offset;
static long int in_count = 1;            /* length of input */
static long int out_count = 0;           /* # of codes output (for debugging) */

/*
 * compress stdin to stdout
 *
 * Algorithm:  use open addressing double hashing (no chaining) on the
 * prefix code / next character combination.  We do a variant of Knuth's
 * algorithm D (vol. 3, sec. 6.4) along with G. Knott's relatively-prime
 * secondary probe.  Here, the modular division first probe is gives way
 * to a faster exclusive-or manipulation.  Also do block compression with
 * an adaptive reset, whereby the code table is cleared when the compression
 * ratio decreases, but after the table fills.  The variable-length output
 * codes are re-sized at this point, and a special CLEAR code is generated
 * for the decompressor.  Late addition:  construct the table according to
 * file size for noticeable speed improvement on small files.  Please direct
 * questions about this implementation to ames!jaw.
 */

static int g_init_bits;
static FILE* g_outfile;

static int ClearCode;
static int EOFCode;

static void
compress( init_bits, outfile)
int init_bits;
FILE* outfile;
{
    register long fcode;
    register code_int i /* = 0 */;
    register int c;
    register code_int ent;
    register code_int disp;
    register code_int hsize_reg;
    register int hshift;

    /*
     * Set up the globals:  g_init_bits - initial number of bits
     *                      g_outfile   - pointer to output file
     */
    g_init_bits = init_bits;
    g_outfile = outfile;

    /*
     * Set up the necessary values
     */
    offset = 0;
    out_count = 0;
    clear_flg = 0;
    in_count = 1;
    maxcode = MAXCODE(n_bits = g_init_bits);

    ClearCode = (1 << (init_bits - 1));
    EOFCode = ClearCode + 1;
    free_ent = ClearCode + 2;

    char_init();

    ent = GIFNextPixel( );

    hshift = 0;
    for ( fcode = (long) hsize;  fcode < 65536L; fcode *= 2L )
        ++hshift;
    hshift = 8 - hshift;                /* set hash code range bound */

    hsize_reg = hsize;
    cl_hash( (count_int) hsize_reg);            /* clear hash table */

    output( (code_int)ClearCode );

#ifdef SIGNED_COMPARE_SLOW
    while ( (c = GIFNextPixel( )) != (unsigned) EOF ) {
#else /*SIGNED_COMPARE_SLOW*/
    while ( (c = GIFNextPixel( )) != EOF ) {	/* } */
#endif /*SIGNED_COMPARE_SLOW*/

        ++in_count;

        fcode = (long) (((long) c << maxbits) + ent);
        i = (((code_int)c << hshift) ^ ent);    /* xor hashing */

        if ( HashTabOf (i) == fcode ) {
            ent = CodeTabOf (i);
            continue;
        } else if ( (long)HashTabOf (i) < 0 )      /* empty slot */
            goto nomatch;
        disp = hsize_reg - i;           /* secondary hash (after G. Knott) */
        if ( i == 0 )
            disp = 1;
probe:
        if ( (i -= disp) < 0 )
            i += hsize_reg;

        if ( HashTabOf (i) == fcode ) {
            ent = CodeTabOf (i);
            continue;
        }
        if ( (long)HashTabOf (i) > 0 )
            goto probe;
nomatch:
        output ( (code_int) ent );
        ++out_count;
        ent = c;
#ifdef SIGNED_COMPARE_SLOW
        if ( (unsigned) free_ent < (unsigned) maxmaxcode) {
#else /*SIGNED_COMPARE_SLOW*/
        if ( free_ent < maxmaxcode ) {	/* } */
#endif /*SIGNED_COMPARE_SLOW*/
            CodeTabOf (i) = free_ent++; /* code -> hashtable */
            HashTabOf (i) = fcode;
        } else
                cl_block();
    }
    /*
     * Put out the final code.
     */
    output( (code_int)ent );
    ++out_count;
    output( (code_int) EOFCode );
}

/*****************************************************************
 * TAG( output )
 *
 * Output the given code.
 * Inputs:
 *      code:   A n_bits-bit integer.  If == -1, then EOF.  This assumes
 *              that n_bits =< (long)wordsize - 1.
 * Outputs:
 *      Outputs code to the file.
 * Assumptions:
 *      Chars are 8 bits long.
 * Algorithm:
 *      Maintain a BITS character long buffer (so that 8 codes will
 * fit in it exactly).  Use the VAX insv instruction to insert each
 * code in turn.  When the buffer fills up empty it and start over.
 */

static unsigned long cur_accum = 0;
static int cur_bits = 0;

static unsigned long masks[] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F,
                                  0x001F, 0x003F, 0x007F, 0x00FF,
                                  0x01FF, 0x03FF, 0x07FF, 0x0FFF,
                                  0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };

static void
output( code )
code_int  code;
{
    cur_accum &= masks[ cur_bits ];

    if( cur_bits > 0 )
        cur_accum |= ((long)code << cur_bits);
    else
        cur_accum = code;

    cur_bits += n_bits;

    while( cur_bits >= 8 ) {
        char_out( (unsigned int)(cur_accum & 0xff) );
        cur_accum >>= 8;
        cur_bits -= 8;
    }

    /*
     * If the next entry is going to be too big for the code size,
     * then increase it, if possible.
     */
   if ( free_ent > maxcode || clear_flg ) {

            if( clear_flg ) {

                maxcode = MAXCODE (n_bits = g_init_bits);
                clear_flg = 0;

            } else {

                ++n_bits;
                if ( n_bits == maxbits )
                    maxcode = maxmaxcode;
                else
                    maxcode = MAXCODE(n_bits);
            }
        }

    if( code == EOFCode ) {
        /*
         * At EOF, write the rest of the buffer.
         */
        while( cur_bits > 0 ) {
                char_out( (unsigned int)(cur_accum & 0xff) );
                cur_accum >>= 8;
                cur_bits -= 8;
        }

        flush_char();

        fflush( g_outfile );

        if( ferror( g_outfile ) )
                writeerr();
    }
}

/*
 * Clear out the hash table
 */
static void
cl_block ()             /* table clear for block compress */
{

        cl_hash ( (count_int) hsize );
        free_ent = ClearCode + 2;
        clear_flg = 1;

        output( (code_int)ClearCode );
}

static void
cl_hash(hsize)          /* reset code table */
register count_int hsize;
{

        register count_int *htab_p = htab+hsize;

        register long i;
        register long m1 = -1;

        i = hsize - 16;
        do {                            /* might use Sys V memset(3) here */
                *(htab_p-16) = m1;
                *(htab_p-15) = m1;
                *(htab_p-14) = m1;
                *(htab_p-13) = m1;
                *(htab_p-12) = m1;
                *(htab_p-11) = m1;
                *(htab_p-10) = m1;
                *(htab_p-9) = m1;
                *(htab_p-8) = m1;
                *(htab_p-7) = m1;
                *(htab_p-6) = m1;
                *(htab_p-5) = m1;
                *(htab_p-4) = m1;
                *(htab_p-3) = m1;
                *(htab_p-2) = m1;
                *(htab_p-1) = m1;
                htab_p -= 16;
        } while ((i -= 16) >= 0);

        for ( i += 16; i > 0; --i )
                *--htab_p = m1;
}

static void
writeerr()
{
        fprintf(stderr, "error writing output file" );
}

/******************************************************************************
 *
 * GIF Specific routines
 *
 ******************************************************************************/

/*
 * Number of characters so far in this 'packet'
 */
static int a_count;

/*
 * Set up the 'byte output' routine
 */
static void
char_init()
{
        a_count = 0;
}

/*
 * Define the storage for the packet accumulator
 */
static char accum[ 256 ];

/*
 * Add a character to the end of the current packet, and if it is 254
 * characters, flush the packet to disk.
 */
static void
char_out( c )
int c;
{
        accum[ a_count++ ] = c;
        if( a_count >= 254 )
                flush_char();
}

/*
 * Flush the packet to disk, and reset the accumulator
 */
static void
flush_char()
{
        if( a_count > 0 ) {
                fputc( a_count, g_outfile );
                fwrite( accum, 1, a_count, g_outfile );
                a_count = 0;
        }
}

/* The End */

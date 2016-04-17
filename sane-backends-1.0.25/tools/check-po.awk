#!/bin/awk -f 

#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License as
#  published by the Free Software Foundation; either version 2 of the
#  License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place - Suite 330, Boston,
#  MA 02111-1307, USA.


# This script will (hopefully!) check the completeness of a .po
# translation file. It will report untranslated strings, as well
# as fuzzy ones. It will print a summarry at the end of the check
# that says how many strings there are, how many are translated
# (and the percentage it represents), how many are fuzzy (and the
# percentage it represents amongst translated strings), and how
# many aree un-translated (and the percentage it represents).
# It will _not_ tell you wether your file is syntactically correct
# (eg. check for terminating double quotes!). And of course it
# will _not_ tell you wether the translations are correct! ;-]
#
# It was originaly been written for SANE backends translations, but
# shall be able to check any .po file.
#
# Originally writen by Yann E. MORIN 
# <yann dot morin dot 1998 at anciens dot enib dot fr>
#
# Output will look like :
# [./src/foobar.c:2345](321)- "This is the string"
#  \____________/ \__/  \_/ |  \________________/
#   |              |     |  |   |
#   |              |     |  |   \-> Original untranslated string
#   |              |     |  |
#   |              |     |  \-> flag telling wether it is
#   |              |     |      fuzzy (F) or not (-)
#   |              |     |
#   |              |     \-> line number in the .po file
#   |              |
#   |              \-> line number in the source file
#   |
#   \-> filename where the original string lies
#
#
# Last four lines will look like :
# Translated     :   23 (23.0%)
#       of which :    2 fuzzy ( 8.6%)
# Not translated :   77 (77.0%)
# Total          :  100
#
#
# TODO:
#   - Print the fuzzy translated string at the same level as the
#     untranslated one;
#   - basic checks about syntax (missing terminating double quotes);
#   - option for brief mode (only last four lines);
#   - other?


BEGIN \
{
  count = 0;
  fuzzy = 0;
  is_fuzzy = 0;
  missing = 0;
  first = 1;
}

# Is this translation fuzzy? If so count it
$1 == "#," && $2 ~ /^fuzzy(|,)$/ \
{
  fuzzy++;
  # Next translation will be fuzzy!
  is_fuzzy = 1;
}

$1 == "#:" \
{
  file = $2;
}

# Skip the first msgid as it is no true translation
$1 ~ /msgid/ && first == 1 \
{
  first = 0;
  next;
}

$1 ~ /msgid/ && first == 0 \
{
  # One more translation
  count++;
  line = NR;

  # Gets the untranslated string (with double quotes :-( )
  $1 = "";
  original = $0;
  getline;
  while( $1 != "msgstr" )
  {
    original = original $0
    getline;
  }

  # Now extract the translated string (with double quotes as well :-( )
  $1 = "";
  translation = $0;
  # In case  we have no blank line after the last translation (EOF),
  # we need to stop this silly loop. Allowing a 10-line message.
  len = 10;
  getline;
  while( $0 != "" && $0 !~ /^#/ && len != 0 )
  {
    translation = translation $0
    getline;
    len--;
  }

  # Remove double quotes from multi-line messages and translations
  msg = ""
  n = split( original, a, "\"" );
  # start at 2 to get rid of the preceding space
  for( i=2; i<=n; i++ )
  {
    msg = msg a[i];
  }
  trans = "";
  n = split( translation, a, "\"" );
  # start at 2 to get rid of the preceding space
  for( i=2; i<=n; i++ )
  {
    trans = trans a[i]
  }

  # Checks wether we have a translation or not, wether it is fuzzy or not
  if( ( trans == "" ) || ( is_fuzzy == 1 ) )
  {
    # Enclose original messages between double quotes
    printf( "[%s](%d)", file, line );
    if( is_fuzzy == 1 )
    {
      printf( "F" );
    }
    else
    {
      printf( "-" );
    }
    printf( " \"%s\"\n", msg );
    if( trans == "" )
    {
      missing++;
    }
  }

  is_fuzzy = 0;
}

END \
{
  # Lines are longer than 80 chars, but I won't cut them
  printf( "\n" );
  printf( "Translated     : %4d (%4.1f%%)\n", count-missing, 100.0*(count-missing)/count );
  printf( "      of which : %4d fuzzy (%4.1f%%)\n", fuzzy, 100*fuzzy/(count-missing) );
  printf( "Not translated : %4d (%4.1f%%)\n", missing, 100.0*missing/count );
  printf( "Total          : %4d\n", count );
}

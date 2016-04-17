/*
 * Worldwide channel/frequency list
 *
 * Nathan Laredo (laredo@broked.net)
 *
 * Frequencies are given in kHz 
 */
#define NTSC_AUDIO_CARRIER	4500
#define PAL_AUDIO_CARRIER_I	6000
#define PAL_AUDIO_CARRIER_BGHN	5500
#define PAL_AUDIO_CARRIER_MN	4500
#define PAL_AUDIO_CARRIER_D	6500
#define SEACAM_AUDIO_DKK1L	6500
#define SEACAM_AUDIO_BG		5500
/* NICAM 728 32-kHz, 14-bit digital stereo audio is transmitted in 1ms frames
   containing 8 bits frame sync, 5 bits control, 11 bits additional data, and
   704 bits audio data.  The bit rate is reduced by transmitting only 10 bits
   plus parity of each 14 bit sample, the largest sample in a frame determines
   which 10 bits are transmitted.  The parity bits for audio samples also 
   specify the scaling factor used for that channel during that frame.  The
   companeded audio data is interleaved to reduce the influence of dropouts
   and the whole frame except for sync bits is scrambled for spectrum shaping.
   Data is modulated using QPSK, at below following subcarrier freqs */
#define NICAM728_PAL_BGH	5850
#define NICAM728_PAL_I		6552

/* COMPREHENSIVE LIST OF FORMAT BY COUNTRY
   (M) NTSC used in:
	Antigua, Aruba, Bahamas, Barbados, Belize, Bermuda, Bolivia, Burma,
	Canada, Chile, Colombia, Costa Rica, Cuba, Curacao, Dominican Republic,
	Ecuador, El Salvador, Guam Guatemala, Honduras, Jamaica, Japan,
	South Korea, Mexico, Montserrat, Myanmar, Nicaragua, Panama, Peru,
	Philippines, Puerto Rico, St Christopher and Nevis, Samoa, Suriname,
	Taiwan, Trinidad/Tobago, United States, Venezuela, Virgin Islands
   (B) PAL used in:
	Albania, Algeria, Australia, Austria, Bahrain, Bangladesh, Belgium,
	Bosnia-Herzegovinia, Brunei Darussalam, Cambodia, Cameroon, Croatia,
	Cyprus, Denmark, Egypt, Ethiopia, Equatorial Guinea, Finland, Germany,
	Ghana, Gibraltar, Greenland, Iceland, India, Indonesia, Israel, Italy,
	Jordan, Kenya, Kuwait, Liberia, Libya, Luxembourg, Malaysa, Maldives,
	Malta, Nepal, Netherlands, New Zeland, Nigeria, Norway, Oman, Pakistan,
	Papua New Guinea, Portugal, Qatar, Sao Tome and Principe, Saudi Arabia,
	Seychelles, Sierra Leone, Singapore, Slovenia, Somali, Spain,
	Sri Lanka, Sudan, Swaziland, Sweden, Switzeland, Syria, Thailand,
	Tunisia, Turkey, Uganda, United Arab Emirates, Yemen
   (N) PAL used in: (Combination N = 4.5MHz audio carrier, 3.58MHz burst)
	Argentina (Combination N), Paraguay, Uruguay
   (M) PAL (525/60, 3.57MHz burst) used in:
	Brazil
   (G) PAL used in:
	Albania, Algeria, Austria, Bahrain, Bosnia/Herzegovinia, Cambodia,
	Cameroon, Croatia, Cyprus, Denmark, Egypt, Ethiopia, Equatorial Guinea,
	Finland, Germany, Gibraltar, Greenland, Iceland, Israel, Italy, Jordan,
	Kenya, Kuwait, Liberia, Libya, Luxembourg, Malaysia, Monaco,
	Mozambique, Netherlands, New Zealand, Norway, Oman, Pakistan,
	Papa New Guinea, Portugal, Qatar, Romania, Sierra Leone, Singapore,
	Slovenia, Somalia, Spain, Sri Lanka, Sudan, Swaziland, Sweeden,
	Switzerland, Syria, Thailand, Tunisia, Turkey, United Arab Emirates,
	Yemen, Zambia, Zimbabwe
   (D) PAL used in:
	China, North Korea, Romania, Czech Republic
   (H) PAL used in:
	Belgium
   (I) PAL used in:
	Angola, Botswana, Gambia, Guinea-Bissau, Hong Kong, Ireland, Lesotho,
	Malawi, Nambia, Nigeria, South Africa, Tanzania, United Kingdom,
	Zanzibar
   (B) SECAM used in:
	Djibouti, Greece, Iran, Iraq, Lebanon, Mali, Mauritania, Mauritus,
	Morocco
   (D) SECAM used in:
	Afghanistan, Armenia, Azerbaijan, Belarus, Bulgaria,
	Estonia, Georgia, Hungary, Zazakhstan, Lithuania, Mongolia, Moldova,
	Poland, Russia, Slovak Republic, Ukraine, Vietnam
   (G) SECAM used in:
	Greecem Iran, Iraq, Mali, Mauritus, Morocco, Saudi Arabia
   (K) SECAM used in:
	Armenia, Azerbaijan, Bulgaria, Estonia, Georgia,
	Hungary, Kazakhstan, Lithuania, Madagascar, Moldova, Poland, Russia,
	Slovak Republic, Ukraine, Vietnam
   (K1) SECAM used in:
	Benin, Burkina Faso, Burundi, Chad, Cape Verde, Central African
	Republic, Comoros, Congo, Gabon, Madagascar, Niger, Rwanda, Senegal,
	Togo, Zaire
   (L) SECAM used in:
	France
*/

/* --------------------------------------------------------------------- */

struct CHANLIST
{
  char *name;
  int freq;
};

struct CHANLISTS
{
  char *name;
  struct CHANLIST *list;
  int count;
};

#define CHAN_COUNT(x) (sizeof(x)/sizeof(struct CHANLIST))

/* --------------------------------------------------------------------- */

extern struct CHANLISTS chanlists[];
extern struct STRTAB chanlist_names[];

extern int chantab;
extern struct CHANLIST *chanlist;
extern int chancount;

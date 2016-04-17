/* sane - Scanner Access Now Easy.

   Copyright (C) 2003 Oliver Rauch
   Copyright (C) 2003-2005 Henning Meier-Geinitz <henning@meier-geinitz.de>
   Copyright (C) 2004, 2005 Gerhard Jaeger <gerhard@gjaeger.de>
   Copyright (C) 2004-2013 Stéphane Voltz <stef.dev@free.fr>
   Copyright (C) 2005-2009 Pierre Willenbrock <pierre@pirsoft.dnsalias.org>
   Copyright (C) 2007 Luke <iceyfor@gmail.com>
   Copyright (C) 2010 Jack McGill <jmcgill85258@yahoo.com>
   Copyright (C) 2010 Andrey Loginov <avloginov@gmail.com>,
   		xerox travelscan device entry
   Copyright (C) 2010 Chris Berry <s0457957@sms.ed.ac.uk> and Michael Rickmann <mrickma@gwdg.de>
                 for Plustek Opticbook 3600 support

   This file is part of the SANE package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston,
   MA 02111-1307, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/

/* ------------------------------------------------------------------------ */
/*                     Some setup DAC and CCD tables                        */
/* ------------------------------------------------------------------------ */

/** Setup table for various scanners using a Wolfson DAC
 */
static Genesys_Frontend Wolfson[] = {
  { DAC_WOLFSON_UMAX, {0x00, 0x03, 0x05, 0x11}
   , {0x00, 0x00, 0x00}
   , {0x80, 0x80, 0x80}
   , {0x02, 0x02, 0x02}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 0: UMAX */
  {DAC_WOLFSON_ST12, {0x00, 0x03, 0x05, 0x03}
   , {0x00, 0x00, 0x00}
   , {0xc8, 0xc8, 0xc8}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 1: ST12 */
  {DAC_WOLFSON_ST24,{0x00, 0x03, 0x05, 0x21}
   , {0x00, 0x00, 0x00}
   , {0xc8, 0xc8, 0xc8}
   , {0x06, 0x06, 0x06}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 2: ST24 */
  {DAC_WOLFSON_5345,{0x00, 0x03, 0x05, 0x12}
   , {0x00, 0x00, 0x00}
   , {0xb8, 0xb8, 0xb8}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 3: MD6228/MD6471 */
  {DAC_WOLFSON_HP2400,
   /* reg0  reg1  reg2  reg3 */
     {0x00, 0x03, 0x05, 0x02} /* reg3=0x02 for 50-600 dpi, 0x32 (0x12 also works well) at 1200 */
   , {0x00, 0x00, 0x00}
   , {0xb4, 0xb6, 0xbc}
   , {0x06, 0x09, 0x08}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 4: HP2400c */
  {DAC_WOLFSON_HP2300,
     {0x00, 0x03, 0x04, 0x02}
   , {0x00, 0x00, 0x00}
   , {0xbe, 0xbe, 0xbe}
   , {0x04, 0x04, 0x04}
   , {0x00, 0x00, 0x00}
   }
  ,				/* 5: HP2300c */
  {DAC_CANONLIDE35,{0x00, 0x3d, 0x08, 0x00}
   , {0x00, 0x00, 0x00}
   , {0xe1, 0xe1, 0xe1}
   , {0x93, 0x93, 0x93}
   , {0x00, 0x19, 0x06}
   }
  ,				/* 6: CANONLIDE35 */
  {DAC_AD_XP200,
     {0x58, 0x80, 0x00, 0x00}	/* reg1=0x80 ? */
   , {0x00, 0x00, 0x00}
   , {0x09, 0x09, 0x09}
   , {0x09, 0x09, 0x09}
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_WOLFSON_XP300,{0x00, 0x35, 0x20, 0x14}  /* 7: XP300 */
   , {0x00, 0x00, 0x00}
   , {0xe1, 0xe1, 0xe1}
   , {0x93, 0x93, 0x93}
   , {0x07, 0x00, 0x00}
   }
  ,				/* 8: HP3670 */
  {DAC_WOLFSON_HP3670,
   /* reg0  reg1  reg2  reg3 */
     {0x00, 0x03, 0x05, 0x32} /* reg3=0x32 for 100-300 dpi, 0x12 at 1200 */
   , {0x00, 0x00, 0x00} /* sign */
   , {0xba, 0xb8, 0xb8} /* offset */
   , {0x06, 0x05, 0x04} /* gain 4,3,2 at 1200 ?*/
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_WOLFSON_DSM600,{0x00, 0x35, 0x20, 0x14}  /* 9: DSMOBILE600 */
   , {0x00, 0x00, 0x00}
   , {0x85, 0x85, 0x85}
   , {0xa0, 0xa0, 0xa0}
   , {0x07, 0x00, 0x00}
   }
  ,
  {DAC_CANONLIDE200,
     {0x9d, 0x91, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x3f, 0x00}  /* 0x00 0x3f 0x00 : offset/brigthness ? */
   , {0x32, 0x04, 0x00}
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_CANONLIDE700,
     {0x9d, 0x9e, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x3f, 0x00}  /* 0x00 0x3f 0x00 : offset/brigthness ? */
   , {0x2f, 0x04, 0x00}
   , {0x00, 0x00, 0x00}
   }
  ,				/* KV-SS080 */
  {DAC_KVSS080,
     {0x00, 0x23, 0x24, 0x0f}
   , {0x00, 0x00, 0x00}
   , {0x80, 0x80, 0x80}
   , {0x4b, 0x4b, 0x4b}
   , {0x00,0x00,0x00}
   }
  ,
  {DAC_G4050,
     {0x00, 0x23, 0x24, 0x1f}
   , {0x00, 0x00, 0x00}
   , {0x45, 0x45, 0x45}	/* 0x20, 0x21, 0x22 */
   , {0x4b, 0x4b, 0x4b} /* 0x28, 0x29, 0x2a */
   , {0x00,0x00,0x00}
   }
  ,
  {DAC_CANONLIDE110,
     {0x80, 0x8a, 0x23, 0x4c}
   , {0x00, 0xca, 0x94}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_PLUSTEK_3600,
     {0x70, 0x80, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x3f, 0x3d, 0x3d}
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_CS8400F,
     {0x00, 0x23, 0x24, 0x0f}
   , {0x00, 0x00, 0x00}
   , {0x60, 0x5c, 0x6c}	/* 0x20, 0x21, 0x22 */
   , {0x8a, 0x9f, 0xc2} /* 0x28, 0x29, 0x2a */
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_IMG101,
     {0x78, 0xf0, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}	/* 0x20, 0x21, 0x22 */
   , {0x00, 0x00, 0x00} /* 0x28, 0x29, 0x2a */
   , {0x00, 0x00, 0x00}
   }
  ,
  {DAC_PLUSTEK3800,
     {0x78, 0xf0, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}	/* 0x20, 0x21, 0x22 */
   , {0x00, 0x00, 0x00} /* 0x28, 0x29, 0x2a */
   , {0x00, 0x00, 0x00}
   },
  {DAC_CANONLIDE80,
     /* reg0: control 74 data, 70 no data
      * reg3: offset
      * reg6: gain
      * reg0 , reg3, reg6 */
     {0x70, 0x16, 0x60, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
   , {0x00, 0x00, 0x00}
  }
};


/** for setting up the sensor-specific settings:
 * Optical Resolution, number of black pixels, number of dummy pixels,
 * CCD_start_xoffset, and overall number of sensor pixels
 * registers 0x08-0x0b, 0x10-0x1d and 0x52-0x5e
 */
static Genesys_Sensor Sensor[] = {
  /* 0: UMAX */
  {CCD_UMAX,
   1200, 48, 64, 0, 10800, 210, 230,
   {0x01, 0x03, 0x05, 0x07}
   ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x05, 0x31, 0x2a, 0x00, 0x00,
    0x00, 0x02}
   ,
   {0x13, 0x17, 0x03, 0x07, 0x0b, 0x0f, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00,
    0x00}
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}}
  ,
  /* 1: Plustek OpticPro S12/ST12 */
  {CCD_ST12,
   600, 48, 85, 152, 5416, 210, 230,
   {0x02, 0x00, 0x06, 0x04} ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2b, 0x08, 0x20, 0x2a, 0x00, 0x00, 0x0c, 0x03} ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* 2: Plustek OpticPro S24/ST24 */
  {CCD_ST24,
   1200,
   48, 64, 0, 10800, 210, 230,
   {0x0e, 0x0c, 0x00, 0x0c} ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x08, 0x31, 0x2a, 0x00, 0x00, 0x00, 0x02} ,
   {0x17, 0x03, 0x07, 0x0b, 0x0f, 0x13, 0x03, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* 3: MD6471 */
  {CCD_5345,
   1200,
   48,
   16, 0, 10872,
   190, 190,
   {0x0d, 0x0f, 0x11, 0x13} ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x0a, 0x30, 0x2a, 0x00, 0x00, 0x00, 0x03} ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x23, 0x00, 0xc1, 0x00, 0x00, 0x00, 0x00} ,
   {2.38, 2.35, 2.34},
   {NULL, NULL, NULL}
  }
  ,
  /* 4: HP2400c */
  {CCD_HP2400,
   1200,
   48,
   15, 0, 10872, 210, 200,
   {0x14, 0x15, 0x00, 0x00} /* registers 0x08-0x0b */ ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xbf, 0x08, 0x3f, 0x2a, 0x00, 0x00, 0x00, 0x02} ,
   {0x0b, 0x0f, 0x13, 0x17, 0x03, 0x07, 0x63, 0x00, 0xc1, 0x00, 0x0e, 0x00, 0x00} ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}
  }
  ,
  /* 5: HP2300c */
  {CCD_HP2300,
   600,
   48,
   20, 0, 5368, 180, 180,	/* 5376 */
   {0x16, 0x00, 0x01, 0x03} ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xb7, 0x0a, 0x20, 0x2a, 0x6a, 0x8a, 0x00, 0x05} ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x06, 0x0b, 0x10, 0x16} ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}
  }
  ,
  /* CANOLIDE35 */
  {CCD_CANONLIDE35,
   1200,
   87,				/* (black) */
   87,				/* (dummy) */
   0,				/* (startxoffset) */
   10400,			/* sensor_pixels */
   0,
   0,
   {0x00, 0x00, 0x00, 0x00},
   {0x04, 0x00, 0x04, 0x00, 0x04, 0x00, 0x00, 0x02, 0x00, 0x50,
    0x00, 0x00, 0x00, 0x02	/* TODO(these do no harm, but may be neccessery for CCD) */
    },
   {0x05, 0x07,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x3a, 0x03,
    0x40,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x00	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* 7: Strobe XP200 */
  {CIS_XP200, 600,
   5,
   38, 0, 5200, 200, 200, /* 5125 */
   {0x16, 0x00, 0x01, 0x03} ,
   {0x14, 0x50, 0x0c, 0x80, 0x0a, 0x28, 0xb7, 0x0a, 0x20, 0x2a, 0x6a, 0x8a, 0x00, 0x05} ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0xc1, 0x06, 0x0b, 0x10, 0x16} ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}
   }
  ,
  /* HP3670 */
  {CCD_HP3670,1200,
   48,
   16, 0, 10872,
   210, 200,
   {0x00, 0x0a, 0x0b, 0x0d} ,
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x07, 0x20, 0x2a, 0x00, 0x00, 0xc0, 0x43} ,
   {0x0f, 0x13, 0x17, 0x03, 0x07, 0x0b, 0x83, 0x00, 0x15, 0x05, 0x0a, 0x0f, 0x00},
   {1.00, 1.00, 1.00},
   {NULL, NULL, NULL}
  }
  ,
  /* Syscan DP 665 */
  {CCD_DP665, 600,
   27,				/*(black) */
   27,				/* (dummy) */
   0,				/* (startxoffset) */
   2496,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x00, 0x02, 0x04, 0x50,
    0x10, 0x00, 0x20, 0x02
    },
   {0x04, 0x05,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x54, 0x03,
    0x00,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x01	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* Visioneer Roadwarrior */
  {CCD_ROADWARRIOR, 600,
   27,				/*(black) */
   27,				/* (dummy) */
   0,				/* (startxoffset) */
   5200,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x00, 0x02, 0x04, 0x50,
    0x10, 0x00, 0x20, 0x02
    },
   {0x04, 0x05,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x54, 0x03,
    0x00,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x01	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
  {1.0, 1.0, 1.0},
  {NULL, NULL, NULL}
  }
,
  /* Pentax DS Mobile 600 */
  {CCD_DSMOBILE600, 600,
   28,				/*(black) */
   28,				/* (dummy) */
   0,				/* (startxoffset) */
   5200,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x15, 0x44, 0x15, 0x44, 0x15, 0x44, 0x00, 0x02, 0x04, 0x50,
    0x10, 0x00, 0x20, 0x02
    },
   {0x04, 0x05,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x54, 0x03,
    0x00,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x01	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* 13: Strobe XP300 */
  {CCD_XP300, 600,
   27,				/*(black) */
   27,				/* (dummy) */
   0,				/* (startxoffset) */
   10240,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x00, 0x02, 0x04, 0x50,
    0x10, 0x00, 0x20, 0x02
    },
   {0x04, 0x05,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x54, 0x03,
    0x00,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x01	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,
  /* 13: Strobe XP300 */
  {CCD_DP685, 600,
   27,				/*(black) */
   27,				/* (dummy) */
   0,				/* (startxoffset) */
   5020,			/*sensor_pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   {0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x00, 0x02, 0x04, 0x50,
    0x10, 0x00, 0x20, 0x02
    },
   {0x04, 0x05,
    0x00, 0x00, 0x00, 0x00,	/*[GB](HI|LOW) not needed for cis */
    0x54, 0x03,
    0x00,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x01	/*TODO (these do no harm, but may be neccessery for CCD) */
    }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
   }
  ,
  /* CANONLIDE200 */
  {CIS_CANONLIDE200,
   4800,	/* optical resolution */
   87*4,	/* black pixels */
   16*4,	/* dummy pixels */
   320*8,	/* CCD_startx_offset 323 */
   5136*8,
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x1d */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* EXPR/EXPG/EXPB */
    0x10, 0x08, 0x00, 0xff, 0x34, 0x00, 0x02, 0x04 },
   /* reg 0x52 - 0x5e */
   {0x03, 0x07,
    0x00, 0x00, 0x00, 0x00,
    0x2a, 0xe1,
    0x55,
    0x00, 0x00, 0x00,
    0x41
    }
   ,
   {1.7, 1.7, 1.7},
   {NULL, NULL, NULL}
   }
  ,
  /* CANONLIDE700 */
  {CIS_CANONLIDE700,
   4800,	/* optical resolution */
   73*8,	/* black pixels 73 at 600 dpi */
   16*8,	/* dummy pixels */
   384*8,	/* CCD_startx_offset 384 at 600 dpi */
   5188*8,	/* 8x5570 segments , 5187+1 for rounding */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x1d */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* EXPR/EXPG/EXPB */
    0x10, 0x08, 0x00, 0xff, 0x34, 0x00, 0x02, 0x04 },
   /* reg 0x52 - 0x5e */
   {0x07, 0x03,
    0x00, 0x00, 0x00, 0x00,
    0x2a, 0xe1,
    0x55,
    0x00, 0x00, 0x00,
    0x41
    }
   ,
  {1.0, 1.0, 1.0},
  {NULL, NULL, NULL}
  }
  ,
  /* CANONLIDE100 */
  {CIS_CANONLIDE100,
   2400,	/* optical resolution */
   87*4,	/* black pixels */
   16*4,	/* dummy pixels 16 */
   320*4,	/* 323 */
   5136*4,      /* 10272 */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x15 */
   {0x01, 0xc1, 0x01, 0x26, 0x00, 0xe5, /* EXPR/EXPG/EXPB */
   /* reg 0x16 - 0x1d 0x19=0x50*/
    0x10, 0x08, 0x00, 0x50, 0x34, 0x00, 0x02, 0x04 },
   /* reg 0x52 - 0x5e */
   {0x03, 0x07,
    0x00, 0x00, 0x00, 0x00,
    0x2a, 0xe1,
    0x55,
    0x00, 0x00, 0x00,
    0x41
    }
   ,
   {1.7, 1.7, 1.7},
   {NULL, NULL, NULL}
  }
  ,
  {CCD_KVSS080,
   600,
   38, /* black pixels on left */
   38, /* 36 dummy pixels */
   152,
   5376, /* 5100-> 5200 */
   160, /* TAU white ref */
   160, /* gain white ref */
   /* 08    09    0a    0b */
   {0x00, 0x00, 0x00, 0x6a} ,
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x1c, 0x00, 0x2a, 0x2c, 0x00, 0x20, 0x04} , /* 18=00 at 600 dpi */
   /* 52    53    54    55    56    57    58    59   5a    5b     5c    5d    5e */
   {0x0c, 0x0f, 0x00, 0x03, 0x06, 0x09, 0x6b, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x23} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
   }
  ,
  {CCD_G4050,
   4800,
   50*8,	/* black_pixels */
   58,	        /* 31 at 600 dpi dummy_pixels 58 at 1200 */
   152,
   5360*8,      /* 5360 max at 600 dpi */
   160,
   160,
   /* 08    09    0a    0b */
   {0x00, 0x00, 0x18, 0x69} ,
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d */
   {0x2c, 0x09, 0x22, 0xb8, 0x10, 0xf0, 0x33, 0x0c, 0x00, 0x2a, 0x30, 0x00, 0x00, 0x08} ,
   /* 52    53    54    55    56    57    58    59   5a    5b     5c    5d    5e */
   {0x0b, 0x0e, 0x11, 0x02, 0x05, 0x08, 0x63, 0x00, 0x40, 0x00, 0x00, 0x00, 0x6f} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
   }
  ,
  {CCD_CS4400F,
   4800,
   50*8,	/* black_pixels */
   20,	        /* 31 at 600 dpi dummy_pixels 58 at 1200 */
   152,
   5360*8,      /* 5360 max at 600 dpi */
   160,
   160,
   /* 08    09    0a    0b */
   {0x00, 0x00, 0x18, 0x69} ,
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d */
   {0x9c, 0x40, 0x9c, 0x40, 0x9c, 0x40, 0x13, 0x0a, 0x10, 0x2a, 0x30, 0x00, 0x00, 0x6b},
   /* 52    53    54    55    56    57    58    59   5a    5b     5c    5d    5e */
   {0x0a, 0x0d, 0x00, 0x03, 0x06, 0x08, 0x5b, 0x00, 0x40, 0x00, 0x00, 0x00, 0x3f},
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
   }
  ,
  {CCD_CS8400F,
   4800,
   50*8,	/* black_pixels */
   20,	        /* 31 at 600 dpi dummy_pixels 58 at 1200 */
   152,
   5360*8,      /* 5360 max at 600 dpi */
   160,
   160,
   /* 08    09    0a    0b */
   {0x00, 0x00, 0x18, 0x69} ,
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d */
   {0x9c, 0x40, 0x9c, 0x40, 0x9c, 0x40, 0x13, 0x0a, 0x10, 0x2a, 0x30, 0x00, 0x00, 0x6b},
   /* 52    53    54    55    56    57    58    59   5a    5b     5c    5d    5e */
   {0x0a, 0x0d, 0x00, 0x03, 0x06, 0x08, 0x5b, 0x00, 0x40, 0x00, 0x00, 0x00, 0x3f},
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
   }
  ,
  /* HP N6310 */
  {CCD_HP_N6310,
   2400,
   96,
   26,
   128,
   42720,
   210,
   230,
   /* 08    09    0a    0b */
   {0x00, 0x10, 0x10, 0x0c} ,
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x0c, 0x02, 0x2a, 0x30, 0x00, 0x00, 0x08} ,
   /* 52    53    54    55    56    57    58    59   5a    5b     5c    5d    5e */
   {0x0b, 0x0e, 0x11, 0x02, 0x05, 0x08, 0x63, 0x00, 0x40, 0x00, 0x00, 0x06, 0x6f} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}
  }
  ,

  /* CANONLIDE110 */
  {CIS_CANONLIDE110,
   2400,	/* optical resolution */
   87,		/* black pixels */
   16,		/* dummy pixels 16 */
   303,		/* 303 */
   5168*4,	/* total pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x15 : EXPR, EXPG and EXPB */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   /* reg 0x16 - 0x1d */
    0x10, 0x04, 0x00, 0x01, 0x30, 0x00, 0x02, 0x01 },
   /* reg 0x52 - 0x5e */
   {
    0x00, 0x02, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04,
    0x1a, 0x00, 0xc0, 0x00, 0x00
    }
   ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}}
  ,

  /* CANONLIDE120 */
  {CIS_CANONLIDE120,
   2400,	/* optical resolution */
   87,		/* black pixels */
   16,		/* dummy pixels 16 */
   303,		/* 303 */
   5168*4,	/* total pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x15 : EXPR, EXPG and EXPB */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   /* reg 0x16 - 0x1d */
    0x10, 0x04, 0x00, 0x01, 0x30, 0x00, 0x02, 0x01 },
   /* reg 0x52 - 0x5e */
   {
    0x00, 0x02, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04,
    0x1a, 0x00, 0xc0, 0x00, 0x00
    }
   ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}}
  ,
  /* CANON LIDE 210 sensor */
  {CIS_CANONLIDE210,
   2400,	/* optical resolution */
   87,		/* black pixels */
   16,		/* dummy pixels 16 */
   303,		/* 303 */
   5168*4,	/* total pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x15 : EXPR, EXPG and EXPB */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   /* reg 0x16 - 0x1d */
    0x10, 0x04, 0x00, 0x01, 0x30, 0x00, 0x02, 0x01 },
   /* reg 0x52 - 0x5e */
   {
    0x00, 0x02, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04,
    0x1a, 0x00, 0xc0, 0x00, 0x00
    }
   ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}}
  ,
  /* CANON LIDE 220 sensor */
  {CIS_CANONLIDE220,
   2400,	/* optical resolution */
   87,		/* black pixels */
   16,		/* dummy pixels 16 */
   303,		/* 303 */
   5168*4,	/* total pixels */
   210,
   200,
   {0x00, 0x00, 0x00, 0x00},
   /* reg 0x10 - 0x15 : EXPR, EXPG and EXPB */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   /* reg 0x16 - 0x1d */
    0x10, 0x04, 0x00, 0x01, 0x30, 0x00, 0x02, 0x01 },
   /* reg 0x52 - 0x5e */
   {
    0x00, 0x02, 0x04, 0x06, 0x04, 0x04, 0x04, 0x04,
    0x1a, 0x00, 0xc0, 0x00, 0x00
    }
   ,
   {2.1, 2.1, 2.1},
   {NULL, NULL, NULL}}
  ,
  {CCD_PLUSTEK_3600,
   1200,
   87,				/*(black) */
   87,				/* (dummy) */
   0,				/* (startxoffset) */
   10100,			/*sensor_pixels */
   210,
   230,
   {0x00, 0x00, 0x00, 0x00},
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x33, 0x0b, 0x11, 0x2a,
    0x00, 0x00, 0x00, 0xc4	/* TODO(these do no harm, but may be neccessery for CCD) */
   },
   {0x07, 0x0a,
    0x0c, 0x00, 0x02, 0x06,	/*[GB](HI|LOW) not needed for cis */
    0x22, 0x69,
    0x40,			/*TODO: bit7 */
    0x00, 0x00, 0x00, 0x02	/*TODO (these do no harm, but may be neccessery for CCD) */
   }
   ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}}
  ,
  /* Canon Image formula 101 */
  {CCD_IMG101,
   1200,	/* optical resolution */
   31,
   31,
   0,
   10800,
   210,
   200,
   {0x60, 0x00, 0x00, 0x8b},
   /* reg 0x10 - 0x15 */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* EXPR/EXPG/EXPB */
   /* reg 0x16 - 0x1d 0x19=0x50*/
    0xbb, 0x13, 0x10, 0x2a, 0x34, 0x00, 0x20, 0x06 },
   /* reg 0x52 - 0x5e */
   {0x02, 0x04,
    0x06, 0x08, 0x0a, 0x00,
    0x59, 0x31,
    0x40,
    0x00, 0x00, 0x00,
    0x1f
    }
   ,
   {1.7, 1.7, 1.7},
   {NULL, NULL, NULL}
  }
  ,
  /* Plustek OpticBook 3800 */
  {CCD_PLUSTEK3800,
   1200,	/* optical resolution */
   31,
   31,
   0,
   10200,
   210,
   200,
   {0x60, 0x00, 0x00, 0x8b},
   /* reg 0x10 - 0x15 */
   {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* EXPR/EXPG/EXPB */
   /* reg 0x16 - 0x1d 0x19=0x50*/
    0xbb, 0x13, 0x10, 0x2a, 0x34, 0x00, 0x20, 0x06 },
   /* reg 0x52 - 0x5e */
   {0x02, 0x04,
    0x06, 0x08, 0x0a, 0x00,
    0x59, 0x31,
    0x40,
    0x00, 0x00, 0x00,
    0x1f
    }
   ,
   {1.7, 1.7, 1.7},
   {NULL, NULL, NULL}
  },
  /* CANOLIDE80 */
  {CIS_CANONLIDE80,
   1200, /* real hardware limit is 2400 */
   20,   /* black pixels */
   6,    /* expdummy 6 */
   /* tuned to give 3*8 multiple startx coordinate during  shading calibration */
   34,    /* CCD_start_xoffset 14=>3, 20=>2 */
   10240, /* 10400, too wide=>10288 in shading data 10240~, 10208 too short for shading, max shading data = 10240 pixels, endpix-startpix=10208 */
   150,
   150,
   {0x00, 0x05, 0x07, 0x09}, /* in fact ,maps to 0x70-0x73 for GL841 */
   /* [0x10-0x15] values are initial led exposure values */
   /* 10    11    12    13    14    15    16    17    18    19    1a    1b    1c    1d*/
   {0x10, 0x00, 0x10, 0x00, 0x05, 0x00, 0x00, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x04},
   {0x03, 0x07, 0x00, 0x00, 0x00, 0x00, 0x29, 0x69, 0x55, 0x00, 0x00, 0x20, 0x41} ,
   {1.0, 1.0, 1.0},
   {NULL, NULL, NULL}}
};

/** for General Purpose Output specific settings:
 * initial GPO value (registers 0x66-0x67/0x6c-0x6d)
 * GPO enable mask (registers 0x68-0x69/0x6e-0x6f)
 * The first register is for GPIO9-GPIO16, the second for GPIO1-GPIO8
 */
static Genesys_Gpo Gpo[] = {
  /* UMAX */
  {GPO_UMAX,
   {0x11, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* Plustek OpticPro S12/ST12 */
  {GPO_ST12,
   {0x11, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* Plustek OpticPro S24/ST24 */
  {GPO_ST24,
   {0x00, 0x00}
   ,
   {0x51, 0x20}
   ,
   }
  ,
  /* MD5345/MD6471 */
  {GPO_5345,
   {0x30, 0x18}
   ,				/* bits 11-12 are for bipolar V-ref input voltage */
   {0xa0, 0x18}
   ,
   }
  ,
  /* HP2400C */
  {GPO_HP2400,
   {0x30, 0x00}
   ,
   {0x31, 0x00}
   ,
   }
  ,
  /* HP2300C */
  {GPO_HP2300,
   {0x00, 0x00}
   ,
   {0x00, 0x00}
   ,
   }
  ,
  /* CANONLIDE35 */
  {GPO_CANONLIDE35,
   {0x02, 0x80}
   ,
   {0xef, 0x80}
   ,
   }
  ,
  /* 7: XP200 */
  {GPO_XP200,
   {0x30, 0x00}
   ,
   {0xb0, 0x00}
   ,
   },
  /* HP3670 */
  {GPO_HP3670,
   {0x00, 0x00}
   ,
   {0x00, 0x00}
  }
  ,
  /* 8: XP300 */
  {GPO_XP300,
   {0x09, 0xc6},
   {0xbb, 0x00},
  }
  ,
  /* Syscan DP 665 */
  {
    GPO_DP665,
   {0x18, 0x00},/*0x19,0x00*/
   {0xbb, 0x00},
  }
  ,
  /* Syscan DP 685 */
  {
    GPO_DP685,
   {0x3f, 0x46}, /* 6c, 6d */
   {0xfb, 0x00}, /* 6e, 6f */
  },
  /* CANONLIDE200 */
  {GPO_CANONLIDE200,
   {0xfb, 0x20},	/* 0xfb when idle , 0xf9/0xe9 (1200) when scanning */
   {0xff, 0x00},
  },
  /* CANONLIDE700 */
  {GPO_CANONLIDE700,
   {0xdb, 0xff},
   {0xff, 0x80},
  },
  {GPO_KVSS080,
   {0xf5, 0x20},
   {0x7e, 0xa1},
  }
  ,
  {GPO_G4050,
   {0x20, 0x00},
   {0xfc, 0x00},
  }
  ,
  /* HP N6310 */
  {GPO_HP_N6310,
   {0xa3, 0x00},
   {0x7f, 0x00},
  }
  ,
  /* CANONLIDE110 */
  {GPO_CANONLIDE110,
   {0xfb, 0x20},
   {0xff, 0x00},
  }
  ,
  /* CANONLIDE210 */
  {GPO_CANONLIDE210,
   {0xfb, 0x20},
   {0xff, 0x00},
  }
  ,
  /* Plustek 3600 */
  {GPO_PLUSTEK_3600,
   {0x02, 0x00},
   {0x1e, 0x80},
  }
  /* CanoScan 4400f */
  ,
  {GPO_CS4400F,
   {0x01, 0x7f},
   {0xff, 0x00},
  }
  /* CanoScan 8400f */
  ,
  {GPO_CS8400F,
   {0x9a, 0xdf},
   {0xfe, 0x60},
  }
  /* Canon Image formula 101 */
  ,
  {GPO_IMG101,
   {0x41, 0xa4},
   {0x13, 0xa7}
  }
  /* Plustek OpticBook 3800 */
  ,
  {GPO_PLUSTEK3800,
   {0x41, 0xa4},
   {0x13, 0xa7}
  },
  /* Canon LiDE 80 */
  {
    GPO_CANONLIDE80,
   {0x28, 0x90}, /* 6c, 6d */
   {0x75, 0x80}, /* 6e, 6f */
  }
};

static Genesys_Motor Motor[] = {
  /* UMAX */
  {MOTOR_UMAX,
   1200,			/* motor base steps */
   2400,			/* maximum motor resolution */
   1,				/* maximum step mode */
   1,                           /* number of power modes*/
   {{{
     11000,			/* maximum start speed */
     3000,			/* maximum end speed */
     128,			/* step count */
     1.0,			/* nonlinearity */
     },
    {
     11000,
     3000,
     128,
     1.0,
   },},},
  },
  {MOTOR_5345,				/* MD5345/6228/6471 */
   1200,
   2400,
   1,
   1,
   {{{
     2000,
     1375,
     128,
     0.5,
     },
    {
     2000,
     1375,
     128,
     0.5,
    },},},
  },
  {MOTOR_ST24,			/* ST24 */
   2400,
   2400,
   1,
   1,
   {{{
     2289,
     2100,
     128,
     0.3,
     },
    {
     2289,
     2100,
     128,
     0.3,
    },},},
  },
  {MOTOR_HP3670,	/* HP 3670 */
   1200,
   2400,
   1,
   1,
   {{{
     11000,	/* start speed */
     3000,	/* max speed */
     128,	/* min steps */
     0.25,
     },
    {
     11000,
     3000,
     128,
     0.5,
    },},},
  },
  {MOTOR_HP2400,		/* HP 2400c */
   1200,
   1200,
   1,
   1,
   {{{
     11000,	/* start speed */
     3000,	/* max speed */
     128,	/* min steps */
     0.25,
     },
    {
     11000,
     3000,
     128,
     0.5,
    },},},
  },
  {MOTOR_HP2300,		/* HP 2300c */
   600, /* 600/1200 */
   1200,
   1,
   1,
   {{{
     3200,
     1200,
     128,
     0.5,
     },
    {
     3200,
     1200,
     128,
     0.5,
   },},},
  },
  {MOTOR_CANONLIDE35,		/* Canon LiDE 35 */
   1200,
   2400,
   1,
   1,
   {{{ 3500, 1300, 60, 0.8, },
    { 3500, 1400, 60, 0.8, },},},
  },
  {MOTOR_XP200,			/* Strobe XP200 */
   600,
   600,
   1,
   1,
   {{{
     3500,
     1300,
     60,
     0.25,
     },
    {
     3500,
     1400,
     60,
     0.5,
    },},},
  },
  {MOTOR_XP300,				/* 7: Visioneer Strobe XP300 */
   300,
   600,
   1,
   1,
   {{{ /* works best with GPIO10, GPIO14 off */
     3700,
     3700,
     2,
     0.8,
     },
    {
     11000,
     11000,
     2,
     0.8,
     },},},
  },
  {MOTOR_DP665,				/* Syscan DP 665 */
   750,
   1500,
   1,
   1,
   {{{
     3000,
     2500,
     10,
     0.8,
     },
    {
     11000,
     11000,
     2,
     0.8,
     },},},
  },
  {MOTOR_ROADWARRIOR,			/* Visioneer Roadwarrior */
   750,
   1500,
   1,
   1,
   {{{
     3000,
     2600,
     10,
     0.8,
     },
    {
     11000,
     11000,
     2,
     0.8,
     },},},
  },
  {MOTOR_DSMOBILE_600,			/* Pentax DSmobile 600 */
   750,
   1500,
   2,
   1,
   {{{
     6666,
     3700,
     8,
     0.8,
     },
    {
     6666,
     3700,
     8,
     0.8,
     },},},
  },
  {MOTOR_CANONLIDE100,		/* Canon LiDE 100 */
   1200,
   6400,
   2,   /* maximum step type count */
   1,   /* maximum power modes count */
   { /* motor slopes */
	   { /* power mode 0 */
		   {   3000,   1000, 127, 0.50}, /* full step */
    		   {   3000,   1500, 127, 0.50}, /* half step */
    		   { 3*2712, 3*2712, 16, 0.80}, /* quarter step 0.75*2712 */
	   },
    },
  },
  {MOTOR_CANONLIDE200,		/* Canon LiDE 200 */
   1200,
   6400,
   2,
   1,
   { /* motor slopes */
	   { /* power mode 0 */
		   {   3000,   1000, 127, 0.50}, /* full step */
    		   {   3000,   1500, 127, 0.50}, /* half step */
    		   { 3*2712, 3*2712, 16, 0.80}, /* quarter step 0.75*2712 */
	   },
    },
  },
  {MOTOR_CANONLIDE700,		/* Canon LiDE 700 */
   1200,
   6400,
   2,
   1,
   { /* motor slopes */
	   { /* power mode 0 */
		   {   3000,   1000, 127, 0.50}, /* full step */
    		   {   3000,   1500, 127, 0.50}, /* half step */
    		   { 3*2712, 3*2712, 16, 0.80}, /* quarter step 0.75*2712 */
	   },
    },
  },
  {MOTOR_KVSS080,
   1200,
   1200,
   2,
   1,
   { /* motor slopes */
	   { /* power mode 0 */
     		{ 22222, 500, 246, 0.5 }, /* max speed / dpi * base dpi => exposure */
     		{ 22222, 500, 246, 0.5 },
     		{ 22222, 500, 246, 0.5 },
    	   },
   },
  },
  {MOTOR_G4050,
   2400,
   9600,
   2,
   1,
   { /* motor slopes */
	   { /* power mode 0 */
     		{ 3961, 240, 246, 0.8 }, /* full step   */
     		{ 3961, 240, 246, 0.8 }, /* half step   */
     		{ 3961, 240, 246, 0.8 }, /* quarter step */
    	   },
   },
  },
  {MOTOR_CS8400F,
   2400,
   9600,
   2,
   1,
   { /* motor slopes */
	   { /* power mode 0 */
     		{ 3961, 240, 246, 0.8 }, /* full step   */
     		{ 3961, 240, 246, 0.8 }, /* half step   */
     		{ 3961, 240, 246, 0.8 }, /* quarter step */
    	   },
   },
  },
  {MOTOR_CANONLIDE110,		/* Canon LiDE 110 */
   4800,
   9600,
   1,   /* maximum step type count */
   1,   /* maximum power modes count */
   { /* motor slopes */
	   { /* power mode 0 */
		   {   3000,   1000, 256, 0.50}, /* full step */
	   },
    },
  },
  {MOTOR_CANONLIDE210,		/* Canon LiDE 210 */
   4800,
   9600,
   1,   /* maximum step type count */
   1,   /* maximum power modes count */
   { /* motor slopes */
	   { /* power mode 0 */
		   {   3000,   1000, 256, 0.50}, /* full step */
	   },
    },
  },
  {MOTOR_PLUSTEK_3600,		/* PLUSTEK 3600 */
   1200,
   2400,
   1,
   1,
   {
     {
       { 3500, 1300, 60, 0.8 },
       { 3500, 3250, 60, 0.8 },
     },
   },},
  {MOTOR_IMG101,		/* Canon Image Formula 101 */
   600,
   1200,
   1,
   1,
   {
     {
       { 3500, 1300, 60, 0.8 },
       { 3500, 3250, 60, 0.8 },
     },
   },},
  {MOTOR_PLUSTEK3800,		/* Plustek OpticBook 3800 */
   600,
   1200,
   1,
   1,
   {
     {
       { 3500, 1300, 60, 0.8 },
       { 3500, 3250, 60, 0.8 },
     },
   },},
  {MOTOR_CANONLIDE80,
   2400, /* 2400 ???? */
   4800, /* 9600 ???? */
   1,	/* max step type */
   1,	/* power mode count */
   {
     { /* start speed, max end speed, step number */
       /* maximum speed (second field) is used to compute exposure as seen by motor */
       /* exposure=max speed/ slope dpi * base dpi */
       /* 5144 = max pixels at 600 dpi */
       /* 1288=(5144+8)*ydpi(=300)/base_dpi(=1200) , where 5152 is exposure */
       /* 6440=9660/(1932/1288) */
       {  9560,  1912, 31, 0.8 },
     },
   },},
};

/* here we have the various device settings...
 */
static Genesys_Model umax_astra_4500_model = {
  "umax-astra-4500",		/* Name */
  "UMAX",			/* Device vendor string */
  "Astra 4500",			/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_UMAX,
  DAC_WOLFSON_UMAX,
  GPO_UMAX,
  MOTOR_UMAX,
  GENESYS_FLAG_UNTESTED,	/* Which flags are needed for this scanner? */
  /* untested, values set by hmg */
  GENESYS_HAS_NO_BUTTONS, /* no buttons supported */
  20,
  200
};

static Genesys_Model canon_lide_50_model = {
  "canon-lide-50",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 35/40/50",		/* Device model name */
  GENESYS_GL841,
  NULL,

  {      1200, 600, 400, 300, 240, 200, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 400, 300, 240, 200, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.42),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.9),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (6.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_CANONLIDE35,
  DAC_CANONLIDE35,
  GPO_CANONLIDE35,
  MOTOR_CANONLIDE35,
  GENESYS_FLAG_LAZY_INIT | 	/* Which flags are needed for this scanner? */
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_DARK_WHITE_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA |
  GENESYS_FLAG_HALF_CCD_MODE,
  GENESYS_HAS_SCAN_SW |
  GENESYS_HAS_FILE_SW |
  GENESYS_HAS_EMAIL_SW |
  GENESYS_HAS_COPY_SW,
  280,
  400
};

static Genesys_Model panasonic_kvss080_model = {
  "panasonic-kv-ss080",		/* Name */
  "Panasonic",			/* Device vendor string */
  "KV-SS080",			/* Device model name */
  GENESYS_GL843,
  NULL,

  { 600, /* 500, 400,*/ 300, 200, 150, 100, 75, 0},	/* possible x-resolutions */
  { 1200, 600, /* 500, 400, */ 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (7.2),		/* Start of scan area in mm  (x) */
  SANE_FIX (14.7),		/* Start of scan area in mm (y) */
  SANE_FIX (217.7),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_KVSS080,
  DAC_KVSS080,
  GPO_KVSS080,
  MOTOR_KVSS080,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW ,
  100,
  100
};

static Genesys_Model hp4850c_model = {
  "hewlett-packard-scanjet-4850c",	/* Name */
  "Hewlett Packard",			/* Device vendor string */
  "ScanJet 4850C",			/* Device model name */
  GENESYS_GL843,
  NULL,

  {2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (7.9),        /* Start of scan area in mm  (x) */
  SANE_FIX (5.9),        /* Start of scan area in mm (y) */
  SANE_FIX (219.6),      /* Size of scan area in mm (x) */
  SANE_FIX (314.5),      /* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,		        /* RGB CCD Line-distance correction in line number */
  				/* 0 38 76 OK 1200/2400 */
  				/* 0 24 48 OK [100,600] dpi */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_G4050,
  DAC_G4050,
  GPO_G4050,
  MOTOR_G4050,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_STAGGERED_LINE |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_DARK_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW,
  100,
  100
};

static Genesys_Model hpg4010_model = {
  "hewlett-packard-scanjet-g4010",	/* Name */
  "Hewlett Packard",			/* Device vendor string */
  "ScanJet G4010",			/* Device model name */
  GENESYS_GL843,
  NULL,

  { 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  { 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (8.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.00),		/* Start of scan area in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in mm (x) 5148 pixels at 600 dpi*/
  SANE_FIX (315.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,		        /* RGB CCD Line-distance correction in line number */
  				/* 0 38 76 OK 1200/2400 */
  				/* 0 24 48 OK [100,600] dpi */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_G4050,
  DAC_G4050,
  GPO_G4050,
  MOTOR_G4050,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_STAGGERED_LINE |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_DARK_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW,
  100,
  100
};

static Genesys_Model hpg4050_model = {
  "hewlett-packard-scanjet-g4050",	/* Name */
  "Hewlett Packard",			/* Device vendor string */
  "ScanJet G4050",			/* Device model name */
  GENESYS_GL843,
  NULL,

  { 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  { 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (8.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.00),		/* Start of scan area in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in mm (x) 5148 pixels at 600 dpi*/
  SANE_FIX (315.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (8.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (13.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (250.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (40.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,		        /* RGB CCD Line-distance correction in line number */
  				/* 0 38 76 OK 1200/2400 */
  				/* 0 24 48 OK [100,600] dpi */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_G4050,
  DAC_G4050,
  GPO_G4050,
  MOTOR_G4050,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_STAGGERED_LINE |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_DARK_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW,
  100,
  100
};


static Genesys_Model canon_4400f_model = {
  "canon-canoscan-4400f",	/* Name */
  "Canon",			/* Device vendor string */
  "Canoscan 4400f",		/* Device model name */
  GENESYS_GL843,
  NULL,

  { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (6.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.00),		/* Start of scan area in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in mm (x) 5148 pixels at 600 dpi*/
  SANE_FIX (315.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (8.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (13.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (250.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (40.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,		        /* RGB CCD Line-distance correction in line number */
  				/* 0 38 76 OK 1200/2400 */
  				/* 0 24 48 OK [100,600] dpi */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_CS4400F,
  DAC_G4050,
  GPO_CS4400F,
  MOTOR_G4050,
  GENESYS_FLAG_NO_CALIBRATION |
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_STAGGERED_LINE |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_DARK_CALIBRATION |
  GENESYS_FLAG_FULL_HWDPI_MODE |
  GENESYS_FLAG_HALF_CCD_MODE |		/* actually quarter CCD mode ... */
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW,
  100,
  100
};


static Genesys_Model canon_8400f_model = {
  "canon-canoscan-8400f",	/* Name */
  "Canon",			/* Device vendor string */
  "Canoscan 8400f",		/* Device model name */
  GENESYS_GL843,
  NULL,

  { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  { 4800, 2400, 1200, 600, 400, 300, 200, 150, 100, 0},
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (4.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (13.00),		/* Start of scan area in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in mm (x) 5148 pixels at 600 dpi*/
  SANE_FIX (315.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (8.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (13.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (217.9),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (250.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (40.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,		        /* RGB CCD Line-distance correction in line number */
  				/* 0 38 76 OK 1200/2400 */
  				/* 0 24 48 OK [100,600] dpi */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_CS8400F,
  DAC_CS8400F,
  GPO_CS8400F,
  MOTOR_CS8400F,
  GENESYS_FLAG_NO_CALIBRATION |
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_STAGGERED_LINE |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_DARK_CALIBRATION |
  GENESYS_FLAG_FULL_HWDPI_MODE |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_COPY_SW,
  100,
  100
};



static Genesys_Model canon_lide_100_model = {
  "canon-lide-100",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 100",			/* Device model name */
  GENESYS_GL847,
  NULL,

  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.1),		/* Start of scan area in mm (x) */
  SANE_FIX (8.3),		/* Start of scan area in mm (y) */
  SANE_FIX (216.07),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE100,
  DAC_CANONLIDE200,
  GPO_CANONLIDE200,
  MOTOR_CANONLIDE100,
  /* Which flags are needed for this scanner? */
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SIS_SENSOR
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  50,
  400
};

static Genesys_Model canon_lide_110_model = {
  "canon-lide-110",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 110",			/* Device model name */
  GENESYS_GL124,
  NULL,

  {4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.2),		/* Start of scan area in mm (x) */
  SANE_FIX (9.0),		/* Start of scan area in mm (y) */
  SANE_FIX (216.70),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE110,
  DAC_CANONLIDE110,
  GPO_CANONLIDE110,
  MOTOR_CANONLIDE110,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  50,
  400
};

static Genesys_Model canon_lide_120_model = {
  "canon-lide-120",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 120",			/* Device model name */
  GENESYS_GL124,
  NULL,

  {4800, 2400, 1200, 600, 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.2),		/* Start of scan area in mm (x) */
  SANE_FIX (9.0),		/* Start of scan area in mm (y) */
  SANE_FIX (216.70),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE120,
  DAC_CANONLIDE110,
  GPO_CANONLIDE110,
  MOTOR_CANONLIDE110,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  50,
  400
};


static Genesys_Model canon_lide_210_model = {
  "canon-lide-210",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 210",			/* Device model name */
  GENESYS_GL124,
  NULL,

  {4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, /* 400,*/ 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.2),		/* Start of scan area in mm (x) */
  SANE_FIX (8.7),		/* Start of scan area in mm (y) */
  SANE_FIX (216.70),		/* Size of scan area in mm (x) */
  SANE_FIX (297.5),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE210,
  DAC_CANONLIDE110,
  GPO_CANONLIDE210,
  MOTOR_CANONLIDE210,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_EXTRA_SW,
  60,
  400
};

static Genesys_Model canon_lide_220_model = {
  "canon-lide-220",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 220",			/* Device model name */
  GENESYS_GL124, /* or a compatible one */
  NULL,

  {4800, 2400, 1200, 600, 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.2),		/* Start of scan area in mm (x) */
  SANE_FIX (8.7),		/* Start of scan area in mm (y) */
  SANE_FIX (216.70),		/* Size of scan area in mm (x) */
  SANE_FIX (297.5),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE220,
  DAC_CANONLIDE110,
  GPO_CANONLIDE210,
  MOTOR_CANONLIDE210,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW | GENESYS_HAS_EXTRA_SW,
  60,
  400
};

static Genesys_Model canon_5600f_model = {
  "canon-5600f",		/* Name */
  "Canon",			/* Device vendor string */
  "5600F",			/* Device model name */
  GENESYS_GL847,
  NULL,

  {1200, 600, 400, 300, 200, 150, 100, 75, 0},	/* possible x-resolutions */
  {1200, 600, 400, 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.1),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.3),		/* Start of scan area in mm (y) */
  SANE_FIX (216.07),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE200,
  DAC_CANONLIDE200,
  GPO_CANONLIDE200,
  MOTOR_CANONLIDE200,
  GENESYS_FLAG_UNTESTED		/* not working yet */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SIS_SENSOR
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  50,
  400
};

static Genesys_Model canon_lide_700f_model = {
  "canon-lide-700f",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 700F",			/* Device model name */
  GENESYS_GL847,
  NULL,

  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.1),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.1),		/* Start of scan area in mm (y) */
  SANE_FIX (216.07),		/* Size of scan area in mm (x) */
  SANE_FIX (297.0),		/* Size of scan area in mm (y) */

  SANE_FIX (1.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE700,
  DAC_CANONLIDE700,
  GPO_CANONLIDE700,
  MOTOR_CANONLIDE700,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SIS_SENSOR
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  70,
  400
};



static Genesys_Model canon_lide_200_model = {
  "canon-lide-200",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 200",			/* Device model name */
  GENESYS_GL847,
  NULL,

  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible x-resolutions */
  {4800, 2400, 1200, 600, 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (1.1),		/* Start of scan area in mm  (x) */
  SANE_FIX (8.3),		/* Start of scan area in mm (y) */
  SANE_FIX (216.07),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE200,
  DAC_CANONLIDE200,
  GPO_CANONLIDE200,
  MOTOR_CANONLIDE200,
      GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_SIS_SENSOR
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_SHADING_REPARK
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_FILE_SW,
  50,
  400
};


static Genesys_Model canon_lide_60_model = {
  "canon-lide-60",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 60",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.42),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.9),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (6.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_CANONLIDE35,
  DAC_CANONLIDE35,
  GPO_CANONLIDE35,
  MOTOR_CANONLIDE35,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_WHITE_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_HALF_CCD_MODE,

  GENESYS_HAS_NO_BUTTONS, /* no buttons supported */
  300,
  400
};				/* this is completely untested -- hmg */

static Genesys_Model canon_lide_80_model = {
  "canon-lide-80",		/* Name */
  "Canon",			/* Device vendor string */
  "LiDE 80",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {      1200, 600, 400, 300, 240, 150, 100, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 400, 300, 240, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */
  SANE_FIX (0.42),		/* Start of scan area in mm  (x)   0.42 */
  SANE_FIX (7.90),		/* Start of scan area in mm (y)    7.90 */
  SANE_FIX (216.07),		/* Size of scan area in mm (x)   218.00 */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (4.5),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CIS_CANONLIDE80,
  DAC_CANONLIDE80,
  GPO_CANONLIDE80,
  MOTOR_CANONLIDE80,
  GENESYS_FLAG_LAZY_INIT | 	/* Which flags are needed for this scanner? */
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_DARK_WHITE_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA |
  GENESYS_FLAG_HALF_CCD_MODE,
  GENESYS_HAS_SCAN_SW |
  GENESYS_HAS_FILE_SW |
  GENESYS_HAS_EMAIL_SW |
  GENESYS_HAS_COPY_SW,
  160, /* 280 @2400 */
  400
};


static Genesys_Model hp2300c_model = {
  "hewlett-packard-scanjet-2300c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 2300c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions, motor can go up to 1200 dpi */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (2.0),		/* Start of scan area in mm (x_offset) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y_offset) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (295.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  16, 8, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_HP2300,
  DAC_WOLFSON_HP2300,
  GPO_HP2300,
  MOTOR_HP2300,
  GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_COPY_SW,
  40,
  132
};

static
Genesys_Model hp2400c_model = {
  "hewlett-packard-scanjet-2400c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 2400c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 100, 50, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (6.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (2.5),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (297.2),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_HP2400,
  DAC_WOLFSON_HP2400,
  GPO_HP2400,
  MOTOR_HP2400,
      GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_STAGGERED_LINE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_SCAN_SW,
  20,
  132
};

static
Genesys_Model visioneer_xp200_model = {
  "visioneer-strobe-xp200",	/* Name */
  "Visioneer",			/* Device vendor string */
  "Strobe XP200",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {600, 300, 200, 100, 75, 0},	/* possible x-resolutions */
  {600, 300, 200, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (16.0),		/* Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (297.2),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CIS_XP200,
  DAC_AD_XP200,			/* Analog Device frontend */
  GPO_XP200,
  MOTOR_XP200,
      GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  120,
  132
};

static Genesys_Model hp3670c_model = {
  "hewlett-packard-scanjet-3670c",	/* Name */
  "Hewlett Packard",		/* Device vendor string */
  "ScanJet 3670c",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (8.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (11.0),		/* Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (104.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (55.6),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (25.6),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (78.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (76.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_HP3670,
  DAC_WOLFSON_HP3670,
  GPO_HP3670,
  MOTOR_HP3670,
      GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_XPA
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_STAGGERED_LINE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_SCAN_SW,
  20,
  200
};

static Genesys_Model plustek_st12_model = {
  "plustek-opticpro-st12",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro ST12",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_ST12,
  DAC_WOLFSON_ST12,
  GPO_ST12,
  MOTOR_UMAX,
  GENESYS_FLAG_UNTESTED | GENESYS_FLAG_14BIT_GAMMA,	/* Which flags are needed for this scanner? */
  GENESYS_HAS_NO_BUTTONS, /* no buttons supported */
  20,
  200
};

static Genesys_Model plustek_st24_model = {
  "plustek-opticpro-st24",	/* Name */
  "Plustek",			/* Device vendor string */
  "OpticPro ST24",		/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 300, 150, 75, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (3.5),		/* Start of scan area in mm  (x) */
  SANE_FIX (7.5),		/* Start of scan area in mm (y) */
  SANE_FIX (218.0),		/* Size of scan area in mm (x) */
  SANE_FIX (299.0),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (1.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 8, 16,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_BGR,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_ST24,
  DAC_WOLFSON_ST24,
  GPO_ST24,
  MOTOR_ST24,
  GENESYS_FLAG_UNTESTED
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_OFFSET_CALIBRATION,
  GENESYS_HAS_NO_BUTTONS, /* no buttons supported */
  20,
  200
};

static Genesys_Model medion_md5345_model = {
  "medion-md5345-model",	/* Name */
  "Medion",			/* Device vendor string */
  "MD5345/MD6228/MD6471",	/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX ( 0.30),		/* Start of scan area in mm  (x) */
  SANE_FIX ( 0.80),		/* 2.79 < Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (296.4),		/* Size of scan area in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in mm (y) */
  SANE_FIX (0.00),		/* Start of black mark in mm (x) */

  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_5345,
  DAC_WOLFSON_5345,
  GPO_5345,
  MOTOR_5345,
  GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_STAGGERED_LINE
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_SHADING_NO_MOVE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_POWER_SW | GENESYS_HAS_OCR_SW | GENESYS_HAS_SCAN_SW,
  40,
  200
};

static Genesys_Model visioneer_xp300_model = {
  "visioneer-strobe-xp300",		/* Name */
  "Visioneer",			/* Device vendor string */
  "Strobe XP300",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.0),		/* Start of scan area in mm (y) */
  SANE_FIX (435.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (26.5),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  /* this is larger than needed -- accounts for second sensor head, which is a
     calibration item */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */
  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_XP300,
  DAC_WOLFSON_XP300,
  GPO_XP300,
  MOTOR_XP300,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model syscan_docketport_665_model = {
  "syscan-docketport-665",		/* Name */
  "Syscan/Ambir",			/* Device vendor string */
  "DocketPORT 665",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (108.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (17.5),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_DP665,
  DAC_WOLFSON_XP300,
  GPO_DP665,
  MOTOR_DP665,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model visioneer_roadwarrior_model = {
  "visioneer-roadwarrior",		/* Name */
  "Visioneer",				/* Device vendor string */
  "Readwarrior",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_ROADWARRIOR,
  DAC_WOLFSON_XP300,
  GPO_DP665,
  MOTOR_ROADWARRIOR,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model syscan_docketport_465_model = {
  "syscan-docketport-465",		/* Name */
  "Syscan",				/* Device vendor string */
  "DocketPORT 465",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_ROADWARRIOR,
  DAC_WOLFSON_XP300,
  GPO_DP665,
  MOTOR_ROADWARRIOR,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_NO_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_UNTESTED,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW,
  300,
  400
};

static Genesys_Model visioneer_xp100_r3_model = {
  "visioneer-xp100-revision3",		/* Name */
  "Visioneer",				/* Device vendor string */
  "XP100 Revision 3",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_ROADWARRIOR,
  DAC_WOLFSON_XP300,
  GPO_DP665,
  MOTOR_ROADWARRIOR,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model pentax_dsmobile_600_model = {
  "pentax-dsmobile-600",		/* Name */
  "Pentax",				/* Device vendor string */
  "DSmobile 600",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_DSMOBILE600,
  DAC_WOLFSON_DSM600,
  GPO_DP665,
  MOTOR_DSMOBILE_600,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model syscan_docketport_467_model = {
  "syscan-docketport-467",		/* Name */
  "Syscan",				/* Device vendor string */
  "DocketPORT 467",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_DSMOBILE600,
  DAC_WOLFSON_DSM600,
  GPO_DP665,
  MOTOR_DSMOBILE_600,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model syscan_docketport_685_model = {
  "syscan-docketport-685",		/* Name */
  "Syscan/Ambir",			/* Device vendor string */
  "DocketPORT 685",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.0),		/* Start of scan area in mm (y) */
  SANE_FIX (212.0),		/* Size of scan area in mm (x) */
  SANE_FIX (500),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (26.5),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  /* this is larger than needed -- accounts for second sensor head, which is a
     calibration item */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */
  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_DP685,
  DAC_WOLFSON_DSM600,
  GPO_DP685,
  MOTOR_XP300,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model syscan_docketport_485_model = {
  "syscan-docketport-485",		/* Name */
  "Syscan/Ambir",			/* Device vendor string */
  "DocketPORT 485",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.0),		/* Start of scan area in mm (y) */
  SANE_FIX (435.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (26.5),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  /* this is larger than needed -- accounts for second sensor head, which is a
     calibration item */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */
  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_XP300,
  DAC_WOLFSON_XP300,
  GPO_XP300,
  MOTOR_XP300,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model dct_docketport_487_model = {
  "dct-docketport-487",		/* Name */
  "DCT",			/* Device vendor string */
  "DocketPORT 487",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (1.0),		/* Start of scan area in mm (y) */
  SANE_FIX (435.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (26.5),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  /* this is larger than needed -- accounts for second sensor head, which is a
     calibration item */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */
  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_XP300,
  DAC_WOLFSON_XP300,
  GPO_XP300,
  MOTOR_XP300,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_UNTESTED,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model visioneer_7100_model = {
  "visioneer-7100-model",	/* Name */
  "Visioneer",			/* Device vendor string */
  "OneTouch 7100",	/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX ( 4.00),		/* Start of scan area in mm  (x) */
  SANE_FIX ( 0.80),		/* 2.79 < Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (296.4),		/* Size of scan area in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in mm (y) */
  SANE_FIX (0.00),		/* Start of black mark in mm (x) */

  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
/* 48, 24, 0, */
  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_5345,
  DAC_WOLFSON_5345,
  GPO_5345,
  MOTOR_5345,
  GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_STAGGERED_LINE
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_POWER_SW | GENESYS_HAS_OCR_SW | GENESYS_HAS_SCAN_SW,
  40,
  200
};

static Genesys_Model xerox_2400_model = {
  "xerox-2400-model",	/* Name */
  "Xerox",		/* Device vendor string */
  "OneTouch 2400",	/* Device model name */
  GENESYS_GL646,
  NULL,

  {1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible x-resolutions */
  {2400, 1200, 600, 400, 300, 200, 150, 100, 75, 50, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX ( 4.00),		/* Start of scan area in mm  (x) */
  SANE_FIX ( 0.80),		/* 2.79 < Start of scan area in mm (y) */
  SANE_FIX (215.9),		/* Size of scan area in mm (x) */
  SANE_FIX (296.4),		/* Size of scan area in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in mm (y) */
  SANE_FIX (0.00),		/* Start of black mark in mm (x) */

  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.00),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.00),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  48, 24, 0,			/* RGB CCD Line-distance correction in pixel */
/* 48, 24, 0, */
  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_5345,
  DAC_WOLFSON_5345,
  GPO_5345,
  MOTOR_5345,
  GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_SEARCH_START
    | GENESYS_FLAG_STAGGERED_LINE
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_HALF_CCD_MODE
    | GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_COPY_SW | GENESYS_HAS_EMAIL_SW | GENESYS_HAS_POWER_SW | GENESYS_HAS_OCR_SW | GENESYS_HAS_SCAN_SW,
  40,
  200
};


static Genesys_Model xerox_travelscanner_model = {
  "xerox-travelscanner",		/* Name */
  "Xerox",				/* Device vendor string */
  "Travelscanner 100",			/* Device model name */
  GENESYS_GL841,
  NULL,

  {600, 300, 150, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (4.0),		/* Start of scan area in mm  (x) */
  SANE_FIX (0.0),		/* Start of scan area in mm (y) */
  SANE_FIX (220.0),		/* Size of scan area in mm (x) */
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (16.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_TRUE,			/* Is this a CIS scanner? */
  SANE_TRUE,			/* Is this a sheetfed scanner? */
  CCD_ROADWARRIOR,
  DAC_WOLFSON_XP300,
  GPO_DP665,
  MOTOR_ROADWARRIOR,
  GENESYS_FLAG_LAZY_INIT 	/* Which flags are needed for this scanner? */
    | GENESYS_FLAG_SKIP_WARMUP
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION,
  GENESYS_HAS_SCAN_SW | GENESYS_HAS_PAGE_LOADED_SW | GENESYS_HAS_CALIBRATE,
  100,
  400
};

static Genesys_Model plustek_3600_model = {
  "plustek-opticbook-3600",	/* Name */
  "PLUSTEK",			/* Device vendor string */
  "OpticBook 3600",		/* Device model name */
  GENESYS_GL841,
  NULL,
  {/*1200,*/ 600, 400, 300, 200, 150, 100, 75, 0},		/* possible x-resolutions */
  {/*2400,*/ 1200, 600, 400, 300, 200, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (0.42),/*SANE_FIX (0.42),		 Start of scan area in mm  (x) */
  SANE_FIX (6.75),/*SANE_FIX (7.9),		 Start of scan area in mm (y) */
  SANE_FIX (216.0),/*SANE_FIX (216.0),		 Size of scan area in mm (x) */
  SANE_FIX (297.0),/*SANE_FIX (297.0),		 Size of scan area in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_PLUSTEK_3600,
  DAC_PLUSTEK_3600,
  GPO_PLUSTEK_3600,
  MOTOR_PLUSTEK_3600,
  GENESYS_FLAG_UNTESTED		/* not fully working yet */
	  | GENESYS_FLAG_CUSTOM_GAMMA
	  | GENESYS_FLAG_SKIP_WARMUP
	  | GENESYS_FLAG_DARK_CALIBRATION
	  | GENESYS_FLAG_OFFSET_CALIBRATION
  	  | GENESYS_FLAG_LAZY_INIT
  	  | GENESYS_FLAG_HALF_CCD_MODE,/*
      | GENESYS_FLAG_NO_CALIBRATION,*/
  GENESYS_HAS_NO_BUTTONS,
  7,
  200
};

static Genesys_Model hpn6310_model = {
  "hewlett-packard-scanjet-N6310",	/* Name */
  "Hewlett Packard",			/* Device vendor string */
  "ScanJet N6310",			/* Device model name */
  GENESYS_GL847,
  NULL,

  { 2400, 1200, 600, 400, 300, 200, 150, 100, 75, 0},
  { 2400, 1200, 600, 400, 300, 200, 150, 100, 75, 0},

  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (6), 		/* Start of scan area in mm  (x) */
  SANE_FIX (2), 		/* Start of scan area in mm (y) */
  SANE_FIX (216),		/* Size of scan area in mm (x) 5148 pixels at 600 dpi*/
  SANE_FIX (511),		/* Size of scan area in mm (y) */

  SANE_FIX (3.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (100.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 0, 0,				/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_HP_N6310,
  DAC_CANONLIDE200,     /*Not defined yet for N6310 */
  GPO_HP_N6310,
  MOTOR_CANONLIDE200,   /*Not defined yet for N6310 */
  GENESYS_FLAG_UNTESTED		/* not fully working yet */
  | GENESYS_FLAG_LAZY_INIT
    | GENESYS_FLAG_14BIT_GAMMA
    | GENESYS_FLAG_DARK_CALIBRATION
    | GENESYS_FLAG_OFFSET_CALIBRATION
    | GENESYS_FLAG_CUSTOM_GAMMA
    | GENESYS_FLAG_SKIP_WARMUP
   | GENESYS_FLAG_NO_CALIBRATION,
/*     | GENESYS_FLAG_HALF_CCD_MODE,*/

  GENESYS_HAS_NO_BUTTONS,
  100,
  100
};


static Genesys_Model plustek_3800_model = {
  "plustek-opticbook-3800",	/* Name */
  "PLUSTEK",			/* Device vendor string */
  "OpticBook 3800",		/* Device model name */
  GENESYS_GL845,
  NULL,

  {1200, 600, 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (7.2),		/* Start of scan area in mm  (x) */
  SANE_FIX (14.7),		/* Start of scan area in mm (y) */
  SANE_FIX (217.7),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_PLUSTEK3800,
  DAC_PLUSTEK3800,
  GPO_PLUSTEK3800,
  MOTOR_PLUSTEK3800,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_NO_BUTTONS,	/* TODO there are 4 buttons to support */
  100,
  100
};


static Genesys_Model canon_formula101_model = {
  "canon-image-formula-101",		/* Name */
  "Canon",			/* Device vendor string */
  "Image Formula 101",			/* Device model name */
  GENESYS_GL846,
  NULL,

  {1200, 600, 300, 150, 100, 75, 0},	/* possible x-resolutions */
  {1200, 600, 300, 150, 100, 75, 0},	/* possible y-resolutions */
  {16, 8, 0},			/* possible depths in gray mode */
  {16, 8, 0},			/* possible depths in color mode */

  SANE_FIX (7.2),		/* Start of scan area in mm  (x) */
  SANE_FIX (14.7),		/* Start of scan area in mm (y) */
  SANE_FIX (217.7),		/* Size of scan area in mm (x) */
  SANE_FIX (300.0),		/* Size of scan area in mm (y) */

  SANE_FIX (9.0),		/* Start of white strip in mm (y) */
  SANE_FIX (0.0),		/* Start of black mark in mm (x) */

  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Start of scan area in TA mode in mm (y) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (x) */
  SANE_FIX (0.0),		/* Size of scan area in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Start of white strip in TA mode in mm (y) */

  SANE_FIX (0.0),		/* Size of scan area after paper sensor stops
				   sensing document in mm */
  SANE_FIX (0.0),		/* Amount of feeding needed to eject document
				   after finishing scanning in mm */

  0, 24, 48,			/* RGB CCD Line-distance correction in pixel */

  COLOR_ORDER_RGB,		/* Order of the CCD/CIS colors */

  SANE_FALSE,			/* Is this a CIS scanner? */
  SANE_FALSE,			/* Is this a sheetfed scanner? */
  CCD_IMG101,
  DAC_IMG101,
  GPO_IMG101,
  MOTOR_IMG101,
  GENESYS_FLAG_LAZY_INIT |
  GENESYS_FLAG_SKIP_WARMUP |
  GENESYS_FLAG_OFFSET_CALIBRATION |
  GENESYS_FLAG_CUSTOM_GAMMA,
  GENESYS_HAS_NO_BUTTONS ,
  100,
  100
};


static Genesys_USB_Device_Entry genesys_usb_device_list[] = {
  /* GL646 devices */
  {0x03f0, 0x0901, &hp2300c_model},
  {0x03f0, 0x0a01, &hp2400c_model},
  {0x03f0, 0x1405, &hp3670c_model},
  {0x0461, 0x0377, &medion_md5345_model},
  {0x04a7, 0x0229, &visioneer_7100_model},
  {0x0461, 0x038b, &xerox_2400_model},
  {0x04a7, 0x0426, &visioneer_xp200_model},
  {0x0638, 0x0a10, &umax_astra_4500_model},
  {0x07b3, 0x0600, &plustek_st12_model},
  {0x07b3, 0x0601, &plustek_st24_model},
  /* GL841 devices */
  {0x04a7, 0x0474, &visioneer_xp300_model},
  {0x04a7, 0x0494, &visioneer_roadwarrior_model},
  {0x04a7, 0x049b, &visioneer_xp100_r3_model},
  {0x04a7, 0x04ac, &xerox_travelscanner_model},
  {0x04a9, 0x2213, &canon_lide_50_model},
  {0x04a9, 0x221c, &canon_lide_60_model},
  {0x04a9, 0x2214, &canon_lide_80_model},
  {0x07b3, 0x0900, &plustek_3600_model},
  {0x0a17, 0x3210, &pentax_dsmobile_600_model},
  {0x04f9, 0x2038, &pentax_dsmobile_600_model}, /* clone, only usb id is different */
  {0x0a82, 0x4800, &syscan_docketport_485_model},
  {0x0a82, 0x4802, &syscan_docketport_465_model},
  {0x0a82, 0x4803, &syscan_docketport_665_model},
  {0x0a82, 0x480c, &syscan_docketport_685_model},
  {0x1dcc, 0x4810, &dct_docketport_487_model},
  {0x1dcc, 0x4812, &syscan_docketport_467_model},
  /* GL843 devices */
  {0x04da, 0x100f, &panasonic_kvss080_model},
  {0x03f0, 0x1b05, &hp4850c_model},
  {0x03f0, 0x4505, &hpg4010_model},
  {0x03f0, 0x4605, &hpg4050_model},
  {0x04a9, 0x2228, &canon_4400f_model},
  {0x04a9, 0x221e, &canon_8400f_model},
  /* GL845 devices */
  {0x07b3, 0x1300, &plustek_3800_model},
  /* GL846 devices */
  {0x1083, 0x162e, &canon_formula101_model},
  /* GL847 devices */
  {0x04a9, 0x1904, &canon_lide_100_model},
  {0x04a9, 0x1905, &canon_lide_200_model},
  {0x04a9, 0x1906, &canon_5600f_model},
  {0x04a9, 0x1907, &canon_lide_700f_model},
  {0x03f0, 0x4705, &hpn6310_model},
  /* GL124 devices */
  {0x04a9, 0x1909, &canon_lide_110_model},
  {0x04a9, 0x190e, &canon_lide_120_model},
  {0x04a9, 0x190a, &canon_lide_210_model},
  {0x04a9, 0x190f, &canon_lide_220_model},
  {0, 0, NULL}
};

#define MAX_SCANNERS (sizeof(genesys_usb_device_list) / \
        sizeof(genesys_usb_device_list[0]))

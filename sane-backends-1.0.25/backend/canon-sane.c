SANE_Status
sane_init (SANE_Int * version_code, SANE_Auth_Callback UNUSEDARG authorize)
{
  char devnam[PATH_MAX] = "/dev/scanner";
  FILE *fp;

  int i, j;
  SANE_Byte primary, secondary, inmask, priMask, secMask;

  DBG_INIT ();
  DBG (1, ">> sane_init\n");

/****** for lineart mode of FB1200S ******/
  for (i = 0; i < 256; i++)
    {
      primary = secondary = 0;
      inmask = 0x80;
      priMask = 0x40;
      secMask = 0x80;
      for (j = 0; j < 4; j++)
        {
          if (i & inmask)
            {
              primary |= priMask;
	      secondary |= secMask;
	    }
	  priMask = priMask >> 2;
	  secMask = secMask >> 2;
	  inmask  = inmask >> 1;
	}
      primaryHigh[i] = primary;
      secondaryHigh[i] = secondary;

      primary = secondary = 0;
      priMask = 0x40;
      secMask = 0x80;
      for (j = 0; j < 4; j++)
	{
	  if (i & inmask)
	    {
	      primary |= priMask;
	      secondary |= secMask;
	    }
	  priMask = priMask >> 2;
	  secMask = secMask >> 2;
	  inmask  = inmask >> 1;
	}
      primaryLow[i] = primary;
      secondaryLow[i] = secondary;
    }
/******************************************/

#if defined PACKAGE && defined VERSION
  DBG (2, "sane_init: " PACKAGE " " VERSION "\n");
#endif

  if (version_code)
    *version_code = SANE_VERSION_CODE (SANE_CURRENT_MAJOR, V_MINOR, 0);

  fp = sanei_config_open (CANON_CONFIG_FILE);
  if (fp)
    {
      char line[PATH_MAX];
      size_t len;

      /* read config file */
      /* while (fgets (line, sizeof (line), fp)) */
      while (sanei_config_read (line, sizeof (line), fp))
	{
	  if (line[0] == '#')	/* ignore line comments */
	    continue;
	  len = strlen (line);
	  /*if (line[len - 1] == '\n')
	     line[--len] = '\0'; */

	  if (!len)
	    continue;		/* ignore empty lines */
	  strcpy (devnam, line);
	}
      fclose (fp);
    }
  sanei_config_attach_matching_devices (devnam, attach_one);
  DBG (1, "<< sane_init\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

void
sane_exit (void)
{
  CANON_Device *dev, *next;
  DBG (1, ">> sane_exit\n");

  for (dev = first_dev; dev; dev = next)
    {
      next = dev->next;
      free ((void *) dev->sane.name);
      free ((void *) dev->sane.model);
      free ((void *) dev);
    }

  DBG (1, "<< sane_exit\n");
}

/**************************************************************************/

SANE_Status
sane_get_devices (const SANE_Device *** device_list,
SANE_Bool UNUSEDARG local_only)
{
  static const SANE_Device **devlist = 0;
  CANON_Device *dev;
  int i;
  DBG (1, ">> sane_get_devices\n");

  if (devlist)
    free (devlist);
  devlist = malloc ((num_devices + 1) * sizeof (devlist[0]));
  if (!devlist)
    return (SANE_STATUS_NO_MEM);

  i = 0;
  for (dev = first_dev; dev; dev = dev->next)
    devlist[i++] = &dev->sane;
  devlist[i++] = 0;

  *device_list = devlist;

  DBG (1, "<< sane_get_devices\n");
  return SANE_STATUS_GOOD;
}

/**************************************************************************/

SANE_Status
sane_open (SANE_String_Const devnam, SANE_Handle * handle)
{
  SANE_Status status;
  CANON_Device *dev;
  CANON_Scanner *s;
  int i, j, c;

  DBG (1, ">> sane_open\n");

  if (devnam[0] == '\0')
    {
      for (dev = first_dev; dev; dev = dev->next)
	{
	  if (strcmp (dev->sane.name, devnam) == 0)
	    break;
	}
    }
  else
    dev = first_dev;

  if (!dev)
    {
      status = attach (devnam, &dev);
      if (status != SANE_STATUS_GOOD)
	return (status);
    }


  if (!dev)
    return (SANE_STATUS_INVAL);

  s = malloc (sizeof (*s));
  if (!s)
    return SANE_STATUS_NO_MEM;
  memset (s, 0, sizeof (*s));

  s->fd = -1;
  s->hw = dev;

  if (s->hw->info.model == FS2710)
    {
      for (i = 0; i < 4; i++)
	{
	  s->gamma_map[i][0] = '\0';
	  s->gamma_table[i][0] = 0;
	}
      for (j = 1; j < 4096; ++j)
	{			/* FS2710 needs inital gamma 2.0 */
	  c = (int) (256.0 * pow (((double) j) / 4096.0, 0.5));
	  for (i = 0; i < 4; i++)
	    {
	      s->gamma_map[i][j] = (u_char) c;
	      if ((j & 0xf) == 0)
		s->gamma_table[i][j >> 4] = c;
	    }
	}
      s->colour = 1;
      s->auxbuf_len = 0;
    }
  else
    {
      for (i = 0; i < 4; ++i)
	{
	  for (j = 0; j < 256; ++j)
	    s->gamma_table[i][j] = j;
	}
    }

  init_options (s);

  if (s->hw->info.model == FB1200)
    s->inbuffer = malloc (30894);	/* modification for FB1200S */
  else
    s->inbuffer = malloc (15312);	/* modification for FB620S */

  if (!s->inbuffer)
    return SANE_STATUS_NO_MEM;

  if (s->hw->info.model == FB1200)
    s->outbuffer = malloc (30894);	/* modification for FB1200S */
  else
    s->outbuffer = malloc (15312);	/* modification for FB620S */

  if (!s->outbuffer)
    {
      free (s->inbuffer);
      return SANE_STATUS_NO_MEM;
    }

  s->next = first_handle;
  first_handle = s;

  *handle = s;

  DBG (1, "<< sane_open\n");
  return SANE_STATUS_GOOD;

}

/**************************************************************************/

void
sane_close (SANE_Handle handle)
{
  CANON_Scanner *s = (CANON_Scanner *) handle;
  SANE_Status status;

  DBG (1, ">> sane_close\n");

  if (s->val[OPT_EJECT_BEFOREEXIT].w)
    {
      if (s->fd == -1)
	sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
      status = medium_position (s->fd);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_close: MEDIUM POSITION failed\n");
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	}
      s->AF_NOW = SANE_TRUE;
      DBG (1, "sane_close AF_NOW = '%d'\n", s->AF_NOW);
    }

  if (s->fd != -1)
    sanei_scsi_close (s->fd);

  if (s->inbuffer)
    free (s->inbuffer);		/* modification for FB620S */
  if (s->outbuffer)
    free (s->outbuffer);	/* modification for FB620S */
  if (s->auxbuf_len > 0)
    free (s->auxbuf);		/* modification for FS2710S */

  free (s);

  DBG (1, ">> sane_close\n");
}

/**************************************************************************/

const SANE_Option_Descriptor *
sane_get_option_descriptor (SANE_Handle handle, SANE_Int option)
{
  CANON_Scanner *s = handle;
  DBG (21, ">> sane_get_option_descriptor option number %d\n", option);

  if ((unsigned) option >= NUM_OPTIONS)
    return (0);

  DBG (21, "   sane_get_option_descriptor option name %s\n",
       option_name[option]);

  DBG (21, "<< sane_get_option_descriptor option number %d\n", option);
  return (s->opt + option);
}

/**************************************************************************/
SANE_Status
sane_control_option (SANE_Handle handle, SANE_Int option,
		     SANE_Action action, void *val, SANE_Int * info)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  SANE_Word w, cap;
  SANE_Byte gbuf[4096];
  size_t buf_size;
  int i, neg, gamma_component, int_t, transfer_data_type;
  time_t dtime, rt;

  DBG (21, ">> sane_control_option %s\n", option_name[option]);

  if (info)
    *info = 0;

  if (s->scanning == SANE_TRUE)
    {
      DBG (21, ">> sane_control_option: device is busy scanning\n");
      return (SANE_STATUS_DEVICE_BUSY);
    }
  if (option >= NUM_OPTIONS)
    return (SANE_STATUS_INVAL);

  cap = s->opt[option].cap;
  if (!SANE_OPTION_IS_ACTIVE (cap))
    return (SANE_STATUS_INVAL);

  if (action == SANE_ACTION_GET_VALUE)
    {
      DBG (21, "sane_control_option get value of %s\n", option_name[option]);
      switch (option)
	{
	  /* word options: */
	case OPT_FLATBED_ONLY:
	case OPT_TPU_ON:
	case OPT_TPU_PN:
	case OPT_TPU_TRANSPARENCY:
	case OPT_RESOLUTION_BIND:
	case OPT_HW_RESOLUTION_ONLY:	/* 990320, ss */
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_CUSTOM_GAMMA:
	case OPT_CUSTOM_GAMMA_BIND:
	case OPT_HNEGATIVE:
	  /*     case OPT_GRC: */
	case OPT_MIRROR:
	case OPT_AE:
	case OPT_PREVIEW:
	case OPT_BIND_HILO:
	case OPT_HILITE_R:
	case OPT_SHADOW_R:
	case OPT_HILITE_G:
	case OPT_SHADOW_G:
	case OPT_HILITE_B:
	case OPT_SHADOW_B:
	case OPT_EJECT_AFTERSCAN:
	case OPT_EJECT_BEFOREEXIT:
	case OPT_THRESHOLD:
	case OPT_AF:
	case OPT_AF_ONCE:
	case OPT_FOCUS:
	  if ((option >= OPT_NEGATIVE) && (option <= OPT_SHADOW_B))
	    {
	      DBG (21, "GET_VALUE for %s: s->val[%s].w = %d\n",
		   option_name[option], option_name[option],
		   s->val[option].w);
	    }
	  *(SANE_Word *) val = s->val[option].w;
	  DBG (21, "value for option %s: %d\n", option_name[option],
	       s->val[option].w);
	  return (SANE_STATUS_GOOD);

	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:

	  memset (gbuf, 0, sizeof (gbuf));
	  buf_size = 256;
	  transfer_data_type = 0x03;

	  DBG (21, "sending GET_DENSITY_CURVE\n");
	  if (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE)
	    /* If using bind analog gamma, option will be OPT_GAMMA_VECTOR.
	       In this case, use the curve for green                        */
	    gamma_component = 2;
	  else
	    /* Else use a different index for each curve */
	    gamma_component = option - OPT_GAMMA_VECTOR;

	  /* Now get the values from the scanner */
	  if (s->hw->info.model != FS2710)
	    {
	      sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
	      status =
		get_density_curve (s->fd, gamma_component, gbuf, &buf_size,
				   transfer_data_type);
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (21, "GET_DENSITY_CURVE\n");
		  return (SANE_STATUS_INVAL);
		}
	    }
	  else
	    status =
	      get_density_curve_fs2710 (s, gamma_component, gbuf, &buf_size);

	  neg = (s->hw->info.is_filmscanner) ?
	    strcmp (filmtype_list[1], s->val[OPT_NEGATIVE].s)
	    : s->val[OPT_HNEGATIVE].w;

	  for (i = 0; i < 256; i++)
	    {
	      if (!neg)
		s->gamma_table[option - OPT_GAMMA_VECTOR][i] =
		  (SANE_Int) gbuf[i];
	      else
		s->gamma_table[option - OPT_GAMMA_VECTOR][i] =
		  255 - (SANE_Int) gbuf[255 - i];
	    }

	  memcpy (val, s->val[option].wa, s->opt[option].size);
	  DBG (21, "value for option %s: %d\n", option_name[option],
	       s->val[option].w);
	  return (SANE_STATUS_GOOD);

	  /* string options: */
	case OPT_TPU_DCM:
	case OPT_TPU_FILMTYPE:
	case OPT_MODE:
	case OPT_NEGATIVE:
	case OPT_NEGATIVE_TYPE:
	case OPT_SCANNING_SPEED:
	  strcpy (val, s->val[option].s);
	  DBG (21, "value for option %s: %s\n", option_name[option],
	       s->val[option].s);
	  return (SANE_STATUS_GOOD);

	default:
	  val = 0;
	  return (SANE_STATUS_GOOD);
	}
    }
  else if (action == SANE_ACTION_SET_VALUE)
    {
      DBG (21, "sane_control_option set value for %s\n", option_name[option]);
      if (!SANE_OPTION_IS_SETTABLE (cap))
	return (SANE_STATUS_INVAL);

      status = sanei_constrain_value (s->opt + option, val, info);

      if (status != SANE_STATUS_GOOD)
	return status;

      switch (option)
	{
	  /* (mostly) side-effect-free word options: */
	case OPT_TPU_PN:
	case OPT_TPU_TRANSPARENCY:
	case OPT_X_RESOLUTION:
	case OPT_Y_RESOLUTION:
	case OPT_TL_X:
	case OPT_TL_Y:
	case OPT_BR_X:
	case OPT_BR_Y:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_PARAMS;
	  /* fall through */
	case OPT_NUM_OPTS:
	case OPT_BRIGHTNESS:
	case OPT_CONTRAST:
	case OPT_THRESHOLD:
	case OPT_HNEGATIVE:
	  /*     case OPT_GRC: */
	case OPT_MIRROR:
	case OPT_AE:
	case OPT_PREVIEW:
	case OPT_HILITE_R:
	case OPT_SHADOW_R:
	case OPT_HILITE_G:
	case OPT_SHADOW_G:
	case OPT_HILITE_B:
	case OPT_SHADOW_B:
	case OPT_AF_ONCE:
	case OPT_FOCUS:
	case OPT_EJECT_AFTERSCAN:
	case OPT_EJECT_BEFOREEXIT:
	  s->val[option].w = *(SANE_Word *) val;
	  DBG (21, "SET_VALUE for %s: s->val[%s].w = %d\n",
	       option_name[option], option_name[option], s->val[option].w);
	  return (SANE_STATUS_GOOD);

	case OPT_RESOLUTION_BIND:
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      s->val[option].w = *(SANE_Word *) val;

	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	      if (!s->val[option].w)
		{		/* don't bind */
		  s->opt[OPT_Y_RESOLUTION].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_X_RESOLUTION].title =
		    SANE_TITLE_SCAN_X_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_X_RESOLUTION;
		}
	      else
		{		/* bind */
		  s->opt[OPT_Y_RESOLUTION].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_X_RESOLUTION].title = SANE_TITLE_SCAN_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].name = SANE_NAME_SCAN_RESOLUTION;
		  s->opt[OPT_X_RESOLUTION].desc = SANE_DESC_SCAN_RESOLUTION;
		}
	    }
	  return SANE_STATUS_GOOD;

	  /* 990320, ss: switch between slider and option menue for resolution */
	case OPT_HW_RESOLUTION_ONLY:
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      int iPos, xres, yres;

	      s->val[option].w = *(SANE_Word *) val;

	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	      if (!s->val[option].w)	/* use complete range */
		{
		  s->opt[OPT_X_RESOLUTION].constraint_type =
		    SANE_CONSTRAINT_RANGE;
		  s->opt[OPT_X_RESOLUTION].constraint.range =
		    &s->hw->info.xres_range;
		  s->opt[OPT_Y_RESOLUTION].constraint_type =
		    SANE_CONSTRAINT_RANGE;
		  s->opt[OPT_Y_RESOLUTION].constraint.range =
		    &s->hw->info.yres_range;
		}
	      else			/* use only hardware resolutions */
		{
		  s->opt[OPT_X_RESOLUTION].constraint_type =
		    SANE_CONSTRAINT_WORD_LIST;
		  s->opt[OPT_X_RESOLUTION].constraint.word_list =
		    s->xres_word_list;
		  s->opt[OPT_Y_RESOLUTION].constraint_type =
		    SANE_CONSTRAINT_WORD_LIST;
		  s->opt[OPT_Y_RESOLUTION].constraint.word_list =
		    s->yres_word_list;

		  /* adjust resolutions */
		  xres = s->xres_word_list[1];
		  for (iPos = 0; iPos < s->xres_word_list[0]; iPos++)
		    {
		      if (s->val[OPT_X_RESOLUTION].w >=
			  s->xres_word_list[iPos + 1])
			xres = s->xres_word_list[iPos + 1];
		    }
		  s->val[OPT_X_RESOLUTION].w = xres;

		  yres = s->yres_word_list[1];
		  for (iPos = 0; iPos < s->yres_word_list[0]; iPos++)
		    {
		      if (s->val[OPT_Y_RESOLUTION].w >=
			  s->yres_word_list[iPos + 1])
			yres = s->yres_word_list[iPos + 1];
		    }
		  s->val[OPT_Y_RESOLUTION].w = yres;
		}
	    }
	  return (SANE_STATUS_GOOD);

	case OPT_BIND_HILO:
	  if (s->val[option].w != *(SANE_Word *) val)
	    {
	      s->val[option].w = *(SANE_Word *) val;

	      if (info)
		*info |= SANE_INFO_RELOAD_OPTIONS;
	      if (!s->val[option].w)
		{		/* don't bind */
		  s->opt[OPT_HILITE_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_SHADOW_R].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_HILITE_B].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_SHADOW_B].cap &= ~SANE_CAP_INACTIVE;

		  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT_G;
		  s->opt[OPT_HILITE_G].name = SANE_NAME_HIGHLIGHT_G;
		  s->opt[OPT_HILITE_G].desc = SANE_DESC_HIGHLIGHT_G;

		  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW_G;
		  s->opt[OPT_SHADOW_G].name = SANE_NAME_SHADOW_G;
		  s->opt[OPT_SHADOW_G].desc = SANE_DESC_SHADOW_G;
		}
	      else
		{		/* bind */
		  s->opt[OPT_HILITE_R].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_SHADOW_R].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_HILITE_B].cap |= SANE_CAP_INACTIVE;
		  s->opt[OPT_SHADOW_B].cap |= SANE_CAP_INACTIVE;

		  s->opt[OPT_HILITE_G].title = SANE_TITLE_HIGHLIGHT;
		  s->opt[OPT_HILITE_G].name = SANE_NAME_HIGHLIGHT;
		  s->opt[OPT_HILITE_G].desc = SANE_DESC_HIGHLIGHT;

		  s->opt[OPT_SHADOW_G].title = SANE_TITLE_SHADOW;
		  s->opt[OPT_SHADOW_G].name = SANE_NAME_SHADOW;
		  s->opt[OPT_SHADOW_G].desc = SANE_DESC_SHADOW;
		}
	    }
	  return SANE_STATUS_GOOD;

	case OPT_AF:
	  if (info && s->val[option].w != *(SANE_Word *) val)
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  s->val[option].w = *(SANE_Word *) val;
	  w = *(SANE_Word *) val;
	  if (w)
	    {
	      s->opt[OPT_AF_ONCE].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_FOCUS].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_AF_ONCE].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_FOCUS].cap &= ~SANE_CAP_INACTIVE;
	    }
	  return (SANE_STATUS_GOOD);

	case OPT_FLATBED_ONLY:
	  s->val[option].w = *(SANE_Word *) val;
	  if (s->hw->adf.Status != ADF_STAT_NONE && s->val[option].w)
	    {					/* switch on */
	      s->hw->adf.Priority |= 0x03;	/* flatbed mode */
	      s->hw->adf.Feeder &= 0x00;	/* autofeed off (default) */
	      s->hw->adf.Status = ADF_STAT_DISABLED;
	    }		/* if it isn't connected, don't bother fixing */
	  return SANE_STATUS_GOOD;

	case OPT_TPU_ON:
	  s->val[option].w = *(SANE_Word *) val;
	  if (s->val[option].w)			/* switch on */
	    {
	      s->hw->tpu.Status = TPU_STAT_ACTIVE;
	      s->opt[OPT_TPU_TRANSPARENCY].cap &=
		(s->hw->tpu.ControlMode == 3) ? ~SANE_CAP_INACTIVE : ~0;
	      s->opt[OPT_TPU_FILMTYPE].cap &=
		(s->hw->tpu.ControlMode == 1) ? ~SANE_CAP_INACTIVE : ~0;
	    }
	  else			/* switch off */
	    {
	      s->hw->tpu.Status = TPU_STAT_INACTIVE;
	      s->opt[OPT_TPU_TRANSPARENCY].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_TPU_FILMTYPE].cap |= SANE_CAP_INACTIVE;
	    }
	  s->opt[OPT_TPU_PN].cap ^= SANE_CAP_INACTIVE;
	  s->opt[OPT_TPU_DCM].cap ^= SANE_CAP_INACTIVE;
          if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_TPU_DCM:
	  if (s->val[OPT_TPU_DCM].s)
	    free (s->val[OPT_TPU_DCM].s);
	  s->val[OPT_TPU_DCM].s = strdup (val);

	  s->opt[OPT_TPU_TRANSPARENCY].cap |= SANE_CAP_INACTIVE;
	  s->opt[OPT_TPU_FILMTYPE].cap |= SANE_CAP_INACTIVE;
	  if (!strcmp (s->val[OPT_TPU_DCM].s,
	  SANE_I18N("Correction according to transparency ratio")))
	    {
	      s->hw->tpu.ControlMode = 3;
	      s->opt[OPT_TPU_TRANSPARENCY].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else if (!strcmp (s->val[OPT_TPU_DCM].s,
	  SANE_I18N("Correction according to film type")))
	    {
	      s->hw->tpu.ControlMode = 1;
	      s->opt[OPT_TPU_FILMTYPE].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    s->hw->tpu.ControlMode = 0;
          if (info)
	    *info |= SANE_INFO_RELOAD_PARAMS | SANE_INFO_RELOAD_OPTIONS;
	  return SANE_STATUS_GOOD;

	case OPT_TPU_FILMTYPE:
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  return SANE_STATUS_GOOD;

	case OPT_MODE:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (!strcmp (val, SANE_VALUE_SCAN_MODE_LINEART)
	  || !strcmp (val, SANE_VALUE_SCAN_MODE_HALFTONE))
	    {
	      /* For Lineart and Halftone: */
	      /* Enable "threshold" */
	      s->opt[OPT_THRESHOLD].cap &= ~SANE_CAP_INACTIVE;

	      /* Disable "custom gamma" and "brightness & contrast" */
	      s->opt[OPT_CUSTOM_GAMMA].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      /* For Gray and Color modes: */
	      /* Disable "threshold" */
	      s->opt[OPT_THRESHOLD].cap |= SANE_CAP_INACTIVE;

	      /* Enable "custom gamma" and "brightness & contrast" */
	      s->opt[OPT_CUSTOM_GAMMA].cap &= ~SANE_CAP_INACTIVE;
	      if (s->val[OPT_CUSTOM_GAMMA].w)
		{
		  if (!strcmp (val, SANE_VALUE_SCAN_MODE_COLOR)
		  || !strcmp (val, SANE_I18N("Fine color")))
		    {
		      s->opt[OPT_CUSTOM_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
		      if (s->val[OPT_CUSTOM_GAMMA_BIND].w == SANE_TRUE)
			{
			  s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
			}
		      else
			{
			  s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_R].cap &=
			    ~SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_G].cap &=
			    ~SANE_CAP_INACTIVE;
			  s->opt[OPT_GAMMA_VECTOR_B].cap &=
			    ~SANE_CAP_INACTIVE;
			}
		    }
		  else
		    {
		      s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		    }
		}
	      else
		{
		  s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
		  s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
		}
	    }
	  return (SANE_STATUS_GOOD);

	case OPT_NEGATIVE:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  if (!strcmp (val, SANE_I18N("Negatives")))
	    {
	      s->RIF = 0;
	      s->opt[OPT_NEGATIVE_TYPE].cap &= ~SANE_CAP_INACTIVE;
	      if (SANE_OPTION_IS_SETTABLE(s->opt[OPT_SCANNING_SPEED].cap))
		s->opt[OPT_SCANNING_SPEED].cap &= ~SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->RIF = 1;
	      s->opt[OPT_NEGATIVE_TYPE].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_SCANNING_SPEED].cap |= SANE_CAP_INACTIVE;
	    }
	  return (SANE_STATUS_GOOD);

	case OPT_NEGATIVE_TYPE:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  for (i = 0; strcmp (val, negative_filmtype_list[i]); i++);
	  s->negative_filmtype = i;
	  return (SANE_STATUS_GOOD);

	case OPT_SCANNING_SPEED:
	  if (info && strcmp (s->val[option].s, (SANE_String) val))
	    *info |= SANE_INFO_RELOAD_OPTIONS | SANE_INFO_RELOAD_PARAMS;
	  if (s->val[option].s)
	    free (s->val[option].s);
	  s->val[option].s = strdup (val);
	  for (i = 0; strcmp (val, scanning_speed_list[i]); i++);
	  s->scanning_speed = i;
	  return (SANE_STATUS_GOOD);

	  /* modification for FB620S */
	case OPT_CALIBRATION_NOW:
	  sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
	  if (status == SANE_STATUS_GOOD)
	    {
	      status = execute_calibration (s->fd);
	      if (status != SANE_STATUS_GOOD)
		{
		  DBG (21, "EXECUTE CALIBRATION failed\n");
		  sanei_scsi_close (s->fd);
		  s->fd = -1;
		  return (SANE_STATUS_INVAL);
		}
	      DBG (21, "EXECUTE CALIBRATION\n");
	      sanei_scsi_close (s->fd);
	    }
	  else
	    DBG (1, "calibration: cannot open device file\n");
	  s->fd = -1;
	  return status;

	case OPT_SCANNER_SELF_DIAGNOSTIC:
	  sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
	  if (status == SANE_STATUS_GOOD)
	    {
	      status = send_diagnostic (s->fd);
	      {
		if (status != SANE_STATUS_GOOD)
		  {
		    DBG (21, "SEND DIAGNOSTIC error: %s\n",
			 sane_strstatus (status));
		    sanei_scsi_close (s->fd);
		    s->fd = -1;
		    return (status);
		  }
		DBG (21, "SEND DIAGNOSTIC result: %s\n",
		  sane_strstatus (status));
		sanei_scsi_close (s->fd);
	      }
	    }
	  else
	    DBG (1, "send diagnostic: cannot open device file\n");
	  s->fd = -1;
	  return status;

	case OPT_RESET_SCANNER:
	  sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
	  if (status == SANE_STATUS_GOOD)
	    {
	      time (&(s->time1));
	      DBG (11, "time0 = %ld\n", s->time0);
	      DBG (11, "time1 = %ld\n", s->time1);
	      dtime = (s->time1) - (s->time0);
	      DBG (11, "dtime = %ld\n", dtime);

	      DBG (11, "switch_preview = %d\n", s->switch_preview);
	      if (s->switch_preview == 0)
		{
		  rt = sqrt (15 * 15 * (SANE_UNFIX (s->val[OPT_BR_Y].w))
		    / 297) + 0.5;
		  rt = rt + 2;
		}
	      else
		rt = 17;

	      DBG (11, "SANE_UNFIX(s->val[OPT_BR_Y].w) = %f\n",
		   SANE_UNFIX (s->val[OPT_BR_Y].w));
	      DBG (11, "rt = %ld\n", rt);

	      if (dtime < rt)
		{
		  int_t = (int) (rt - dtime);
		  DBG (11, "int_t = %d\n", int_t);

		  sleep (int_t);
		}
	      status = reset_scanner (s->fd);
	      {
		if (status != SANE_STATUS_GOOD)
		  {
		    DBG (21, "RESET SCANNER failed\n");
		    sanei_scsi_close (s->fd);
		    s->fd = -1;
		    return (status);
		  }

		DBG (21, "RESET SCANNER\n");
		sanei_scsi_close (s->fd);
	      }
	    }
	  else
	    DBG (1, "reset scanner: cannot open device file\n");
	  s->fd = -1;
	  return status;

	case OPT_EJECT_NOW:
	  sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
	  status = medium_position (s->fd);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (21, "MEDIUM POSITION failed\n");
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (SANE_STATUS_INVAL);
	    }
	  DBG (21, "AF_NOW before = '%d'\n", s->AF_NOW);
	  s->AF_NOW = SANE_TRUE;
	  DBG (21, "AF_NOW after = '%d'\n", s->AF_NOW);
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return status;

	case OPT_CUSTOM_GAMMA:
	  w = *(SANE_Word *) val;

	  if (w == s->val[OPT_CUSTOM_GAMMA].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  s->val[OPT_CUSTOM_GAMMA].w = w;
	  if (w)
	    {
	      const char *mode = s->val[OPT_MODE].s;

	      if (!strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY))
		s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      else if (!strcmp (mode, SANE_VALUE_SCAN_MODE_COLOR)
	      || !strcmp (mode, SANE_I18N("Fine color")))
		{
		  s->opt[OPT_CUSTOM_GAMMA_BIND].cap &= ~SANE_CAP_INACTIVE;
		  if (s->val[OPT_CUSTOM_GAMMA_BIND].w)
		    {
		      s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
		    }
		  else
		    {
		      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
		      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
		    }
		}
	      s->opt[OPT_BRIGHTNESS].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_CONTRAST].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_CUSTOM_GAMMA_BIND].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;

	      s->opt[OPT_BRIGHTNESS].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_CONTRAST].cap &= ~SANE_CAP_INACTIVE;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_CUSTOM_GAMMA_BIND:
	  w = *(SANE_Word *) val;

	  if (w == s->val[OPT_CUSTOM_GAMMA_BIND].w)
	    return SANE_STATUS_GOOD;	/* no change */

	  if (info)
	    *info |= SANE_INFO_RELOAD_OPTIONS;

	  s->val[OPT_CUSTOM_GAMMA_BIND].w = w;
	  if (w)
	    {
	      s->opt[OPT_GAMMA_VECTOR].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap |= SANE_CAP_INACTIVE;
	    }
	  else
	    {
	      s->opt[OPT_GAMMA_VECTOR].cap |= SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_R].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_G].cap &= ~SANE_CAP_INACTIVE;
	      s->opt[OPT_GAMMA_VECTOR_B].cap &= ~SANE_CAP_INACTIVE;
	    }

	  return SANE_STATUS_GOOD;

	case OPT_GAMMA_VECTOR:
	case OPT_GAMMA_VECTOR_R:
	case OPT_GAMMA_VECTOR_G:
	case OPT_GAMMA_VECTOR_B:
	  memcpy (s->val[option].wa, val, s->opt[option].size);
	  DBG (21, "setting gamma vector\n");
/*       if (info) */
/* 	*info |= SANE_INFO_RELOAD_OPTIONS; */
	  return (SANE_STATUS_GOOD);

	}
    }

  DBG (1, "<< sane_control_option %s\n", option_name[option]);
  return (SANE_STATUS_INVAL);

}

/**************************************************************************/

SANE_Status
sane_get_parameters (SANE_Handle handle, SANE_Parameters *params)
{
  CANON_Scanner *s = handle;
  DBG (1, ">> sane_get_parameters\n");

  if (!s->scanning)
    {
      int width, length, xres, yres;
      const char *mode;

      memset (&s->params, 0, sizeof (s->params));

      width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
	* s->hw->info.mud / MM_PER_INCH;
      length = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
	* s->hw->info.mud / MM_PER_INCH;

      xres = s->val[OPT_X_RESOLUTION].w;
      yres = s->val[OPT_Y_RESOLUTION].w;
      if (s->val[OPT_RESOLUTION_BIND].w || s->val[OPT_PREVIEW].w)
	yres = xres;

      /* make best-effort guess at what parameters will look like once
         scanning starts.  */
      if (xres > 0 && yres > 0 && width > 0 && length > 0)
	{
	  DBG (11, "sane_get_parameters: width='%d', xres='%d', mud='%d'\n",
	       width, xres, s->hw->info.mud);
	  s->params.pixels_per_line = width * xres / s->hw->info.mud;
	  DBG (11, "sane_get_parameters: length='%d', yres='%d', mud='%d'\n",
	       length, yres, s->hw->info.mud);
	  s->params.lines = length * yres / s->hw->info.mud;
	  DBG (11, "sane_get_parameters: pixels_per_line='%d', lines='%d'\n",
	       s->params.pixels_per_line, s->params.lines);
	}

      mode = s->val[OPT_MODE].s;
      if (!strcmp (mode, SANE_VALUE_SCAN_MODE_LINEART)
      || !strcmp (mode, SANE_VALUE_SCAN_MODE_HALFTONE))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line / 8;
	  /* workaround rounding problems */
	  s->params.pixels_per_line = s->params.bytes_per_line * 8;
	  s->params.depth = 1;
	}
      else if (!strcmp (mode, SANE_VALUE_SCAN_MODE_GRAY))
	{
	  s->params.format = SANE_FRAME_GRAY;
	  s->params.bytes_per_line = s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      else if (!strcmp (mode, SANE_VALUE_SCAN_MODE_COLOR)
      || !strcmp (mode, SANE_I18N("Fine color")))
	{
	  s->params.format = SANE_FRAME_RGB;
	  s->params.bytes_per_line = 3 * s->params.pixels_per_line;
	  s->params.depth = 8;
	}
      else
	{
	  s->params.format = SANE_FRAME_RGB;
	  s->params.bytes_per_line = 6 * s->params.pixels_per_line;
	  s->params.depth = 16;
	}
      s->params.last_frame = SANE_TRUE;
    }

  DBG (11, "sane_get_parameters: xres='%d', yres='%d', pixels_per_line='%d', "
    "bytes_per_line='%d', lines='%d'\n", s->xres, s->yres,
    s->params.pixels_per_line, s->params.bytes_per_line, s->params.lines);

  if (params)
    *params = s->params;

  DBG (1, "<< sane_get_parameters\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

SANE_Status
sane_start (SANE_Handle handle)
{
  int mode;
  char *mode_str;
  CANON_Scanner *s = handle;
  SANE_Status status;
  u_char wbuf[72], dbuf[28], ebuf[72];
  u_char cbuf[2];			/* modification for FB620S */
  size_t buf_size, i;

  char tmpfilename[] = "/tmp/canon.XXXXXX"; /* for FB1200S */
  char *thistmpfile; /* for FB1200S */

  DBG (1, ">> sane_start\n");

  s->tmpfile = -1; /* for FB1200S */

/******* making a tempfile for 1200 dpi scanning of FB1200S ******/
  if (s->hw->info.model == FB1200)
    {
      thistmpfile = strdup(tmpfilename);

      if (thistmpfile != NULL)
        {
          if (mktemp(thistmpfile) == 0)
            {  
              DBG(1, "mktemp(thistmpfile) is failed\n");
              return (SANE_STATUS_INVAL);
	    }
	}
      else
        {
	  DBG(1, "strdup(thistmpfile) is failed\n");
	  return (SANE_STATUS_INVAL);
	}

      s->tmpfile = open(thistmpfile, O_RDWR | O_CREAT | O_EXCL, 0600);

      if (s->tmpfile == -1)
	{
	  DBG(1, "error opening temp file %s\n", thistmpfile);
	  DBG(1, "errno: %i; %s\n", errno, strerror(errno));
	  errno = 0;
	  return (SANE_STATUS_INVAL);
	}
      DBG(1, " ****** tmpfile is opened ****** \n");

      unlink(thistmpfile);
      free (thistmpfile);
      DBG(1, "free thistmpfile\n");
    }
/******************************************************************/

  s->scanning = SANE_FALSE;

  if ((s->hw->adf.Status != ADF_STAT_NONE)
      && (s->val[OPT_FLATBED_ONLY].w != SANE_TRUE)
      && (s->hw->adf.Problem != 0))
    {
      DBG (3, "SCANNER ADF HAS A PROBLEM\n");
      if (s->hw->adf.Problem & 0x08)
	{
	  status = SANE_STATUS_COVER_OPEN;
	  DBG (3, "ADF Cover Open\n");
	}
      else if (s->hw->adf.Problem & 0x04)
	{
	  status = SANE_STATUS_JAMMED;
	  DBG (3, "ADF Paper Jam\n");
	}
      else			/* adf.Problem = 0x02 */
	{
	  status = SANE_STATUS_NO_DOCS;
	  DBG (3, "ADF No More Documents\n");
	}
      return status;
    }
  else if ((s->hw->adf.Status != ADF_STAT_NONE)
	   && (s->val[OPT_FLATBED_ONLY].w == SANE_TRUE))
    {
      set_adf_mode (s->fd, s->hw->adf.Priority);
      /* 2.23 define ADF Mode */
    }

  /* First make sure we have a current parameter set.  Some of the
     parameters will be overwritten below, but that's OK.  */
  status = sane_get_parameters (s, 0);
  if (status != SANE_STATUS_GOOD)
    return status;

  status = sanei_scsi_open (s->hw->sane.name, &s->fd, sense_handler, s->hw);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "open of %s failed: %s\n", s->hw->sane.name,
	sane_strstatus (status));
      return (status);
    }

#if 0				/* code moved after define_scan() calls */
  /* Do focus, but not for the preview */
  if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FOCUS_GROUP].cap)
    && !s->val[OPT_PREVIEW].w && s->AF_NOW)
    {
      if ((status = do_focus (s)) != SANE_STATUS_GOOD) return (status);
      if (s->val[OPT_AF_ONCE].w) s->AF_NOW = SANE_FALSE;
    }
#endif

  if (s->val[OPT_CUSTOM_GAMMA].w)
    {
      if ((status = do_gamma (s)) != SANE_STATUS_GOOD) return (status);
    }

  DBG (3, "attach: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = 20;
  status = get_scan_mode (s->fd, (u_char) SCAN_CONTROL_CONDITIONS,
    ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "attach: GET SCAN MODE for scan control conditions failed\n");
      sanei_scsi_close (s->fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    DBG (3, "scan mode control byte[%d] = %d\n", (int) i, ebuf[i]);

  if (s->hw->adf.Status != ADF_STAT_NONE)
    {
      DBG (3, "attach: sending GET SCAN MODE for transparency unit\n");
      memset (ebuf, 0, sizeof (ebuf));
      buf_size = 12;
      status = get_scan_mode (s->fd, (u_char) TRANSPARENCY_UNIT,
			      ebuf, &buf_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "attach: GET SCAN MODE for transparency unit failed\n");
	  sanei_scsi_close (s->fd);
	  return (SANE_STATUS_INVAL);
	}
      for (i = 0; i < buf_size; i++)
	DBG (3, "scan mode control byte[%d] = %d\n", (int) i,
	ebuf[i]);
    }

  mode_str = s->val[OPT_MODE].s;
  s->xres = s->val[OPT_X_RESOLUTION].w;
  s->yres = s->val[OPT_Y_RESOLUTION].w;

  if (s->val[OPT_RESOLUTION_BIND].w || s->val[OPT_PREVIEW].w)
      s->yres = s->xres;

  s->ulx = SANE_UNFIX (s->val[OPT_TL_X].w) * s->hw->info.mud / MM_PER_INCH;
  s->uly = SANE_UNFIX (s->val[OPT_TL_Y].w) * s->hw->info.mud / MM_PER_INCH;

  s->width = SANE_UNFIX (s->val[OPT_BR_X].w - s->val[OPT_TL_X].w)
    * s->hw->info.mud / MM_PER_INCH;
  s->length = SANE_UNFIX (s->val[OPT_BR_Y].w - s->val[OPT_TL_Y].w)
    * s->hw->info.mud / MM_PER_INCH;

  DBG (11, "s->width='%d', s->length='%d'\n", s->width, s->length);

  if (s->hw->info.model != CS2700 && s->hw->info.model != FS2710)
    {
      if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_LINEART)
      || !strcmp (mode_str, SANE_VALUE_SCAN_MODE_HALFTONE))
	s->RIF = s->val[OPT_HNEGATIVE].w;
      else
	s->RIF = !s->val[OPT_HNEGATIVE].w;
    }

  s->brightness = s->val[OPT_BRIGHTNESS].w;
  s->contrast = s->val[OPT_CONTRAST].w;
  s->threshold = s->val[OPT_THRESHOLD].w;
  s->bpp = s->params.depth;

  s->GRC = s->val[OPT_CUSTOM_GAMMA].w;
  s->Mirror = s->val[OPT_MIRROR].w;
  s->AE = s->val[OPT_AE].w;

  s->HiliteG = s->val[OPT_HILITE_G].w;
  s->ShadowG = s->val[OPT_SHADOW_G].w;
  if (s->val[OPT_BIND_HILO].w)
    {
      s->HiliteR = s->val[OPT_HILITE_G].w;
      s->ShadowR = s->val[OPT_SHADOW_G].w;
      s->HiliteB = s->val[OPT_HILITE_G].w;
      s->ShadowB = s->val[OPT_SHADOW_G].w;
    }
  else
    {
      s->HiliteR = s->val[OPT_HILITE_R].w;
      s->ShadowR = s->val[OPT_SHADOW_R].w;
      s->HiliteB = s->val[OPT_HILITE_B].w;
      s->ShadowB = s->val[OPT_SHADOW_B].w;
    }

  if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_LINEART))
    {
      mode = 4;
      s->image_composition = 0;
    }
  else if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_HALFTONE))
    {
      mode = 4;
      s->image_composition = 1;
    }
  else if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_GRAY))
    {
      mode = 5;
      s->image_composition = 2;
    }
  else if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_COLOR)
  || !strcmp (mode_str, SANE_I18N("Fine color")))
    {
      mode = 6;
      s->image_composition = 5;
    }
  else if (!strcmp (mode_str, SANE_I18N("Raw")))
    {
      mode = 6;
      s->image_composition = 5;
    }
  else
    {
      mode = 6;
      s->image_composition = 5;
    }

  memset (wbuf, 0, sizeof (wbuf));
  wbuf[7] = 64;
  wbuf[10] = s->xres >> 8;
  wbuf[11] = s->xres;
  wbuf[12] = s->yres >> 8;
  wbuf[13] = s->yres;
  wbuf[14] = s->ulx >> 24;
  wbuf[15] = s->ulx >> 16;
  wbuf[16] = s->ulx >> 8;
  wbuf[17] = s->ulx;
  wbuf[18] = s->uly >> 24;
  wbuf[19] = s->uly >> 16;
  wbuf[20] = s->uly >> 8;
  wbuf[21] = s->uly;
  wbuf[22] = s->width >> 24;
  wbuf[23] = s->width >> 16;
  wbuf[24] = s->width >> 8;
  wbuf[25] = s->width;
  wbuf[26] = s->length >> 24;
  wbuf[27] = s->length >> 16;
  wbuf[28] = s->length >> 8;
  wbuf[29] = s->length;
  wbuf[30] = s->brightness;
  wbuf[31] = s->threshold;
  wbuf[32] = s->contrast;
  wbuf[33] = s->image_composition;
  wbuf[34] = (s->hw->info.model == FS2710) ? 12 : s->bpp;
  wbuf[36] = 1;
  wbuf[37] = (1 << 7) + 0x03;
  wbuf[50] = (s->GRC << 3) | (s->Mirror << 2);
#if 1
   wbuf[50] |= s->AE;	/* AE also for preview; needed by frontend controls */
#else
  if (!s->val[OPT_PREVIEW].w) wbuf[50] |= s->AE; /* AE not during preview */
#endif
  wbuf[54] = 2;
  wbuf[57] = 1;
  wbuf[58] = 1;
  wbuf[59] = s->HiliteR;
  wbuf[60] = s->ShadowR;
  wbuf[62] = s->HiliteG;
  wbuf[64] = s->ShadowG;
  wbuf[70] = s->HiliteB;
  wbuf[71] = s->ShadowB;

  DBG (7, "RIF=%d, GRC=%d, Mirror=%d, AE=%d, Speed=%d\n", s->RIF, s->GRC,
    s->Mirror, s->AE, s->scanning_speed);
  DBG (7, "HR=%d, SR=%d, HG=%d, SG=%d, HB=%d, SB=%d\n", s->HiliteR,
    s->ShadowR, s->HiliteG, s->ShadowG, s->HiliteB, s->ShadowB);

  if (s->hw->info.model == FB620)	/* modification for FB620S */
    {
      wbuf[36] = 0;
      wbuf[37] = (s->RIF << 7) + 0x3;
      wbuf[50] = s->GRC << 3;
      wbuf[54] = 0;
      wbuf[57] = 0;
      wbuf[58] = 0;
    }
  else if (s->hw->info.model == FB1200)	/* modification for FB1200S */
    {
#if 0
      wbuf[34] = (((600 < s->val[OPT_X_RESOLUTION].w)
	|| (600 < s->val[OPT_Y_RESOLUTION].w))
	&& (strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_LINEART) != 0))
	? 12 : s->bpp;
#endif
      wbuf[36] = 0;
      wbuf[37] = (s->RIF << 7) + 0x3;
      wbuf[50] = (1 << 4) | (s->GRC << 3);
      wbuf[57] = 1;
      wbuf[58] = 1;
    }
  else if (s->hw->info.model == IX4015) /* modification for IX-4015 */
    {
      wbuf[36] = 0;
      wbuf[37] = (s->RIF << 7);
      wbuf[57] = 0;
      wbuf[58] = 0;
      /* no highlight and shadow control */
      wbuf[59] = 0;
      wbuf[60] = 0;
      wbuf[62] = 0;
      wbuf[64] = 0;
      wbuf[70] = 0;
      wbuf[71] = 0;
    }

  buf_size = sizeof (wbuf);
  status = set_window (s->fd, wbuf);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "SET WINDOW failed: %s\n", sane_strstatus (status));
      return (status);
    }
  if (s->hw->info.model == FS2710)
    status = set_parameters_fs2710 (s);
  buf_size = sizeof (wbuf);
  memset (wbuf, 0, buf_size);
  status = get_window (s->fd, wbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET WINDOW failed: %s\n", sane_strstatus (status));
      return (status);
    }
  DBG (5, "xres=%d\n", (wbuf[10] << 8) + wbuf[11]);
  DBG (5, "yres=%d\n", (wbuf[12] << 8) + wbuf[13]);
  DBG (5, "ulx=%d\n", (wbuf[14] << 24) + (wbuf[15] << 16) + (wbuf[16] << 8)
    + wbuf[17]);
  DBG (5, "uly=%d\n", (wbuf[18] << 24) + (wbuf[19] << 16) + (wbuf[20] << 8)
    + wbuf[21]);
  DBG (5, "width=%d\n", (wbuf[22] << 24) + (wbuf[23] << 16) + (wbuf[24] << 8)
    + wbuf[25]);
  DBG (5, "length=%d\n", (wbuf[26] << 24) + (wbuf[27] << 16) + (wbuf[28] << 8)
    + wbuf[29]);
  DBG (5, "Highlight Red=%d\n", wbuf[59]);
  DBG (5, "Shadow Red=%d\n", wbuf[60]);
  DBG (5, "Highlight (Green)=%d\n", wbuf[62]);
  DBG (5, "Shadow (Green)=%d\n", wbuf[64]);
  DBG (5, "Highlight Blue=%d\n", wbuf[70]);
  DBG (5, "Shadow Blue=%d\n", wbuf[71]);

  if (s->hw->tpu.Status == TPU_STAT_ACTIVE || s->hw->info.is_filmscanner)
    {
      DBG (3, "sane_start: sending DEFINE SCAN MODE for transparency unit, "
	"NP=%d, Negative film type=%d\n", !s->RIF, s->negative_filmtype);
      memset (wbuf, 0, sizeof (wbuf));
      wbuf[0] = 0x02;
      wbuf[1] = 6;
      wbuf[2] = 0x80;
      wbuf[3] = 0x05;
      wbuf[4] = 39;
      wbuf[5] = 16;
      wbuf[6] = !s->RIF;
      wbuf[7] = s->negative_filmtype;
      status = define_scan_mode (s->fd, TRANSPARENCY_UNIT, wbuf);
      /* note: If we implement a TPU for the FB1200S, we need
         TRANSPARENCY_UNIT_FB1200 here. */
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "define scan mode failed: %s\n", sane_strstatus (status));
	  return (status);
	}
    }

  DBG (3, "sane_start: sending DEFINE SCAN MODE for scan control "
    "conditions\n");
  memset (wbuf, 0, sizeof (wbuf));
  wbuf[0] = 0x20;
  if (s->hw->info.model == FB1200)
    {
      wbuf[1] = 17;
      wbuf[16] = 3;
      wbuf[17] = 8;
      wbuf[18] = (1 << 7) | (1 << 3);

      DBG (3, "sane_start: sending DEFINE SCAN MODE for scan control "
	"conditions of FB1200\n");
      status = define_scan_mode (s->fd, SCAN_CONTROL_CON_FB1200, wbuf);
    }
  else
    {
      wbuf[1] = 14;
      /* For preview use always normal speed: */
      if (!s->val[OPT_PREVIEW].w && s->hw->info.is_filmscanner)
	wbuf[11] = s->scanning_speed;
      wbuf[15] = (s->hw->info.model == FB620
	&& !strcmp (mode_str, SANE_I18N("Fine color"))
	&& !s->val[OPT_PREVIEW].w) ? 1 << 3 : 0;
      status = define_scan_mode (s->fd, SCAN_CONTROL_CONDITIONS, wbuf);
    }
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "define scan mode failed: %s\n", sane_strstatus (status));
      return (status);
    }

  DBG (3, "sane_start: sending GET SCAN MODE for scan control conditions\n");
  memset (ebuf, 0, sizeof (ebuf));
  buf_size = sizeof (ebuf);
  status = get_scan_mode (s->fd, SCAN_CONTROL_CONDITIONS, ebuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "sane_start: GET SCAN MODE for scan control conditions "
	"failed\n");
      sanei_scsi_close (s->fd);
      return (SANE_STATUS_INVAL);
    }
  for (i = 0; i < buf_size; i++)
    DBG (3, "scan mode byte[%d] = %d\n", (int) i, ebuf[i]);

  /* Focus, but not for previews or negatives with speed control */
  if (SANE_OPTION_IS_ACTIVE(s->opt[OPT_FOCUS_GROUP].cap)
    && !s->val[OPT_PREVIEW].w && s->AF_NOW
    && (s->RIF || s->AE || s->scanning_speed == 0))
    {
      if ((status = do_focus (s)) != SANE_STATUS_GOOD) return (status);
      if (s->val[OPT_AF_ONCE].w) s->AF_NOW = SANE_FALSE;
    }

  /* ============= modification for FB620S ============= */
  DBG (3, "TEST_UNIT_READY\n");
  status = test_unit_ready (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "test unit ready failed (%s)\n", sane_strstatus (status));
      sanei_scsi_close (s->fd);
      s->fd = -1;
      return (status);
    }

  if (s->hw->info.can_calibrate)
    {
      DBG (3, "sane_start: sending GET_CALIBRATION_STATUS\n");
      buf_size = sizeof (cbuf);
      memset (cbuf, 0, buf_size);
      status = get_calibration_status (s->fd, cbuf, &buf_size);
      if (status != SANE_STATUS_GOOD)
	{
	  DBG (1, "sane_start: GET_CALIBRATION_STATUS failed: %s\n",
	       sane_strstatus (status));
	  sanei_scsi_close (s->fd);
	  s->fd = -1;
	  return (status);
	}

      DBG (1, "cbuf[0] = %d\n", cbuf[0]);
      DBG (1, "cbuf[1] = %d\n", cbuf[1]);

      cbuf[0] &= 3;
      if (cbuf[0] == 1 || cbuf[0] == 2 || cbuf[0] == 3)
	{
	  status = execute_calibration (s->fd);
	  DBG (3, "sane_start: EXECUTE_CALIBRATION\n");
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "sane_start: EXECUTE_CALIBRATION failed\n");
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (status);
	    }

	  DBG (3, "after calibration: GET_CALIBRATION_STATUS\n");
	  buf_size = sizeof (cbuf);
	  memset (cbuf, 0, buf_size);
	  status = get_calibration_status (s->fd, cbuf, &buf_size);
	  if (status != SANE_STATUS_GOOD)
	    {
	      DBG (1, "after calibration: GET_CALIBRATION_STATUS failed: %s\n",
		sane_strstatus (status));
	      sanei_scsi_close (s->fd);
	      s->fd = -1;
	      return (status);
	    }
	  DBG (1, "cbuf[0] = %d\n", cbuf[0]);
	  DBG (1, "cbuf[1] = %d\n", cbuf[1]);
	}
    }

  status = scan (s->fd);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "start of scan failed: %s\n", sane_strstatus (status));
      return (status);
    }

  buf_size = sizeof (dbuf);
  memset (dbuf, 0, buf_size);
  status = get_data_status (s->fd, dbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET DATA STATUS failed: %s\n", sane_strstatus (status));
      return (status);
    }
  DBG (5, ">> GET DATA STATUS\n");
  DBG (5, "Scan Data Available=%d\n", (dbuf[9] << 16) + (dbuf[10] << 8)
       + dbuf[11]);
  DBG (5, "Magnified Width=%d\n", (dbuf[12] <<24) + (dbuf[13] << 16)
       + (dbuf[14] << 8) + dbuf[15]);
  DBG (5, "Magnified Length=%d\n", (dbuf[16] << 24) + (dbuf[17] << 16)
       + (dbuf[18] << 8) + dbuf[19]);
  DBG (5, "Rest Data=%d bytes\n", (dbuf[20] << 24) + (dbuf[21] << 16)
       + (dbuf[22] << 8) + dbuf[23]);
  DBG (5, "Filled Data Buffer=%d\n", (dbuf[24] << 24) + (dbuf[25] << 16)
       + (dbuf[26] << 8) + dbuf[27]);
  DBG (5, "<< GET DATA STATUS\n");

  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

  if (s->hw->info.model == FB1200)
    {
      if (s->bytes_to_read != (((size_t) dbuf[9] << 16)
      + ((size_t) dbuf[10] << 8) + (size_t) dbuf[11]))
	{
	  s->params.bytes_per_line = (((size_t) dbuf[12] << 24)
	    + ((size_t) dbuf[13] << 16) + ((size_t) dbuf[14] << 8)
	    + (size_t)dbuf[15]);
	  s->params.lines = (((size_t) dbuf[16] << 24)
	    + ((size_t) dbuf[17] << 16) + ((size_t) dbuf[18] << 8)
	    + (size_t) dbuf[19]);
	  s->bytes_to_read = s->params.bytes_per_line * s->params.lines;

	  mode_str = s->val[OPT_MODE].s;
	  if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_LINEART))
	    {
	      if (((600 < s->val[OPT_X_RESOLUTION].w)
		|| (600 < s->val[OPT_Y_RESOLUTION].w)))
		{
		  s->params.bytes_per_line *= 2;
		  s->params.lines /= 2;
		}
	      s->params.pixels_per_line = s->params.bytes_per_line * 8;
	    }
	  else if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_GRAY))
	      s->params.pixels_per_line = s->params.bytes_per_line;
	  else if (!strcmp (mode_str, SANE_VALUE_SCAN_MODE_COLOR)
	    || !strcmp (mode_str, SANE_I18N("Fine color")))
	      s->params.pixels_per_line = s->params.bytes_per_line / 3;
	  else
	    s->params.pixels_per_line = s->params.bytes_per_line / 6;
	}
    }

  DBG (1, "%d pixels per line, %d bytes, %d lines high, total %lu bytes, "
       "dpi=%d\n", s->params.pixels_per_line, s->params.bytes_per_line,
       s->params.lines, (u_long) s->bytes_to_read,
       s->val[OPT_X_RESOLUTION].w);

/**************************************************/
/* modification for FB620S and FB1200S */
  s->buf_used = 0;
  s->buf_pos = 0;
/**************************************************/

  s->scanning = SANE_TRUE;

  DBG (1, "<< sane_start\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
sane_read_direct (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
		  SANE_Int *len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  size_t nread;

  DBG (21, ">> sane_read\n");

  *len = 0;
  nread = max_len;

  DBG (21, "   sane_read: nread=%d, bytes_to_read=%d\n", (int) nread,
  (int) s->bytes_to_read);
  if (s->bytes_to_read == 0)
    {
      do_cancel (s);
      return (SANE_STATUS_EOF);
    }

  if (!s->scanning) return (do_cancel (s));

  if (nread > s->bytes_to_read) nread = s->bytes_to_read;
  status = read_data (s->fd, buf, &nread);
  if (status != SANE_STATUS_GOOD)
    {
      do_cancel (s);
      return (SANE_STATUS_IO_ERROR);
    }
  *len = nread;
  s->bytes_to_read -= nread;

  DBG (21, "   sane_read: nread=%d, bytes_to_read=%d\n", (int) nread,
  (int) s->bytes_to_read);
  DBG (21, "<< sane_read\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/

static SANE_Status
read_fs2710 (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
	     SANE_Int *len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  int c;
  size_t i, nread, nread2;
  u_char *p;
#if defined(WORDS_BIGENDIAN)
  u_char b;
#endif

  DBG (21, ">> sane_read\n");

  *len = 0;
  nread = max_len;

  DBG (21, "   sane_read: nread=%d, bytes_to_read=%d\n", (int) nread,
  (int) s->bytes_to_read);

  if (nread > s->bytes_to_read) nread = s->bytes_to_read;
  if (s->bytes_to_read == 0)
    {
      do_cancel (s);
      return (SANE_STATUS_EOF);
    }

  if (!s->scanning) return (do_cancel (s));

  /* We must receive 2 little-endian bytes per pixel and colour.
     In raw mode we must swap the bytes if we are running a big-endian
     architecture (SANE standard 3.2.1), and pass them both.
     Otherwise the other subroutines expect only 1 byte, so we must
     set up an intermediate buffer which is twice as large
     as buf, and then map this buffer to buf. */

  if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR))
    {
      if (max_len > s->auxbuf_len)
	{				/* extend buffer? */
	  if (s->auxbuf_len > 0) free (s->auxbuf);
	  s->auxbuf_len = max_len;
	  if ((s->auxbuf = (u_char *) malloc (2 * max_len)) == NULL)
	    {
	      DBG (1, "sane_read buffer size insufficient\n");
	      do_cancel (s);
	      return SANE_STATUS_NO_MEM;
	    }
	}

      nread2 = 2 * nread;
      if ((status = read_data (s->fd, s->auxbuf, &nread2)) != SANE_STATUS_GOOD)
	{
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}
      nread = nread2 / 2;
      for (i = 0, p = s->auxbuf; i < nread; i++)
	{
	  c = *p++ >> 4;
	  c |= *p++ << 4;
	  *buf++ = s->gamma_map[s->colour++][c];
	  if (s->colour > 3) s->colour = 1;	/* cycle through RGB */
	}
    }
  else
    {
      if ((status = read_data (s->fd, buf, &nread)) != SANE_STATUS_GOOD)
	{
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}
#if defined(WORDS_BIGENDIAN)
      for (p = buf; p < buf + nread; p++)
	{
	  b = *p;
	  *p = *(p + 1);
	  p++;
	  *p = b;
	}
#endif
    }
  *len = nread;
  s->bytes_to_read -= nread;

  DBG (21, "   sane_read: nread=%d, bytes_to_read=%d\n", (int) nread,
  (int) s->bytes_to_read);
  DBG (21, "<< sane_read\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/
/* modification for FB620S */

static SANE_Status
read_fb620 (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
	    SANE_Int *len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  SANE_Byte *out, *red, *green, *blue;
  SANE_Int ncopy;
  size_t nread = 0, i, pixel_per_line;

  DBG (21, ">> read_fb620\n");

  *len = 0;

  DBG (21, "   read_fb620: nread=%d, bytes_to_read=%d\n", (int) nread,
  (int) s->bytes_to_read);

  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used)
    {
      s->reset_flag = 0;	/* no reset */

      do_cancel (s);
      DBG (21, "do_cancel(EOF)\n");
      DBG (21, "reset_flag = %d\n", s->reset_flag);
      return (SANE_STATUS_EOF);
    }
  else
    {
      s->reset_flag = 1;	/* do reset */
      DBG (21, "reset_flag = %d\n", s->reset_flag);
    }
  DBG (21, "   read_fb620: buf_pos=%d, buf_used=%d\n", s->buf_pos,
       s->buf_used);

  if (!s->scanning)
    return (do_cancel (s));

  if (s->buf_pos < s->buf_used)
    {
      ncopy = s->buf_used - s->buf_pos;
      if (ncopy > max_len)
	ncopy = max_len;
      memcpy (buf, &(s->outbuffer[s->buf_pos]), ncopy);

      max_len -= ncopy;
      *len += ncopy;
      buf = &(buf[ncopy]);
      s->buf_pos += ncopy;
    }

  if (s->buf_pos >= s->buf_used && s->bytes_to_read)
    {
      /* buffer is empty: read in scan line and sort color data as shown
         above */

      nread = s->params.bytes_per_line;

      if (nread > s->bytes_to_read)
	nread = s->bytes_to_read;

      status = read_data (s->fd, s->inbuffer, &nread);

      if (status != SANE_STATUS_GOOD)
	{
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}

      s->buf_used = s->params.bytes_per_line;

      out = s->outbuffer;
      pixel_per_line = s->params.pixels_per_line;

      red = s->inbuffer;
      green = &(s->inbuffer[pixel_per_line]);
      blue = &(s->inbuffer[2 * pixel_per_line]);

      for (i = 0; i < pixel_per_line; i++)
	{
	  *out++ = *red++;
	  *out++ = *green++;
	  *out++ = *blue++;
	}

      s->buf_pos = 0;

      s->bytes_to_read -= s->buf_used;

    }

  if (max_len && s->buf_pos < s->buf_used)
    {
      ncopy = s->buf_used - s->buf_pos;
      if (ncopy > max_len)
	ncopy = max_len;
      memcpy (buf, &(s->outbuffer[s->buf_pos]), ncopy);
      *len += ncopy;
      s->buf_pos += ncopy;
    }

  DBG (21, "<< read_fb620\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/
/* modification for FB1200S */

static SANE_Status
read_fb1200 (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
SANE_Int *len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  SANE_Byte *firstimage, *secondimage/*, inmask, outmask, outbyte,
	    primaryHigh[256], primaryLow[256], secondaryHigh[256],
	    secondaryLow[256] */;
  SANE_Int ncopy;
  u_char dbuf[28];
  size_t buf_size, nread, remain, nwritten, nremain, pos, pix, pixel_per_line,
	 byte, byte_per_line/*, bit*/;
  ssize_t wres, readres;
  int maxpix;

  DBG (21, ">> read_fb1200\n");

  buf_size = sizeof (dbuf);
  memset (dbuf, 0, buf_size);
  status = get_data_status (s->fd, dbuf, &buf_size);
  if (status != SANE_STATUS_GOOD)
    {
      DBG (1, "GET DATA STATUS failed: %s\n", sane_strstatus (status));
      return (status);
    }
  DBG (5, ">> GET DATA STATUS\n");
  DBG (5, "Scan Data Available=%d\n", (dbuf[9] << 16) + (dbuf[10] << 8)
    + dbuf[11]);
  DBG (5, "Rest Data=%d bytes\n", (dbuf[20] << 24) + (dbuf[21] << 16)
    + (dbuf[22] << 8) + dbuf[23]);
  DBG (5, "Filled Data Buffer=%d\n", (dbuf[24] << 24) + (dbuf[25] << 16)
    + (dbuf[26] << 8) + dbuf[27]);
  DBG (5, "temp file position:%u\n", (unsigned int) lseek(s->tmpfile,
  0, SEEK_CUR));
  DBG (5, "<< GET DATA STATUS\n");

  *len = 0;

  DBG (21, "   read_fb1200: bytes_to_read=%d\n",
  (int) s->bytes_to_read);

  if (s->bytes_to_read == 0 && s->buf_pos == s->buf_used)
    {
      do_cancel (s);
      DBG (21, "do_cancel(EOF)\n");
      return (SANE_STATUS_EOF);
    }

  DBG (21, "   read_fb1200: buf_pos=%d, buf_used=%d\n", s->buf_pos,
       s->buf_used);

  if (!s->scanning)
    return (do_cancel (s));

  if (s->buf_pos >= s->buf_used && s->bytes_to_read)
    {
      nread = s->params.bytes_per_line / 2;

      if (nread > s->bytes_to_read)
	nread = s->bytes_to_read;

      status = read_data (s->fd, s->inbuffer, &nread);

      if (status != SANE_STATUS_GOOD)
	{
	  do_cancel (s);
	  return (SANE_STATUS_IO_ERROR);
	}

      /**** save the primary scan data to tmpfile ****/

      if ((SANE_Int) s->bytes_to_read > s->params.bytes_per_line
	* s->params.lines / 2)
	{
	  remain = nread;
	  nwritten = 0;
	  while (remain)
	    {
	      errno = 0;
	      wres = write (s->tmpfile, &s->inbuffer[nwritten], remain);
	      if (wres == -1)
		{
		  DBG(1, "error write tmp file: %i, %s\n", errno,
		    strerror(errno));
		  do_cancel(s);
		  return (SANE_STATUS_NO_MEM);
		}
	      remain -= wres;
	      nwritten += wres;
	    }

	  s->bytes_to_read -= nread;

	  if ((SANE_Int) s->bytes_to_read <= s->params.bytes_per_line
	    * s->params.lines / 2)
	    {
	      if ((SANE_Int) s->bytes_to_read < s->params.bytes_per_line
		* s->params.lines / 2)
		DBG(1, "warning: read more data for the primary scan "
		  "than expected\n");

	      lseek (s->tmpfile, 0L, SEEK_SET);
	      *len = 0;
	      *buf = 0;
	      return (SANE_STATUS_GOOD);
	    }

	  DBG(1, "writing: the primary data to tmp file\n");
	  *len = 0;
	  *buf = 0;
	  return (SANE_STATUS_GOOD);
	}
      /** the primary scan data from tmpfile and the secondary scan data
	  are merged **/

      s->buf_used = s->params.bytes_per_line;
      byte_per_line = s->params.bytes_per_line;
      pixel_per_line = s->params.pixels_per_line;


      /** read an entire scan line from the primary scan **/

      remain = nread;
      pos = 0;
      firstimage = &(s->inbuffer[byte_per_line/2]);

      while (remain > 0)
	{
	  nremain = (remain < SSIZE_MAX)? remain: SSIZE_MAX;
	  errno = 0;
	  readres = read (s->tmpfile, &(firstimage[pos]), nremain);
	  if (readres == -1)
	    {
	      DBG(1, "error reading tmp file: %i %s\n", errno,
		strerror(errno));
	      do_cancel(s);
	      return (SANE_STATUS_IO_ERROR);
	    }
	  if (readres == 0)
	    {
	      DBG(1, "0 byte read from temp file. premature EOF?\n");
	      return (SANE_STATUS_INVAL);
	      /* perhaps an error return? */
	    }
	  DBG(1, "reading: the primary data from tmp file\n");
	  remain -= readres;
	  pos += readres;
	}

      secondimage = s->inbuffer;

      if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_COLOR))
	{
	  maxpix = pixel_per_line / 2;
	  for (pix = 0; (int) pix < maxpix; pix++)
	    {
	      s->outbuffer[6 * pix] = secondimage[3 * pix];
	      s->outbuffer[6 * pix + 1] = secondimage[3 * pix + 1];
	      s->outbuffer[6 * pix + 2] = secondimage[3 * pix + 2];
	      s->outbuffer[6 * pix + 3] = firstimage[3 * pix];
	      s->outbuffer[6 * pix + 4] = firstimage[3 * pix + 1];
	      s->outbuffer[6 * pix + 5] = firstimage[3 * pix + 2];
	    }
	}
      else if (!strcmp (s->val[OPT_MODE].s, SANE_VALUE_SCAN_MODE_GRAY))
	{
	  for (pix = 0; pix < pixel_per_line / 2; pix++)
	    {
	      s->outbuffer[2 * pix] = secondimage[pix];
	      s->outbuffer[2 * pix + 1] = firstimage[pix];
	    }
	}
      else /* for lineart mode */
	{
	  maxpix = byte_per_line / 2;
	  for (byte = 0; (int) byte < maxpix; byte++)
	    {
	      s->outbuffer[2 * byte] = primaryHigh[firstimage[byte]]
		| secondaryHigh[secondimage[byte]];
	      s->outbuffer[2 * byte + 1] = primaryLow[firstimage[byte]]
		| secondaryLow[secondimage[byte]];
#if 0
	      inmask = 128;
	      outmask = 128;
	      outbyte = 0;
	      for (bit = 0; bit < 4; bit++)
		{
		  if (inmask == (secondimage[byte] & inmask))
		    outbyte = outbyte | outmask;
		  outmask = outmask >> 1;
		  if (inmask == (firstimage[byte] & inmask))
		    outbyte = outbyte | outmask;
		  outmask = outmask >> 1;
		  inmask = inmask >> 1;
		}
	      s->outbuffer[2 * byte] = outbyte;

	      outmask = 128;
	      outbyte = 0;
	      for (bit = 0; bit < 4; bit++)
		{
		  if (inmask == (secondimage[byte] & inmask))
		    outbyte = outbyte | outmask;
		  outmask = outmask >> 1;
		  if (inmask == (firstimage[byte] & inmask))
		    outbyte = outbyte | outmask;
		  outmask = outmask >> 1;
		  inmask = inmask >> 1;
		}
	      s->outbuffer[2 * byte + 1] = outbyte;
#endif
	    }
	}

      s->buf_pos = 0;
      s->bytes_to_read -= nread;
    }

  if (max_len && s->buf_pos < s->buf_used)
    {
      ncopy = s->buf_used - s->buf_pos;
      if (ncopy > max_len)
	ncopy = max_len;
      memcpy (buf, &(s->outbuffer[s->buf_pos]), ncopy * 2);
      *len += ncopy;
      s->buf_pos += ncopy;
    }

  DBG (21, "<< read_fb1200\n");
  return (SANE_STATUS_GOOD);
}

/**************************************************************************/
SANE_Status
sane_read (SANE_Handle handle, SANE_Byte *buf, SANE_Int max_len,
	   SANE_Int *len)
{
  CANON_Scanner *s = handle;
  SANE_Status status;
  if (s->hw->info.model == FB620 && s->params.format == SANE_FRAME_RGB)
    status = read_fb620 (handle, buf, max_len, len);
  else if (s->hw->info.model == FS2710)
    status = read_fs2710 (handle, buf, max_len, len);
  else if (s->hw->info.model == FB1200 && ((600 < s->val[OPT_X_RESOLUTION].w)
    || (600 < s->val[OPT_Y_RESOLUTION].w)))
    status = read_fb1200 (handle, buf, max_len, len);
  else
    status = sane_read_direct (handle, buf, max_len, len);
  if (s->time0 == -1)
    s->time0 = 0;
  else
    time (&(s->time0));

  DBG (11, "sane_read: time0 = %ld\n", s->time0);
  s->switch_preview = s->val[OPT_PREVIEW].w;
  return (status);
}

/**************************************************************************/

void
sane_cancel (SANE_Handle handle)
{
  CANON_Scanner *s = handle;
  DBG (1, ">> sane_cancel\n");

/******** for FB1200S ************/
  if(s->hw->info.model == FB1200)
   {
    if (s->tmpfile != -1)
     {
      close (s->tmpfile);
      DBG(1, " ****** tmpfile is closed ****** \n");
     }
    else
     {
      DBG(1, "tmpfile is failed\n");
/*    return (SANE_STATUS_INVAL);*/
     }
   }
/*********************************/

  s->scanning = SANE_FALSE;
  DBG (1, "<< sane_cancel\n");
}

/**************************************************************************/

SANE_Status
sane_set_io_mode (SANE_Handle UNUSEDARG handle,
SANE_Bool UNUSEDARG non_blocking)
{
  DBG (1, ">> sane_set_io_mode\n");
  DBG (1, "<< sane_set_io_mode\n");
  return SANE_STATUS_UNSUPPORTED;
}

/**************************************************************************/

SANE_Status
sane_get_select_fd (SANE_Handle UNUSEDARG handle,
SANE_Int UNUSEDARG * fd)
{
  DBG (1, ">> sane_get_select_fd\n");
  DBG (1, "<< sane_get_select_fd\n");

  return SANE_STATUS_UNSUPPORTED;
}

/**************************************************************************/

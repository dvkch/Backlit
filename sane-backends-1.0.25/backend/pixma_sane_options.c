/* Automatically generated from pixma_sane.c */
static const SANE_Range constraint_gamma_table = 
  { 0,255,0 };
static const SANE_Range constraint_gamma = 
  { SANE_FIX(0.3),SANE_FIX(5),SANE_FIX(0) };
static const SANE_Range constraint_threshold = 
  { 0,100,1 };
static const SANE_Range constraint_threshold_curve = 
  { 0,127,1 };


static
int find_string_in_list(SANE_String_Const str, const SANE_String_Const *list)
{
  int i;
  for (i = 0; list[i] && strcmp(str, list[i]) != 0; i++) {}
  return i;
}

static
int build_option_descriptors(struct pixma_sane_t *ss)
{
  SANE_Option_Descriptor *sod;
  option_descriptor_t *opt;

  memset(OPT_IN_CTX, 0, sizeof(OPT_IN_CTX));

  opt = &(OPT_IN_CTX[opt_opt_num_opts]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_TITLE_NUM_OPTIONS;
  sod->desc = SANE_DESC_NUM_OPTIONS;
  sod->name = "";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_opt_num_opts].info = 0;
  opt->def.w = opt_last;
  opt->val.w = opt_last;

  opt = &(OPT_IN_CTX[opt__group_1]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_GROUP;
  sod->title = SANE_I18N("Scan mode");
  sod->desc = sod->title;

  opt = &(OPT_IN_CTX[opt_resolution]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_TITLE_SCAN_RESOLUTION;
  sod->desc = SANE_DESC_SCAN_RESOLUTION;
  sod->name = "resolution";
  sod->unit = SANE_UNIT_DPI;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_WORD_LIST;
  sod->constraint.word_list = ss->dpi_list;
  OPT_IN_CTX[opt_resolution].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.w = 75;
  opt->val.w = 75;

  opt = &(OPT_IN_CTX[opt_mode]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_STRING;
  sod->title = SANE_TITLE_SCAN_MODE;
  sod->desc = SANE_DESC_SCAN_MODE;
  sod->name = "mode";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 31;
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  sod->constraint.string_list = ss->mode_list;
  OPT_IN_CTX[opt_mode].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.s = SANE_VALUE_SCAN_MODE_COLOR;
  opt->val.w = find_string_in_list(opt->def.s, sod->constraint.string_list);

  opt = &(OPT_IN_CTX[opt_source]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_STRING;
  sod->title = SANE_TITLE_SCAN_SOURCE;
  sod->desc = SANE_I18N("Selects the scan source (such as a document-feeder). Set source before mode and resolution. Resets mode and resolution to auto values.");
  sod->name = "source";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 31;
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT;
  sod->constraint_type = SANE_CONSTRAINT_STRING_LIST;
  sod->constraint.string_list = ss->source_list;
  OPT_IN_CTX[opt_source].info = 0;
  opt->def.s = SANE_I18N("Flatbed");
  opt->val.w = find_string_in_list(opt->def.s, sod->constraint.string_list);

  opt = &(OPT_IN_CTX[opt_button_controlled]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_BOOL;
  sod->title = SANE_I18N("Button-controlled scan");
  sod->desc = SANE_I18N("When enabled, scan process will not start immediately. To proceed, press \"SCAN\" button (for MP150) or \"COLOR\" button (for other models). To cancel, press \"GRAY\" button.");
  sod->name = "button-controlled";
  sod->unit = SANE_UNIT_NONE;
  sod->size = sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_button_controlled].info = 0;
  opt->def.w = SANE_FALSE;
  opt->val.w = SANE_FALSE;

  opt = &(OPT_IN_CTX[opt__group_2]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_GROUP;
  sod->title = SANE_I18N("Gamma");
  sod->desc = sod->title;

  opt = &(OPT_IN_CTX[opt_custom_gamma]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_BOOL;
  sod->title = SANE_TITLE_CUSTOM_GAMMA;
  sod->desc = SANE_DESC_CUSTOM_GAMMA;
  sod->name = "custom-gamma";
  sod->unit = SANE_UNIT_NONE;
  sod->size = sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_custom_gamma].info = 0;
  opt->def.w = SANE_TRUE;
  opt->val.w = SANE_TRUE;

  opt = &(OPT_IN_CTX[opt_gamma_table]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_TITLE_GAMMA_VECTOR;
  sod->desc = SANE_DESC_GAMMA_VECTOR;
  sod->name = "gamma-table";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 4096 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &constraint_gamma_table;
  OPT_IN_CTX[opt_gamma_table].info = 0;

  opt = &(OPT_IN_CTX[opt_gamma]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_FIXED;
  sod->title = SANE_I18N("Gamma function exponent");
  sod->desc = SANE_I18N("Changes intensity of midtones");
  sod->name = "gamma";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &constraint_gamma;
  OPT_IN_CTX[opt_gamma].info = 0;
  opt->def.w = SANE_FIX(AUTO_GAMMA);
  opt->val.w = SANE_FIX(AUTO_GAMMA);

  opt = &(OPT_IN_CTX[opt__group_3]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_GROUP;
  sod->title = SANE_I18N("Geometry");
  sod->desc = sod->title;

  opt = &(OPT_IN_CTX[opt_tl_x]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_FIXED;
  sod->title = SANE_TITLE_SCAN_TL_X;
  sod->desc = SANE_DESC_SCAN_TL_X;
  sod->name = "tl-x";
  sod->unit = SANE_UNIT_MM;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &ss->xrange;
  OPT_IN_CTX[opt_tl_x].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.w = SANE_FIX(0);
  opt->val.w = SANE_FIX(0);

  opt = &(OPT_IN_CTX[opt_tl_y]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_FIXED;
  sod->title = SANE_TITLE_SCAN_TL_Y;
  sod->desc = SANE_DESC_SCAN_TL_Y;
  sod->name = "tl-y";
  sod->unit = SANE_UNIT_MM;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &ss->yrange;
  OPT_IN_CTX[opt_tl_y].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.w = SANE_FIX(0);
  opt->val.w = SANE_FIX(0);

  opt = &(OPT_IN_CTX[opt_br_x]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_FIXED;
  sod->title = SANE_TITLE_SCAN_BR_X;
  sod->desc = SANE_DESC_SCAN_BR_X;
  sod->name = "br-x";
  sod->unit = SANE_UNIT_MM;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &ss->xrange;
  OPT_IN_CTX[opt_br_x].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.w = sod->constraint.range->max;
  opt->val.w = sod->constraint.range->max;

  opt = &(OPT_IN_CTX[opt_br_y]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_FIXED;
  sod->title = SANE_TITLE_SCAN_BR_Y;
  sod->desc = SANE_DESC_SCAN_BR_Y;
  sod->name = "br-y";
  sod->unit = SANE_UNIT_MM;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &ss->yrange;
  OPT_IN_CTX[opt_br_y].info = SANE_INFO_RELOAD_PARAMS;
  opt->def.w = sod->constraint.range->max;
  opt->val.w = sod->constraint.range->max;

  opt = &(OPT_IN_CTX[opt__group_4]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_GROUP;
  sod->title = SANE_I18N("Buttons");
  sod->desc = sod->title;

  opt = &(OPT_IN_CTX[opt_button_update]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_BUTTON;
  sod->title = SANE_I18N("Update button state");
  sod->desc = sod->title;
  sod->name = "button-update";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 0;
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_button_update].info = 0;

  opt = &(OPT_IN_CTX[opt_button_1]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Button 1");
  sod->desc = sod->title;
  sod->name = "button-1";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_button_1].info = 0;
  opt->def.w = 0;
  opt->val.w = 0;

  opt = &(OPT_IN_CTX[opt_button_2]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Button 2");
  sod->desc = sod->title;
  sod->name = "button-2";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_button_2].info = 0;
  opt->def.w = 0;
  opt->val.w = 0;

  opt = &(OPT_IN_CTX[opt_original]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Type of original to scan");
  sod->desc = sod->title;
  sod->name = "original";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_original].info = 0;
  opt->def.w = 0;
  opt->val.w = 0;

  opt = &(OPT_IN_CTX[opt_target]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Target operation type");
  sod->desc = sod->title;
  sod->name = "target";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_target].info = 0;
  opt->def.w = 0;
  opt->val.w = 0;

  opt = &(OPT_IN_CTX[opt_scan_resolution]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Scan resolution");
  sod->desc = sod->title;
  sod->name = "scan-resolution";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_DETECT|SANE_CAP_ADVANCED;
  sod->constraint_type = SANE_CONSTRAINT_NONE;
  OPT_IN_CTX[opt_scan_resolution].info = 0;
  opt->def.w = 0;
  opt->val.w = 0;

  opt = &(OPT_IN_CTX[opt__group_5]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_GROUP;
  sod->title = SANE_I18N("Extras");
  sod->desc = sod->title;

  opt = &(OPT_IN_CTX[opt_threshold]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_TITLE_THRESHOLD;
  sod->desc = SANE_DESC_THRESHOLD;
  sod->name = "threshold";
  sod->unit = SANE_UNIT_PERCENT;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &constraint_threshold;
  OPT_IN_CTX[opt_threshold].info = 0;
  opt->def.w = 50;
  opt->val.w = 50;

  opt = &(OPT_IN_CTX[opt_threshold_curve]);
  sod = &opt->sod;
  sod->type = SANE_TYPE_INT;
  sod->title = SANE_I18N("Threshold curve");
  sod->desc = SANE_I18N("Dynamic threshold curve, from light to dark, normally 50-65");
  sod->name = "threshold-curve";
  sod->unit = SANE_UNIT_NONE;
  sod->size = 1 * sizeof(SANE_Word);
  sod->cap  = SANE_CAP_SOFT_SELECT|SANE_CAP_SOFT_DETECT|SANE_CAP_AUTOMATIC|SANE_CAP_INACTIVE;
  sod->constraint_type = SANE_CONSTRAINT_RANGE;
  sod->constraint.range = &constraint_threshold_curve;
  OPT_IN_CTX[opt_threshold_curve].info = 0;

  return 0;

}


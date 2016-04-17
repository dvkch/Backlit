/* Automatically generated from pixma_sane.c */

typedef union {
  SANE_Word w;
  SANE_Int  i;
  SANE_Bool b;
  SANE_Fixed f;
  SANE_String s;
  void *ptr;
} option_value_t;

typedef enum {
  opt_opt_num_opts,
  opt__group_1,
  opt_resolution,
  opt_mode,
  opt_source,
  opt_button_controlled,
  opt__group_2,
  opt_custom_gamma,
  opt_gamma_table,
  opt_gamma,
  opt__group_3,
  opt_tl_x,
  opt_tl_y,
  opt_br_x,
  opt_br_y,
  opt__group_4,
  opt_button_update,
  opt_button_1,
  opt_button_2,
  opt_original,
  opt_target,
  opt_scan_resolution,
  opt__group_5,
  opt_threshold,
  opt_threshold_curve,
  opt_last
} option_t;


typedef struct {
  SANE_Option_Descriptor sod;
  option_value_t val,def;
  SANE_Word info;
} option_descriptor_t;


struct pixma_sane_t;
static int build_option_descriptors(struct pixma_sane_t *ss);


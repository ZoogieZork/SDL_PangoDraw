#ifndef __PJX_MARKUP_PRIVATE_H__
#define __PJX_MARKUP_PRIVATE_H__

G_BEGIN_DECLS

void pjx_markup_init ();
void pjx_markup_destroy ();

typedef struct _OpenTag OpenTag;

struct _OpenTag
{
  GSList *attrs;
  gsize start_index;
  /* Current total scale level; reset whenever
   * an absolute size is set.
   * Each "larger" ups it 1, each "smaller" decrements it 1
   */
  gint scale_level;
  /* Our impact on scale_level, so we know whether we
   * need to create an attribute ourselves on close
   */
  gint scale_level_delta;
  /* Base scale factor currently in effect
   * or size that this tag
   * forces, or parent's scale factor or size.
   */
  double base_scale_factor;
  int base_font_size;
  guint has_base_font_size : 1;
};

void pjx_markup_add_attribute_to_context(OpenTag *ot,
					 PangoAttribute *attr);

typedef struct _MarkupData MarkupData;

struct _MarkupData
{
  PangoAttrList *attr_list;
  GString *text;
  GSList *tag_stack;
  gsize index;
  GSList *to_apply;
  gunichar accel_marker;
  gunichar accel_char;
};

typedef gboolean (*TagParseFunc) (MarkupData            *md,
                                  OpenTag               *tag,
                                  const gchar          **names,
                                  const gchar          **values,
                                  GMarkupParseContext   *context,
                                  GError               **error);

void pjx_markup_add_parse_func (char *element_name, TagParseFunc parse_func);

G_END_DECLS

#endif /* __PJX_MARKUP_PRIVATE_H__ */

#ifndef __JISX4051_SHAPER_H__
#define __JISX4051_SHAPER_H__

G_BEGIN_DECLS

void
jisx451_shaper_init ();

void
jisx451_shaper_destroy ();

int
jisx4051_kerning (gunichar wc,
		  gunichar before_wc, 
		  PangoFcFont *fc_font, 
		  PangoGlyphString *glyphs, 
		  int i);

G_END_DECLS

#endif /* __JISX4051_SHAPER_H__ */

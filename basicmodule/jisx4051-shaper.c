/* Pango
 * jisx4051-shaper.c: Japanese punctuation kerning
 *
 * Copyright (c) 2005 by NAKAMURA Ken'ichi.
 * Author: NAKAMURA Ken'ichi <nakamura@sbp.fp.a.u-tokyo.ac.jp>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * This shaper handles only FULLWIDTH letter. The description of 
 * JIS X 4051-1995 about BASIC LATIN is substituted to HALFWIDTH 
 * AND FULLWIDTH FORMS.
 */

#include <glib.h>
#include <pango/pango-engine.h>
#include <pango/pango-utils.h>
#include <pango/pangofc-font.h>

/*
 * hajime-kakko-rui and owari-kakko-rui is mapped to G_UNICODE_OPEN_PUNCTUIATION
 * and G_UNICODE_CLOSE_PUNCTUATION.
 */

typedef enum {
  HAJIME_KAKKO_RUI,
  OWARI_KAKKO_RUI,
  GYOUTOU_KINSOKU_WAJI,
  KUGIRI_YAKUMONO,
  NAKATEN_RUI,
  KUTEN_RUI,
  BUNRI_KINSI_MOJI,
  IDEOGRAPHIC_SPACE, 
  ITEM_END, 
  OTHER_LETTER,
} LetterClass;
#define LETTER_CLASS_N 10

typedef struct _LetterClassCase {
  gunichar wc;
  LetterClass klass;
} LetterClassCase;

typedef enum {
  HALF_FULLWIDTH_SPACE,
  QUATER_FULLWIDTH_SPACE,
  NO_SPACE,
} SpacingSize;

typedef struct _SpacingPattern {
  SpacingSize before_spacing;
  SpacingSize after_spacing;
} SpacingPattern;

#define nn_sp {NO_SPACE, NO_SPACE,}
#define hn_sp {HALF_FULLWIDTH_SPACE, NO_SPACE,}
#define nh_sp {NO_SPACE, HALF_FULLWIDTH_SPACE,}
#define qn_sp {QUATER_FULLWIDTH_SPACE, NO_SPACE,}
#define nq_sp {NO_SPACE, QUATER_FULLWIDTH_SPACE,}
#define qq_sp {QUATER_FULLWIDTH_SPACE, QUATER_FULLWIDTH_SPACE,}
#define hq_sp {HALF_FULLWIDTH_SPACE, QUATER_FULLWIDTH_SPACE,}

static const SpacingPattern jisx4051_spacing_matrix [LETTER_CLASS_N][LETTER_CLASS_N] = {
  /* 
   HAJIME_KAKKO_RUI,
          OWARI_KAKKO_RUI,
                 GYOUTOU_KINSOKU_WAJI, 
                        KUGIRI_YAKUMONO, 
                               NAKATEN_RUI, 
                                      KUTEN_RUI, 
					     BUNRI_KINSI_MOJI, 
					            IDEOGRAPHIC_SPACE, 
						           ITEM_END,
						                  OTHER_LETTER, */
  {nn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* HAJIME_KAKKO_RUI */
  {hn_sp, nn_sp, hn_sp, hn_sp, nq_sp, nn_sp, hn_sp, nn_sp, hn_sp, hn_sp}, /* OWARI_KAKKO_RUI */
  {hn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* GYOUTOU_KINSOKU_WAJI */
  {hn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* KUGIRI_YAKUMONO */
  {qn_sp, qn_sp, qn_sp, qn_sp, qq_sp, qn_sp, qn_sp, qn_sp, qn_sp, qn_sp}, /* NAKATEN_RUI */
  {hn_sp, nn_sp, hn_sp, hn_sp, hq_sp, nn_sp, hn_sp, hn_sp, hn_sp, hn_sp}, /* KUTEN_RUI */
  {hn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* BUNRI_KINSI_MOJI */
  {nn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* IDEOGRAPHIC_SPACE */
  {nn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* ITEM_END */
  {hn_sp, nn_sp, nn_sp, nn_sp, nq_sp, nn_sp, nn_sp, nn_sp, nn_sp, nn_sp}, /* OTHER_LETTER */
};

typedef enum {
  CUT_LEFT_HALF,
  CUT_RIGHT_HALF,
  CUT_BOTH_QUARTER,
  CUT_NONE,
} TrimPattern;

static const TrimPattern trim_pattern_table [] = {
  CUT_LEFT_HALF, /* HAJIME_KAKKO_RUI */
  CUT_RIGHT_HALF, /* OWARI_KAKKO_RUI */
  CUT_NONE, /* GYOUTOU_KINSOKU_WAJI */
  CUT_NONE, /* KUGIRI_YAKUMONO */
  CUT_BOTH_QUARTER, /* NAKATEN_RUI */
  CUT_RIGHT_HALF, /* KUTEN_RUI */
  CUT_NONE, /* BUNRI_KINSI_MOJI */
  CUT_NONE, /* IDEOGRAPHIC_SPACE */
  CUT_NONE, /* ITEM_END */
  CUT_NONE, /* OTHER_LETTER */
};

static GHashTable* letter_class_hash_table;

static const gunichar hajime_kakko_rui[] = {
  0xFF08, /* FULLWIDTH LEFT PARENTHESIS */
  0xFF3B, /* FULLWIDTH LEFT SQUARE BRACKET */
  0xFF5B, /* FULLWIDTH LEFT CURLY BRACKET */
  0x2018, /* LEFT SINGLE QUOTATION MARK */
  0x201B, /* SINGLE HIGH-REVERSED-9 QUOTATION MARK */
  0x201C, /* LEFT DOUBLE QUOTATION MARK */
  0x201F, /* DOUBLE HIGH-REVERSED-9 QUOTATION MARK */
  0x3008, /* LEFT ANGLE BRACKET */
  0x300A, /* LEFT DOUBLE ANGLE BRACKET */
  0x300C, /* LEFT CORNER BRACKET */
  0x300E, /* LEFT WHITE CORNER BRACKET */
  0x3010, /* LEFT BLACK LENTICULAR BRACKET */
  0x3014, /* LEFT TORTOISE SHELL BRACKET */
  0x3016, /* LEFT WHITE LENTICULAR BRACKET */
  0x3018, /* LEFT WHITE TORTOISE SHELL BRACKET */
  0x301A, /* LEFT WHITE SQUARE BRACKET */
  0x301D, /* REVERSED DOUBLE PRIME QUOTATION MARK */
};

static const gunichar owari_kakko_rui[] = {
  0xFF09, /* FULLWIDTH RIGHT PARENTHESIS */
  0xFF0C, /* FULLWIDTH COMMA */
  0xFF3D, /* FULLWIDTH RIGHT SQUARE BRACKET */
  0xFF5D, /* FULLWIDTH RIGHT CURLY BRACKET */
  0x2019, /* RIGHT SINGLE QUOTATION MARK */
  0x201A, /* SINGLE LOW-9 QUOTATION MARK */
  0x201D, /* RIGHT DOUBLE QUOTATION MARK */
  0x201E, /* DOUBLE LOW-9 QUOTATION MARK */
  0x3001, /* IDEOGRAPHIC COMMA */
  0x3009, /* RIGHT ANGLE BRACKET */
  0x300B, /* RIGHT DOUBLE ANGLE BRACKET */
  0x300D, /* RIGHT CORNER BRACKET */
  0x300F, /* RIGHT WHITE CORNER BRACKET */
  0x3011, /* RIGHT BLACK LENTICULAR BRACKET */
  0x3015, /* RIGHT TORTOISE SHELL BRACKET */
  0x3017, /* RIGHT WHITE LENTICULAR BRACKET */
  0x3019, /* RIGHT WHITE TORTOISE SHELL BRACKET */
  0x301B, /* RIGHT WHITE SQUARE BRACKET */
  0x301E, /* DOUBLE PRIME QUOTATION MARK */
  0x301F, /* LOW DOUBLE PRIME QUOTATION MARK */
};

static const gunichar gyoutou_kinsoku_waji[] = {
    0x203C, /* double exclamation mark */
    0x2044, /* fraction slash */
    0x301C, /* WAVE DASH */
    /*
     * NOTE: WAVE DASH should be PangoLogAttr#is_char_break = 0, but not implemented
     * in pango-1.6.0.
     */
    0x3041, /* HIRAGANA LETTER SMALL A */
    0x3043, /* HIRAGANA LETTER SMALL I */
    0x3045, /* HIRAGANA LETTER SMALL U */
    0x3047, /* HIRAGANA LETTER SMALL E */
    0x3049, /* HIRAGANA LETTER SMALL O */
    0x3063, /* HIRAGANA LETTER SMALL TU */
    0x3083, /* HIRAGANA LETTER SMALL YA */
    0x3085, /* HIRAGANA LETTER SMALL YU */
    0x3087, /* HIRAGANA LETTER SMALL YO */
    0x308E, /* HIRAGANA LETTER SMALL WA */
    0x309D, /* HIRAGANA ITERATION MARK */
    0x309E, /* HIRAGANA VOICED ITERATION MARK */
    0x30A1, /* KATAKANA LETTER SMALL A */
    0x30A3, /* KATAKANA LETTER SMALL I */
    0x30A5, /* KATAKANA LETTER SMALL U */
    0x30A7, /* KATAKANA LETTER SMALL E */
    0x30A9, /* KATAKANA LETTER SMALL O */
    0x30C3, /* KATAKANA LETTER SMALL TU */
    0x30E3, /* KATAKANA LETTER SMALL YA */
    0x30E5, /* KATAKANA LETTER SMALL YU */
    0x30E7, /* KATAKANA LETTER SMALL YO */
    0x30EE, /* KATAKANA LETTER SMALL WA */
    0x30F5, /* KATAKANA LETTER SMALL KA */
    0x30F6, /* KATAKANA LETTER SMALL KE */
    0x30FC, /* KATAKANA-HIRAGANA PROLONGED SOUND MARK */
    0x30FD, /* KATAKANA ITERATION MARK */
    0x30FE, /* KATAKANA VOICED ITERATION MARK */
};

static const gunichar kugiri_yakumono[] = {
    0xFF01, /* FULLWIDTH EXCLAMATION MARK */
    0xFF1F, /* FULLWIDTH QUESTION MARK */
};

static const gunichar nakaten_rui[] = {
    0xFF1A, /* FULLWIDTH COLON */
    0xFF1B, /* FULLWIDTH SEMICOLON */
    0x30FB, /* KATAKANA MIDDLE DOT */
};

static const gunichar kuten_rui[] = {
    0xFF0E, /* FULLWIDTH FULL STOP */
    0x3002, /* IDEOGRAPHIC FULL STOP */
};

static const gunichar bunri_kinshi_moji[] = {
    0x2014, /* EM DASH */
    0x2024, /* ONE DOT LEADER */
    0x2025, /* TWO DOT LEADER */
    0x2026, /* HORIZONTAL ELLIPSIS */
};

static void
set_letter_class (gunichar wc, LetterClass klass)
{
  LetterClassCase *lcc = g_new(LetterClassCase, 1);
  lcc->wc = wc;
  lcc->klass = klass;

  g_hash_table_insert (letter_class_hash_table, &(lcc->wc), lcc);
}

void
jisx451_shaper_init ()
{
  int i;

  letter_class_hash_table = g_hash_table_new_full (g_int_hash, g_int_equal, 
    NULL, (GDestroyNotify)g_free);

#define SET_LETTER_CLASS(A, T) \
  for(i = 0; i < sizeof(A) / sizeof(gunichar); i ++) \
    set_letter_class(A[i], T)

  SET_LETTER_CLASS (gyoutou_kinsoku_waji, GYOUTOU_KINSOKU_WAJI);
  SET_LETTER_CLASS (kugiri_yakumono, KUGIRI_YAKUMONO);
  SET_LETTER_CLASS (nakaten_rui, NAKATEN_RUI);
  SET_LETTER_CLASS (kuten_rui, KUTEN_RUI);
  SET_LETTER_CLASS (bunri_kinshi_moji, BUNRI_KINSI_MOJI);
  SET_LETTER_CLASS (hajime_kakko_rui, HAJIME_KAKKO_RUI);
  SET_LETTER_CLASS (owari_kakko_rui, OWARI_KAKKO_RUI);

  set_letter_class(0x3000 /* IDEOGRAPHIC SPACE */, IDEOGRAPHIC_SPACE);
  set_letter_class(0, ITEM_END);
}

static LetterClass get_letter_class (gunichar wc)
{
  LetterClassCase *lcc = g_hash_table_lookup (letter_class_hash_table, &wc);
  if (lcc == NULL)
    return OTHER_LETTER;
  else
    return lcc->klass;
}

void
jisx451_shaper_destroy ()
{
  g_hash_table_destroy (letter_class_hash_table);
}

static void 
shift_glyph_string (PangoGlyphString *glyphs, 
		    int i)
{
  glyphs->glyphs[i + 1] = glyphs->glyphs[i];
  glyphs->log_clusters[i + 1] = glyphs->log_clusters[i];
}

static void 
add_space (PangoFcFont *fc_font, 
	   PangoGlyphString *glyphs, 
	   int i,
	   int width,
	   gboolean is_before)
{
  pango_glyph_string_set_size (glyphs, glyphs->num_glyphs + 1);
  shift_glyph_string (glyphs, i);

  glyphs->glyphs[i].glyph = 0;
  glyphs->glyphs[i].geometry.width = width;
  glyphs->glyphs[i].geometry.x_offset = 0;

  if (is_before)
    glyphs->log_clusters[i] = glyphs->log_clusters[i - 1];
  else
    {
      glyphs->log_clusters[i] = glyphs->log_clusters[i + 1];
      glyphs->glyphs[i].attr.is_cluster_start = TRUE;
      glyphs->glyphs[i + 1].attr.is_cluster_start = FALSE;
    }
}

static void 
add_space_tail (PangoFcFont *fc_font, 
		PangoGlyphString *glyphs, 
		int i,
		int width)
{
  pango_glyph_string_set_size (glyphs, glyphs->num_glyphs + 1);

  glyphs->glyphs[i].glyph = 0;
  glyphs->glyphs[i].geometry.width = width;
  glyphs->glyphs[i].geometry.x_offset = 0;
  glyphs->glyphs[i].attr.is_cluster_start = FALSE;
  glyphs->log_clusters[i] = glyphs->log_clusters[i - 1];

}

/*
  Returns num of additional letter.
*/
int
jisx4051_kerning (gunichar wc,
		  gunichar before_wc, 
		  PangoFcFont *fc_font, 
		  PangoGlyphString *glyphs, 
		  int i)
{
  LetterClass before_lc = get_letter_class (before_wc);
  LetterClass after_lc = get_letter_class (wc);
  SpacingPattern spacing_pattern = jisx4051_spacing_matrix [before_lc][after_lc];
  TrimPattern trim_pattern = trim_pattern_table [after_lc];
  int n_additional = 0;
  int fullwidth;

  if (after_lc != ITEM_END)
    fullwidth = glyphs->glyphs[i].geometry.width;
  else if (before_lc != ITEM_END)
    {
      fullwidth = glyphs->glyphs[i - 1].geometry.width;
      switch (trim_pattern_table [before_lc])
	{
	case CUT_LEFT_HALF:
	  fullwidth *= 2;
	  break;
	case CUT_RIGHT_HALF:
	  fullwidth *= 2;
	  break;
	case CUT_BOTH_QUARTER:
	  fullwidth *= 2;
	  break;
	case CUT_NONE:
	  break;
	default:
	  break;
	}
    }
  else
    return 0;

  switch (trim_pattern)
    {
    case CUT_LEFT_HALF:
      glyphs->glyphs[i].geometry.width /= 2;
      glyphs->glyphs[i].geometry.x_offset = - glyphs->glyphs[i].geometry.width;
      break;
    case CUT_RIGHT_HALF:
      glyphs->glyphs[i].geometry.width /= 2;
      break;
    case CUT_BOTH_QUARTER:
      glyphs->glyphs[i].geometry.width /= 2;
      glyphs->glyphs[i].geometry.x_offset = - glyphs->glyphs[i].geometry.width / 2;
      break;
    case CUT_NONE:
      break;
    default:
      break;
    }

  switch (spacing_pattern.before_spacing)
    {
    case HALF_FULLWIDTH_SPACE:
      if (after_lc == ITEM_END)
	add_space_tail(fc_font, glyphs, i, fullwidth / 2);
      else
	add_space (fc_font, glyphs, i, fullwidth / 2, TRUE);
      n_additional ++;
      i ++;
      break;
    case QUATER_FULLWIDTH_SPACE:
      if (after_lc == ITEM_END)
	add_space_tail(fc_font, glyphs, i, fullwidth / 4);
      else
	add_space (fc_font, glyphs, i, fullwidth / 4, TRUE);
      n_additional ++;
      i ++;
      break;
    case NO_SPACE:
      break;
    default:
      break;
    }

  switch (spacing_pattern.after_spacing)
    {
    case HALF_FULLWIDTH_SPACE:
      add_space (fc_font, glyphs, i, fullwidth / 2, FALSE);
      n_additional ++;
      i ++;
      break;
    case QUATER_FULLWIDTH_SPACE:
      add_space (fc_font, glyphs, i, fullwidth / 4, FALSE);
      n_additional ++;
      i ++;
      break;
    case NO_SPACE:
      break;
    default:
      break;
    }

  return n_additional;
}

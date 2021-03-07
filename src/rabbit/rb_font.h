/* rb_font.h
 * Helpers for generating images of text.
 * We only support monospaced fonts, and only codepoints below U+100.
 */
 
#ifndef RB_FONT_H
#define RB_FONT_H

/* The minimal font is hard-coded in a tightly packed form (192 bytes at rest)
 * This function unpacks it to a regular image containing 16 columns and 6 rows of 4x6 pixels each.
 * Glyphs are aligned against the top-left of their cell; each has a blank right and bottom edge.
 * Each output pixel is either 0xff000000 or 0x00000000.
 * Use RB_FONTCONTENT_G0 and RB_FONT_FLAG_(MARGINL|MARGINT).
 * The glyphs are ugly, almost illegible. This is meant mostly for troubleshooting.
 */
struct rb_image *rb_font_generate_minimal();

/* Generate an image from some text and a font image.
 * If (wlimit>0), we try to respect it by breaking lines between words.
 */
struct rb_image *rb_font_print(
  const struct rb_image *font,
  int fontcontent,
  int flags,
  int wlimit,
  const char *src,int srcc
);
struct rb_image *rb_font_printf(
  const struct rb_image *font,
  int fontcontent,
  int flags,
  int wlimit,
  const char *fmt,...
);

#define RB_FONTCONTENT_BYTE    0x00 /* 16x16 grid, codepoints U+0..U+ff */
#define RB_FONTCONTENT_ASCII   0x01 /* 16x8 grid, codepoints U+0..U+7f */
#define RB_FONTCONTENT_G0      0x02 /* 16x6 grid, codepoints U+20..U+7f */

#define RB_FONT_FLAG_MARGINL   0x0001 /* add 1 blank column at the left */
#define RB_FONT_FLAG_MARGINR   0x0002 /* '' right */
#define RB_FONT_FLAG_MARGINT   0x0004 /* add 1 blank row at the top */
#define RB_FONT_FLAG_MARGINB   0x0008 /* '' bottom */
#define RB_FONT_FLAG_UTF8      0x0010 /* Text is UTF-8, otherwise we assume 8859-1. */

#endif

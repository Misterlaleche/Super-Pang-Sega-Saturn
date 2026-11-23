#ifndef SPANG_FRONTEND_LAYOUT_H
#define SPANG_FRONTEND_LAYOUT_H
/* Screen IDs match frontend/core.h: version0, warning1, title2, credit3,
 * mode4, confirmed5, attract6, stage-selector7, records8,name-entry9.
 * Source is the original
 * 512x256 raster; the visible arcade window starts at64,8.
 *
 * Output is always native320x240. Text/menu panels and SUPER retain their
 * original pixels. PANG uses an arcade-raster adaptation: horizontal5/6, vertical1:1; its settled bitmap bypasses the original tile raster.
 * Reflowed text omits its unused original cells: a full repaint must first
 * fill the original palette index returned by frontend_layout_backdrop().
 * Credits changes can change the prompt width and require a full repaint.
 */
unsigned frontend_layout_backdrop(unsigned screen);
int frontend_layout_cell(unsigned screen,unsigned index,unsigned credits,
                         int *x,int *y,int *width,int *height);
/* Each source sprite is16x16. layer0=title art,1=scrolling caption,
 * 2=mode labels; this layer is stored in object byte19 by the controller. */
int frontend_layout_sprite(unsigned screen,int source_x,int source_y,unsigned layer,
                           int *x,int *y,int *width,int *height);
/* Original actor metadata in bytes20..31 locks PANG cell boundaries to the
 * final ROM pose. Native masks are sliced on those boundaries; translation
 * and per-cell arcade coordinate wrapping remain live.
 * Returns -1 for invalid metadata,0 offscreen,1 visible. */
int frontend_layout_title_object(const unsigned char object[32],
                                int *x,int *y,int *width,int *height,
                                unsigned *phase_x,unsigned *phase_y);
/* Source pixel in a floor(5*x/6) destination bin, relative to a source-cell
 * start whose phase is0..5. The same rule samples background and sprites. */
unsigned frontend_layout_title_sample(unsigned phase,unsigned pixel);
#endif

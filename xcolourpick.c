#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

/* Extra output for debugging. Also, overwrites scaled_data with red before
 * magnifying, so that it's visible if any uninitialized memory is drawn.
 */
#define V(a) 

/* Scaling parameters */
/* Having an odd number of pixels is necessary: The mouse coordinates should be
 * the center of the area that gets magnified. But if there's an even number of
 * pixels, the mouse coordinates cannot be exactly in the center, since the
 * center is in-between two pixels. This results in either pixels at the top
 * and left edges or the pixels at the bottom and right edges not getting
 * magnified.
 */
const int acPx   = 17;
const int acHalf =  8;
int mag = 4;
int scPx, scHalf;
char *scaled_data;

const char *help_msg =
"Usage: xcolourpick <options>\n"
"--decimal\tOutput colour as decimal tuple instead of hexadecimal\n"
"--factor <fac>\tSet the magnification factor; default is 4\n"
"--multiple\tDon't exit after first colour pick\n"
"--help\t\tPrint this help message and exit\n"
"Short-form options -d, -f, -m, -h are also possible.\n"
"\n"
"Mouse bindings:\n"
"Button 1\tPrint colour of pixel at cursor\n"
"Button 2\tMagnify pixels around cursor\n"
"Other buttons\tExit";
int decimal  = 0;
int multiple = 0;
int help     = 0;

void GetColour(Display *d, Screen *s, int x, int y) {
  XImage *img = XGetImage(d, RootWindowOfScreen(s), x, y, 1, 1,
      AllPlanes, ZPixmap);
  if (!img) {
    fprintf(stderr, "Failed to get image\n");
    return;
  }
  int px = XGetPixel(img, 0, 0);
  if (decimal) {
    printf("%d %d %d\n",
        (px & 0xff0000) >> 16, (px & 0x00ff00) >> 8, (px & 0x0000ff));
  } else {
    printf("%06x\n", px);
  }

  XDestroyImage(img);
}

char test[] = {0x00, 0x00, 0xff, 0xff};
void DrawMagnified(Display *d, Screen *s, Window w, XImage *img,
    int off_x, int off_y, int width, int height) {
  int bpx = (img->bits_per_pixel + 7) / 8;
  int sc_bytes_per_line = bpx * scPx;
  if (width == acPx && height == acPx) {
    /* We got full image, no need to consider offsets and width/height */
    for (int y = 0; y < acPx; y++) {
      int start = y * img->bytes_per_line;
      int startSc = mag * y * sc_bytes_per_line;
      for (int x = 0; x < acPx; x++)
        for (int dup = 0; dup < mag; dup++)
          memcpy(&scaled_data[startSc + mag*bpx*x + dup*bpx],
              &img->data[start + bpx*x], bpx);
      for (int dup = 1; dup < mag; dup++)
        memcpy(&scaled_data[startSc + dup*sc_bytes_per_line],
            &scaled_data[startSc], sc_bytes_per_line);
    }
  } else {
    V(for (int i = 0; i < scPx * scPx; i++)
        memcpy(&scaled_data[bpx*i], test, bpx));
    for (int y = 0; y < height; y++) {
      int start = y * img->bytes_per_line;
      int startSc = mag * (off_y + y) * sc_bytes_per_line;
      for (int x = 0; x < width; x++)
        for (int dup = 0; dup < mag; dup++)
          memcpy(&scaled_data[startSc + mag*bpx*(off_x + x) + dup*bpx],
              &img->data[start + bpx*x], bpx);
      for (int dup = 1; dup < mag; dup++)
        memcpy(&scaled_data[startSc + dup*sc_bytes_per_line],
            &scaled_data[startSc], sc_bytes_per_line);
    }
  }

  XImage *new_img = XCreateImage(d, DefaultVisualOfScreen(s),
      DefaultDepthOfScreen(s), ZPixmap, 0, scaled_data, scPx, scPx,
      img->bitmap_pad, scPx * bpx);
  XPutImage(d, w, DefaultGCOfScreen(s), new_img, 0, 0, 0, 0, scPx, scPx);
  
  XFree(new_img);
}

void GetSizes(int v, int screen_len, int *off, int *len) {
  if (v - acHalf >= 0) {
    *off = 0;
    *len = v + acHalf < screen_len ? acPx : acHalf + screen_len - v;
  } else {
    *off = acHalf - v;
    *len = acPx - *off;
  }
  V(printf("off: %d, len: %d\n", *off, *len));
}

int main(int argc, char **argv) {
  /* Arguments processing */
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];
    int l = strlen(arg);
    if (l > 1 && arg[0] == '-') {
      if (l > 2 && arg[1] == '-') {
        /* Long-form options */
	if (strcmp("decimal", arg+2) == 0)       decimal  = 1;
        else if (strcmp("multiple", arg+2) == 0) multiple = 1;
	else if (strcmp("help", arg+2) == 0)     help     = 1;
        else if (strcmp("factor", arg+2) == 0) {
          fOption:
          if (argc > i+1) {
            mag = atoi(argv[i+1]);
            i++;
          } else fprintf(stderr, "Missing argument.\n");
        } else fprintf(stderr, "Unknown argument: %s\n", arg);
      } else {
        /* Short-form options */
	for (char *p = arg+1; *p; p++) {
	  switch (*p) {
	  case 'd': decimal  = 1; break;
          case 'f': goto fOption;
	  case 'm': multiple = 1; break;
	  case 'h': help     = 1; break;
	  default: fprintf(stderr, "Unknown argument: -%c\n", *p);
	  }
	}
      }
    } else {
      fprintf(stderr, "Unknown argument: %s\n", arg);
    }
  }
  if (help) {
    puts(help_msg);
    return 0;
  }

  /* Check if factor is within limits.
   * Why 1361 specifically? It is the largest number such that size of the
   * scaled_data array (calculated as: 17 * 17 * 4 * mag * mag) will still fit
   * into a 32-bit int.
   * TODO: If int is 16 bit, that already overflows at mag > 5. Should we do
   *       anything to fix that?
   */
  if (mag < 1 || mag > 1361) {
    fprintf(stderr, "Factor %d too %s.\n", mag, mag < 1 ? "small" : "large");
    return 1;
  }
  scPx = mag * acPx;
  scHalf = scPx / 2;
  /* NOTE: This assumes no more than 4 bytes per pixel.
   * I do not know of any display with more than 32-bit colours,
   * so that should be OK.
   */
  scaled_data = calloc(scPx*scPx, 4);
  if (!scaled_data) {
    fprintf(stderr, "Failed to allocate memory for scaled_data.\n");
    return 1;
  }

  Display *d = XOpenDisplay((char*) NULL);
  if (!d) {
    fprintf(stderr, "Failed to open display.\n");
    return 1;
  }
  Screen *s = DefaultScreenOfDisplay(d);
  unsigned long black_px = BlackPixelOfScreen(s);
  unsigned long white_px = WhitePixelOfScreen(s);
  int screen_w = WidthOfScreen(s);
  int screen_h = HeightOfScreen(s);

  XSetWindowAttributes attr = {
    .override_redirect = 1
  };
  Window w = XCreateWindow(d, RootWindowOfScreen(s),
      0, 0, screen_w, screen_h,
      0, CopyFromParent, InputOnly, CopyFromParent,
      CWOverrideRedirect, &attr);
  Window wmag = XCreateWindow(d, RootWindow(d, DefaultScreen(d)),
      0, 0, scPx, scPx, 1,
      CopyFromParent, InputOutput, CopyFromParent,
      CWOverrideRedirect, &attr);

  Cursor c = XCreateFontCursor(d, XC_crosshair);
  XDefineCursor(d, w, c);
  XSetWindowBorder(d, wmag, white_px);

  XSelectInput(d, w, ButtonPressMask);
  XMapWindow(d, w);
  XEvent report;
  int x, y;
  int off_x, off_y, width, height;
  while (1) {
    V(printf("Getting next event...\n"));
    XNextEvent(d, &report);
    if (XFilterEvent(&report, None)) continue;
    switch (report.type) {
    case ButtonPress:
      x = report.xbutton.x;
      y = report.xbutton.y;
      V(printf("Button %d pressed at %d,%d\n", report.xbutton.button, x, y));
      if (report.xbutton.button == 1) {
        GetColour(d, s, x, y);
	if (!multiple) goto done;
      } else if (report.xbutton.button == 3) {
        /* If cursor is at a corner or edge, actual size of image might be
	 * smaller than 16x16. Therefore, calculate:
	 *  - Offsets: at which offset in the (yet unmagnified) 16x16 square
	 *    the image contents are
	 *  - Length: how many pixels of image we can get
	 */
	GetSizes(x, screen_w, &off_x, &width);
	GetSizes(y, screen_h, &off_y, &height);
	V(printf("x, y, w, h: %d, %d, %d, %d\n",
            x - acHalf + off_x, y - acHalf + off_y, width, height));

        /* This is necessary so that if the button was pressed on the
         * magnification window, we get whatever is under that window and
         * not the window itself.
         * TODO: This appears not to fix the problem entirely, just makes
         *       it happen less often.
         */
	XUnmapWindow(d, wmag);
        XSync(d, 0);

        XImage *img = XGetImage(d, RootWindowOfScreen(s),
            x - acHalf + off_x, y - acHalf + off_y, width, height,
	    AllPlanes, ZPixmap);
        if (!img) {
          fprintf(stderr, "Failed to get image\n");
          continue;
        }
        XMapWindow(d, wmag);
	XRaiseWindow(d, w);
	XMoveWindow(d, wmag, x - scHalf, y - scHalf);
        DrawMagnified(d, s, wmag, img, off_x, off_y, width, height);
        XDestroyImage(img);
      } else {
        goto done;
      }
      break;
    }
  }

done:
  free(scaled_data);
  XFreeCursor(d, c);
  XDestroyWindow(d, wmag);
  XDestroyWindow(d, w);
  XCloseDisplay(d);
  return 0;
}

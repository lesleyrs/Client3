#include <math.h>
#include <string.h>

#include "pix2d.h"

Pix2D _Pix2D = {0};

void pix2d_bind(int width, int height, int *pixels) {
    _Pix2D.pixels = pixels;
    _Pix2D.width = width;
    _Pix2D.height = height;
    pix2d_set_clipping(height, width, 0, 0);
}

void pix2d_reset_clipping(void) {
    _Pix2D.left = 0;
    _Pix2D.top = 0;
    _Pix2D.right = _Pix2D.width;
    _Pix2D.bottom = _Pix2D.height;
    _Pix2D.bound_x = _Pix2D.right - 1;
    _Pix2D.center_x = _Pix2D.right / 2;
}

void pix2d_set_clipping(int bottom, int right, int top, int left) {
    if (left < 0) {
        left = 0;
    }

    if (top < 0) {
        top = 0;
    }

    if (right > _Pix2D.width) {
        right = _Pix2D.width;
    }

    if (bottom > _Pix2D.height) {
        bottom = _Pix2D.height;
    }

    _Pix2D.left = left;
    _Pix2D.top = top;
    _Pix2D.right = right;
    _Pix2D.bottom = bottom;
    _Pix2D.bound_x = _Pix2D.right - 1;
    _Pix2D.center_x = _Pix2D.right / 2;
    _Pix2D.center_y = _Pix2D.bottom / 2;
}

void pix2d_clear(void) {
    int length = _Pix2D.width * _Pix2D.height;
    // for (int i = 0; i < length; i++) {
    // 	_Pix2D.pixels[i] = 0;
    // }

    // NOTE: compare performance on diff platforms and entrypoints (playground etc should be better as they use draw_area)
    memset(_Pix2D.pixels, 0, length * sizeof(int));
}

void pix2d_fill_rect(int x, int y, int rgb, int w, int h) {
    if (x < _Pix2D.left) {
        w -= _Pix2D.left - x;
        x = _Pix2D.left;
    }
    if (y < _Pix2D.top) {
        h -= _Pix2D.top - y;
        y = _Pix2D.top;
    }
    if (x + w > _Pix2D.right) {
        w = _Pix2D.right - x;
    }
    if (y + h > _Pix2D.bottom) {
        h = _Pix2D.bottom - y;
    }
    // int off = _Pix2D.width - w;
    int pixel = x + y * _Pix2D.width;
    // for (int col = -h; col < 0; col++) {
    // 	for (int row = -w; row < 0; row++) {
    // 		_Pix2D.pixels[pixel++] = rgb;
    // 	}
    // 	pixel += off;
    // }

    // TODO: compare with above
    for (int col = 0; col < h; col++) {
        int *line = &_Pix2D.pixels[pixel + col * _Pix2D.width];
        for (int row = 0; row < w; row++) {
            line[row] = rgb;
        }
    }
}

void pix2d_draw_rect(int x, int y, int rgb, int w, int h) {
    pix2d_hline(x, y, rgb, w);
    pix2d_hline(x, y + h - 1, rgb, w);
    pix2d_vline(x, y, rgb, h);
    pix2d_vline(x + w - 1, y, rgb, h);
}

void pix2d_fill_circle(int x_center, int y_center, int y_radius, int rgb, int alpha) {
	int inv_alpha = 256 - alpha;
	int r0 = (rgb >> 16 & 0xff) * alpha;
	int g0 = (rgb >> 8 & 0xff) * alpha;
	int b0 = (rgb & 0xff) * alpha;
	int y_start = y_center - y_radius;
	if (y_start < 0) {
		y_start = 0;
	}

	int y_end = y_center + y_radius;
	if (y_end >= _Pix2D.height) {
		y_end = _Pix2D.height - 1;
	}

	for (int y = y_start; y <= y_end; y++) {
		int midpoint = y - y_center;
		int x_radius = (int) sqrt((y_radius * y_radius) - (midpoint * midpoint));
		int x_start = x_center - x_radius;
		if (x_start < 0) {
			x_start = 0;
		}

		int x_end = x_center + x_radius;
		if (x_end >= _Pix2D.width) {
			x_end = _Pix2D.width - 1;
		}

		int offset = x_start + y * _Pix2D.width;
		for (int x = x_start; x <= x_end; x++) {
			int r1 = (_Pix2D.pixels[offset] >> 16 & 0xff) * inv_alpha;
			int g1 = (_Pix2D.pixels[offset] >> 8 & 0xff) * inv_alpha;
			int b1 = (_Pix2D.pixels[offset] & 0xff) * inv_alpha;
			int color = ((r0 + r1) >> 8 << 16) + ((g0 + g1) >> 8 << 8) + ((b0 + b1) >> 8);
			_Pix2D.pixels[offset++] = color;
		}
	}
}

// TODO line memsets
void pix2d_hline(int x, int y, int rgb, int w) {
    if (y < _Pix2D.top || y >= _Pix2D.bottom) {
        return;
    }
    if (x < _Pix2D.left) {
        w -= _Pix2D.left - x;
        x = _Pix2D.left;
    }
    if (x + w > _Pix2D.right) {
        w = _Pix2D.right - x;
    }
    int off = x + y * _Pix2D.width;
    for (int i = 0; i < w; i++) {
        _Pix2D.pixels[off + i] = rgb;
    }
}

void pix2d_vline(int x, int y, int rgb, int h) {
    if (x < _Pix2D.left || x >= _Pix2D.right) {
        return;
    }
    if (y < _Pix2D.top) {
        h -= _Pix2D.top - y;
        y = _Pix2D.top;
    }
    if (y + h > _Pix2D.bottom) {
        h = _Pix2D.bottom - y;
    }
    int off = x + y * _Pix2D.width;
    for (int i = 0; i < h; i++) {
        _Pix2D.pixels[off + i * _Pix2D.width] = rgb;
    }
}

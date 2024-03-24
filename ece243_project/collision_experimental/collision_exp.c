#include <stdlib.h>

volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];	  // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

const int VGA_x = 320;
const int VGA_y = 240;

const int x_min = 0;
const int x_max = 320;
const int y_min = 0;
const int y_max = 240;
const int box_size = 2;
const int N = 8;
const int speed = 3;

void clear_screen();

void draw_line(int x0, int y0, int x1, int x2, short int line_colour);

void swap(int* a, int* b);

int poll_status(volatile int* status_reg);

void initializeDirection(int* arr, int size);

void initializeColour(int* arr, int size);

void initializePosition(int* arr, int size, int range);

void wait_for_vsync();

void draw_frame(int* x_box, int* y_box, short int* boxColour, int N);

void handle_physics(int* x_box, int* y_box, int* dx, int* dy, int N, int speed,
					short int** collision_frame);

void draw_box(int x, int y, int x_size, int y_size, short int color);

// hitbox object, describes top left (x1, y1) and bottom right (x2, y2) corners,
// rest can be determined implicitly TO DO: maybe add rotation value? can be
// used to have angled corners/boxes
struct hitbox {
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
};

const int hitbox_total = 1;
short int collision_frame[240][320];

int main(void) {
	volatile int* pixel_ctrl_ptr = (int*)0xFF203020;
	// declare other variables(not shown)
	// initialize location and direction of rectangles(not shown)

	/* set front pixel buffer to Buffer 1 */
	*(pixel_ctrl_ptr + 1) =
		(int)&Buffer1;	// first store the address in the  back buffer
	/* now, swap the front/back buffers, to set the front buffer location */
	wait_for_vsync();

	/* initialize a pointer to the pixel buffer, used by drawing functions */
	pixel_buffer_start = *pixel_ctrl_ptr;
	clear_screen();	 // pixel_buffer_start points to the pixel buffer

	/* set back pixel buffer to Buffer 2 */
	*(pixel_ctrl_ptr + 1) = (int)&Buffer2;
	pixel_buffer_start = *(pixel_ctrl_ptr + 1);	 // we draw on the back buffer
	clear_screen();	 // pixel_buffer_start points to the pixel buffer

	const unsigned short int color_list[] = {0xffff, 0xf800, 0x07e0, 0x001f,
											 0xf0b0, 0x0f0b, 0xf22f, 0x2ff2,
											 0x22ff, 0xff22};

	struct hitbox colliders[hitbox_total];

	// randomize locations of hitboxes and their sizes
	colliders[0].x1 = 50;
	colliders[0].y1 = 50;
	colliders[0].x2 = 150;
	colliders[0].y2 = 150;
	/*for (int k = 0; k < hitbox_total; k++) {
		colliders[k].x1 = ((rand() % x_max/2));
		colliders[k].y1 = ((rand() % y_max/2));

		// ensure x2 is GREATER than x1
		while (colliders[k].x2 <= colliders[k].x1 && colliders[k].x2 < x_max) {
			colliders[k].x2 = ((rand() % x_max/2) + x_max/2);
		}
		// same for y2 and y1
		while (colliders[k].y2 <= colliders[k].y1 && colliders[k].y2 < y_max) {
			colliders[k].y2 = ((rand() % y_max/2) + y_max/2);
		}
	}
	*/

	// fill in hitbox mask to show where boxes CANNOT exist in
	for (int k = 0; k < hitbox_total; k++) {
		for (int x_coord = colliders[k].x1; x_coord < colliders[k].x2;
			 x_coord++) {
			for (int y_coord = colliders[k].y1; y_coord < colliders[k].y2;
				 y_coord++) {
				collision_frame[x_coord][y_coord] = 1;
			}
		}
	}

	int x_box[N];
	int y_box[N];
	int old_x[N];
	int old_y[N];
	int dx[N];
	int dy[N];
	short int boxColour[N];

	// initialize variables (positions, directions, colors)
	initializePosition(x_box, N, 320);
	initializePosition(y_box, N, 240);
	initializeDirection(dx, N);
	initializeDirection(dy, N);
	for (int k = 0; k < N; k++) {
		boxColour[k] = color_list[rand() % 10];
	}
	while (1) {
		// wait for vsync and update buffer positions
		wait_for_vsync();
		pixel_buffer_start = *(pixel_ctrl_ptr + 1);

		// erase old boxes
		draw_frame(old_x, old_y, NULL, N);

		// move boxes and update old positions
		for (int k = 0; k < N; k++) {
			old_x[k] = x_box[k];
			old_y[k] = y_box[k];
		}
		handle_physics(x_box, y_box, dx, dy, N, speed, collision_frame);

		// draw hitboxes
		for (int k = 0; k < hitbox_total; k++) {
			draw_box(colliders[k].x1, colliders[k].y1,
					 colliders[k].x2 - colliders[k].x1,
					 colliders[k].y2 - colliders[k].y1, color_list[1]);
		}

		// draw new boxes at shifted positions
		draw_frame(x_box, y_box, boxColour, N);
	}
}

void handle_physics(int* x_box, int* y_box, int* dx, int* dy, int N, int speed,
					short int** collision_frame) {
	// for each box...
	for (int k = 0; k < N; k++) {
		// boundary checks
		if (x_box[k] + (speed * dx[k]) <= x_min ||
			x_box[k] + (speed * dx[k]) >= x_max) {
			// redirect dx
			dx[k] *= -1;
		}
		if (y_box[k] + (speed * dy[k]) <= y_min ||
			y_box[k] + (speed * dy[k]) >= y_max) {
			// redirect dy
			dy[k] *= -1;
		}

		// TO DO: ADD COLLISION CHECKS
		if(x_box[k] + (speed * dx[k]))

		// move point
		x_box[k] += dx[k] * speed;
		y_box[k] += dy[k] * speed;
	}
}

// to erase, set boxColour to null
void draw_frame(int* x_box, int* y_box, short int* boxColour, int N) {
	for (int k = 0; k < N - 1; k++) {
		if (boxColour) {
			draw_line(x_box[k], y_box[k], x_box[k + 1], y_box[k + 1],
					  boxColour[k]);
			draw_box(x_box[k], y_box[k], box_size, box_size, boxColour[k]);
		} else if (boxColour == NULL) {
			draw_line(x_box[k], y_box[k], x_box[k + 1], y_box[k + 1], 0);
			draw_box(x_box[k], y_box[k], box_size, box_size, 0);
		}
	}
	// draw final line
	if (boxColour) {
		draw_line(x_box[0], y_box[0], x_box[N - 1], y_box[N - 1], boxColour[N]);
		draw_box(x_box[N - 1], y_box[N - 1], box_size, box_size, boxColour[N]);
	} else if (boxColour == NULL) {
		draw_line(x_box[0], y_box[0], x_box[N - 1], y_box[N - 1], 0);
		draw_box(x_box[N - 1], y_box[N - 1], box_size, box_size, 0);
	}
}

void draw_box(int x, int y, int x_size, int y_size, short int color) {
	for (int a = x; a < x + x_size + 1; a++) {
		for (int b = y; b < y + y_size + 1; b++) {
			plot_pixel(a, b, color);
		}
	}
}

void wait_for_vsync() {
	volatile int* pixel_ctrl_ptr = (int*)0xff203020;
	int status;
	*pixel_ctrl_ptr = 1;
	status = *(pixel_ctrl_ptr + 3);
	while ((status & 0b01) != 0) {
		status = *(pixel_ctrl_ptr + 3);
	}
	// should exit when vsync occurs!
}

void initializePosition(int* arr, int size, int range) {
	for (int k = 0; k < size; k++) {
		arr[k] = ((rand() % range));
	}
}

void initializeDirection(int* arr, int size) {
	for (int k = 0; k < size; k++) {
		arr[k] = ((rand() % 2) * 2) - 1;
	}
}

int poll_status(volatile int* status_reg) {
	// extract S bit (bit 0 of status reg)
	return *(status_reg) & 0b01;
}

void clear_screen() {
	for (int x = 0; x < VGA_x; x++) {
		for (int y = 0; y < VGA_y; y++) {
			plot_pixel(x, y, 0x0);
		}
	}
}

// largely from lab document
void draw_line(int x0, int y0, int x1, int y1, short int line_colour) {
	int is_steep = abs(y1 - y0) > abs(x1 - x0);

	if (is_steep) {
		swap(&x0, &y0);
		swap(&x1, &y1);
	}
	if (x0 > x1) {
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	int deltaX = x1 - x0;
	int deltaY = abs(y1 - y0);
	int error = -(deltaX / 2);
	int y = y0;
	int y_step;
	if (y0 < y1) {
		y_step = 1;
	} else {
		y_step = -1;
	}

	for (int x = x0; x < x1; x++) {
		if (is_steep) {
			plot_pixel(y, x, line_colour);
		} else {
			plot_pixel(x, y, line_colour);
		}
		error = error + deltaY;
		if (error > 0) {
			y = y + y_step;
			error = error - deltaX;
		}
	}
}

void swap(int* a, int* b) {
	int tmp = *a;
	*a = *b;
	*b = tmp;
}

void plot_pixel(int x, int y, short int line_color) {
	volatile short int* one_pixel_address;

	one_pixel_address = pixel_buffer_start + (y << 10) + (x << 1);

	*one_pixel_address = line_color;
}
#include <bits/types/struct_timeval.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <sys/select.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>


#define LEVEL_FILE "level1"
#define PLAYER_SPRITE "default.sprite"


/* TODO
 * - Make renderer separate structure that is passed into functions
 * - Make physics engine
 * - Add collisions
 * - Refactor level loading
*  - Add animations (swapping between sprites when velocity is 0)
*  - Refactor input handling and movement system
*/
// Goal of this file is to create a simple level 

// Maybe make the level dynamically generated based off another file
// Maybe make a level editor


typedef struct {
	int x;
	int y;
} vector2;

typedef struct {
	char ** texture;
	uint lengthpx;
	uint heightpx;
} sprite;

typedef struct {
	sprite texture;

} level;

typedef struct {
	vector2 position;
	vector2 dimensions;
} hitbox;

typedef struct {
	sprite sprite;
	vector2 position;
	vector2 velocity;
	vector2 acceleration;
} player;


struct winsize getWindowSize();

int load_level(char * file_name, level *level_data);
int print_level(level *level_data, struct winsize ws);
int render_pixels(vector2 pos, char * pixels, struct winsize ws);
void move_cursor_to_pos(vector2 pos, struct winsize ws);
void disable_canonical_mode();
int check_input();
int render_sprite(sprite *sprite, vector2 pos, struct winsize ws);
int init_player(player *p, struct winsize ws, level level_data);
int load_sprite_texture(sprite *sprite, char *filename);
int max(int *arr, int len);


int main(int argc, char ** argv) {

	system("stty -echo");
	disable_canonical_mode();
	// Should use another thread for input handling


	// struct winsize ws = getWindowSize();
	struct winsize ws;
	if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
		return 1;
	}

	int wlength = ws.ws_col;
	int wheight = ws.ws_row;


	level level_data = {0};

	load_level(LEVEL_FILE, &level_data);

	// Recursively initializes the player on the stack
	player player1 = {0};
	init_player(&player1, ws, level_data);

	while (true) {

		print_level(&level_data, ws);

		if (check_input() != 0) {
			char input = getchar();
			switch (input) {
				case 'a':
					 player1.position.x -= 1;
					break;
				case 'd':
					player1.position.x += 1;
					break;
				case 'w':
					player1.position.y += 1;
					break;
				case 's':
					player1.position.y -= 1;
					break;
				default:
					break;
			}
		}

		render_sprite(&player1.sprite, player1.position, ws);
		fflush(stdout);
		printf("\033[1;1H\r");

		struct timespec sleeptime;
		sleeptime.tv_sec = 0;
		sleeptime.tv_nsec = 10 * 1000000;
		nanosleep(&sleeptime, &sleeptime);
	}
	
	// Move cursor up n lines to overwrite screen
	// printf("\033[%dA\r", wheight);
	//
	//
	// fflush(stdout);
	// sleep(1);

	system("stty echo");

}


int init_player(player *p, struct winsize ws, level level_data) {
	load_sprite_texture(&p->sprite, PLAYER_SPRITE);

	// Set default position for player
	vector2 *pos = &p->position;
	pos->x = 10 + (ws.ws_col - level_data.texture.lengthpx) / 2;
	pos->y = 2;

	return 0;
}

int max(int *arr, int len) {
	if (len == 0) {
		return 0; 
	}

	int max = arr[0];
	for (int i = 1; i < len; i++) {
		if (max < arr[i]) {
			max = arr[i];
		}
	}

	return max;
}
 
int load_sprite_texture(sprite *sprite, char *filename) {
	FILE *sprite_file;
	sprite_file = fopen(filename, "r");

	if (sprite_file == NULL) {
		printf("error opening sprite file");
		return 1;
	}

	// Find size of file
	if (fseek(sprite_file, 0L, SEEK_END) != 0) {
		perror("Error seeking to EOF");
		return 2;
	}
	long size = ftell(sprite_file);

	// Reset to start of file
	fseek(sprite_file, 0L, SEEK_SET);

	char *file_buffer = malloc(size + 1);

	if (fread(file_buffer, 1, size, sprite_file) < size) {
		printf("error reading file\n");
		return 3;
	}
	file_buffer[size] = '\0';

	// Count lines and line lengths
	int line_count = 0;
	for (int i = 0; i < size; i++) {
		char currchar = file_buffer[i];
		if (currchar == '\n') {
			line_count++;
		}
	}

	int line_lengths[line_count];
	int curr_line = 0;
	int line_length = 0;
	for (int i = 0; i < size; i++) {
		char currchar = file_buffer[i];
		if (currchar == '\n' || currchar == '\0') {
			// Save length of this line
			line_lengths[curr_line] = line_length;

			// Reset
			line_length = 0;
			curr_line++;
		} else {
			line_length++;
		}
	}

	// Only really want largest line length
	int max_line_length = max(line_lengths, line_count);

	// Format sprite into 2D array
	char **texture = malloc(sizeof(char *) * line_count);
	int pos = 0;
	for (int i = 0; i < line_count; i++) {
		char *line = malloc(sizeof(char) * (max_line_length + 1));
		int curr_line_len = line_lengths[i];
		// Fill up the line
		for (int j = 0; j < curr_line_len; j++) {
			char currchar = file_buffer[pos];
			line[j] = currchar;
			pos++;
		}

		for (int j = curr_line_len; j < max_line_length; j++) {
			line[j] = ' ';
		}

		pos++; // To skip newline


		line[max_line_length] = '\0';

		texture[i] = line;
	}


	sprite->texture = texture;
	sprite->heightpx = line_count;
	sprite->lengthpx = max_line_length;


	// Cleanup
	free(file_buffer);
	fclose(sprite_file);

	return 0;
}

 int load_level(char * file_name, level *level_data) {
	load_sprite_texture(&level_data->texture, LEVEL_FILE);
	return 0;
}

int pad_left(int px) {
	if (px < 0) {
		return 1;
	}

	for (int i = 0; i < px; i++) {
		printf(" ");
	}

	return 0;
}

int pad_top_bottom(int px) {
	if (px < 0) {
		return 1;
	}
	for (int i = 0; i < px; i++) {
		printf("\n");
	}

	return 0;
}

int print_level(level *level_data, struct winsize ws) {
	pad_top_bottom(ws.ws_row - level_data->texture.heightpx);
	for (int i = 0; i < level_data->texture.heightpx; i++) {
		char * row = level_data->texture.texture[i];
		pad_left((ws.ws_col - level_data->texture.lengthpx)/2);
		printf("%s", row);
		if ((i + 1) != level_data->texture.heightpx) {
			printf("\n");
		}
	}	

	return 0;
}

int render_pixels(vector2 pos, char * pixels, struct winsize ws) {
	move_cursor_to_pos(pos, ws);
	printf("%s", pixels);

	return 0;
}



void move_cursor_to_pos(vector2 pos, struct winsize ws) {
	// Convert vector2 into screen coordinates
	// ANSI Escape codes think 1,1 is top left
	// Want 0,0 to be bottom left of our map
	int x = pos.x + 1;
	int y = ws.ws_row - pos.y - 1;
	printf("\033[%d;%dH", y, x);
}

void disable_canonical_mode() {
	struct termios attr;
	tcgetattr(STDIN_FILENO, &attr);

	attr.c_lflag &= ~(ICANON | ECHO);

	tcsetattr(STDIN_FILENO, TCSANOW, &attr);
}

int check_input() {
	fd_set file_descriptor_set;
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	FD_ZERO(&file_descriptor_set);
	FD_SET(STDIN_FILENO, &file_descriptor_set);

	if (select(STDIN_FILENO + 1, &file_descriptor_set, NULL, NULL, &timeout) == -1) {
		return 0;
	}

	return FD_ISSET(STDIN_FILENO, &file_descriptor_set);
}

int render_sprite(sprite *sprite, vector2 pos, struct winsize ws) {
	vector2 temppos;
	temppos.x = pos.x;
	temppos.y = pos.y;
	char **texture = sprite->texture;
	int heightpx = sprite->heightpx;
	for (int i = heightpx - 1; i >= 0; i--) {
		render_pixels(temppos, texture[i], ws);
		temppos.y++;
	}

	return 0;
}

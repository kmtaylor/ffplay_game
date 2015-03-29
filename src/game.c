#include <pthread.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#include <SDL.h>

#include <libavformat/avformat.h>
#include <libavcodec/avfft.h>

#include "ffplay.h"

#define PACKET_HEADER 0xff
#define PACKET_SIZE 3

#define UART_SPEED 115200
#define UART_NAME "/dev/ttyS0"

typedef struct s_control_packet control_packet;

struct s_control_packet {
    uint8_t instruction;
    uint8_t value;

    control_packet *next;
};

struct s_control_data {
    pthread_mutex_t lock;
    pthread_cond_t data_ready;

    control_packet *packet_list;

    int start_game;

    uint8_t cur_instruction;
    int fd;
};

static struct s_control_data control_data = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .data_ready = PTHREAD_COND_INITIALIZER,

    .packet_list = NULL,
    .start_game = 1,
};

#define ATTRACT_VIDEO_STREAM	0
#define ATTRACT_AUDIO_STREAM	1
#define COUNTDOWN_VIDEO_STREAM	2
#define COUNTDOWN_AUDIO_STREAM	3
#define GAME_VIDEO_STREAM	4
#define GAME_AUDIO_STREAM	5
#define WINNER1_VIDEO_STREAM	6
#define WINNER1_AUDIO_STREAM	7
#define WINNER2_VIDEO_STREAM	8
#define WINNER2_AUDIO_STREAM	9

#define GAME_MODE(mode_name, video_stream, audio_stream) \
    static void mode_name##_mode(VideoState *is) {			    \
	stream_component_close(is, is->last_video_stream);		    \
	stream_component_close(is, is->last_audio_stream);		    \
	stream_component_open(is, video_stream);			    \
	stream_component_open(is, audio_stream);			    \
	stream_seek(is, 0, 0, 0);					    \
    }

GAME_MODE(attract, ATTRACT_VIDEO_STREAM, ATTRACT_AUDIO_STREAM)
GAME_MODE(countdown, COUNTDOWN_VIDEO_STREAM, COUNTDOWN_AUDIO_STREAM)
GAME_MODE(game, GAME_VIDEO_STREAM, GAME_AUDIO_STREAM)
GAME_MODE(winner1, WINNER1_VIDEO_STREAM, WINNER1_AUDIO_STREAM)
GAME_MODE(winner2, WINNER2_VIDEO_STREAM, WINNER2_AUDIO_STREAM)

void setup_uart(void) {
    int fd;
    struct termios t;

    fd = open(UART_NAME, O_RDWR | O_NONBLOCK | O_NOCTTY );
    if (fd < 0) return;

    /* fcntl(fd, F_SETFL, O_RDWR); turn off blocking */
    tcgetattr(fd, &t);
    cfsetospeed(&t, UART_SPEED);
    cfsetispeed(&t, UART_SPEED);
    t.c_iflag &= ~( INPCK | ISTRIP | IGNCR | ICRNL | INLCR | IXON | IXOFF |
		    IXANY | IMAXBEL ); // Basically turn everything off
    t.c_iflag |= IGNBRK; // Ignore break condition
    t.c_oflag &= ~OPOST; // Transmit ouput as is
    t.c_cflag &= ~( HUPCL | CSTOPB | PARENB | CRTSCTS );
    t.c_cflag |= ( CLOCAL | CREAD | CS8 );
    t.c_lflag &= ~( ICANON | ECHO | ISIG | IEXTEN | TOSTOP );
    t.c_lflag |= NOFLSH;
    tcsetattr(fd, TCSANOW, &t);
    control_data.fd = fd;
}

void write_uart(uint8_t instruction, uint8_t value) {
    int nbytes = 0, written;
    uint8_t data[PACKET_SIZE];

    data[0] = PACKET_HEADER;
    data[1] = instruction;
    data[2] = value;

    while (nbytes < PACKET_SIZE) {
	written = write(control_data.fd, &data[nbytes], 1);
	if (written < 0) break;
	nbytes += written;
    }
}

void read_uart(void) {
    static int byte_no;
    uint8_t data;
    control_packet *new_packet;

    if (read(control_data.fd, &data, 1) < 1) return;

    if (data == PACKET_HEADER) byte_no = 0;
    else byte_no++;

    if (byte_no == 1) {
	control_data.cur_instruction = data;
    } else if (byte_no == 2) {
	/* Add to linked list */
	pthread_mutex_lock(&control_data.lock);

	if (!control_data.packet_list) {
	    control_data.packet_list = malloc(sizeof(struct s_control_packet));
	    new_packet = control_data.packet_list;
	} else {
	    new_packet = control_data.packet_list;
	    while (new_packet->next) new_packet = new_packet->next;
	    new_packet->next = malloc(sizeof(struct s_control_packet));
	    new_packet = new_packet->next;
	}
	new_packet->instruction = control_data.cur_instruction;
	new_packet->value = data;
	new_packet->next = NULL;

	pthread_cond_signal(&control_data.data_ready);
	pthread_mutex_unlock(&control_data.lock);
    }
}

static void *control_func(void *p) {
    fd_set readfds, loopfds;

    FD_ZERO(&readfds);
    FD_SET(control_data.fd, &readfds);

    while (1) {
	loopfds = readfds;
	select(FD_SETSIZE, &loopfds, NULL, NULL, NULL);
	
	if (FD_ISSET(control_data.fd, &loopfds)) {
	    read_uart();
	}
    }

    return NULL;
}

enum state_enum {
    ATTRACT_MODE, GAME_MODE, COUNTDOWN_MODE, WINNER1_MODE, WINNER2_MODE
};

static void sleep_mode(enum state_enum state) {
    switch (state) {
	case ATTRACT_MODE:
	    sleep(17);
	    break;
	case COUNTDOWN_MODE:
	    sleep(8);
	    break;
	case GAME_MODE:
	    sleep(10);
	    break;
	case WINNER1_MODE:
	    sleep(5);
	    break;
	case WINNER2_MODE:
	    sleep(5);
	    break;
    }
}

static int choose_winner(void) {
    return 0;
}

static enum state_enum get_game_state(enum state_enum old_state) {
    switch (old_state) {
	case ATTRACT_MODE:
	    pthread_mutex_lock(&control_data.lock);
	    if (control_data.start_game) {
		pthread_mutex_unlock(&control_data.lock);
		return COUNTDOWN_MODE;
	    } else {
		pthread_mutex_unlock(&control_data.lock);
		return ATTRACT_MODE;
	    }
	case COUNTDOWN_MODE:
	    return GAME_MODE;
	case GAME_MODE:
	    if (choose_winner()) return WINNER1_MODE;
	    else return WINNER2_MODE;
	case WINNER1_MODE:
	case WINNER2_MODE:
	    return ATTRACT_MODE;
    }
    return ATTRACT_MODE;
}

static void *game_func(void *is_p) {
    VideoState *is = (VideoState *) is_p;
    static enum state_enum state = ATTRACT_MODE;

    sleep_mode(state);

    while (1) {
	state = get_game_state(state);
	switch (state) {
	    case ATTRACT_MODE:
		attract_mode(is);
		sleep_mode(state);
		break;

	    case COUNTDOWN_MODE:
		countdown_mode(is);
		sleep_mode(state);
		break;

	    case GAME_MODE:
		game_mode(is);
		/* draw bar graphs (in a new thread) */
		sleep_mode(state);
		break;

	    case WINNER1_MODE:
		winner1_mode(is);
		sleep_mode(state);
		break;

	    case WINNER2_MODE:
		winner2_mode(is);
		sleep_mode(state);
		break;
	}
    }

    return NULL;
}

int main(void) {
    pthread_t game_thread;
    pthread_t control_thread;
    VideoState *is;

    wanted_stream[AVMEDIA_TYPE_AUDIO] = ATTRACT_AUDIO_STREAM;
    wanted_stream[AVMEDIA_TYPE_VIDEO] = ATTRACT_VIDEO_STREAM;

    //setup_uart();

    is = ffplay_init("../resource/media.mp4");
    
    pthread_create(&game_thread, NULL, game_func, is);
    pthread_create(&control_thread, NULL, control_func, NULL);

    ffplay_event_loop(is);

    return 0;
}

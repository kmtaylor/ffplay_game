/* SDL audio buffer size, in samples. Should be small to have precise
   A/V sync as SDL does not have hardware buffer fullness info. */
#define SDL_AUDIO_BUFFER_SIZE 1024

/* NOTE: the size must be big enough to compensate the hardware audio buffersize size */
/* TODO: We assume that a decoded and resampled frame fits into this buffer */
#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define VIDEO_PICTURE_QUEUE_SIZE 4
#define SUBPICTURE_QUEUE_SIZE 4

typedef struct AudioParams {
    int freq;
    int channels;
    int64_t channel_layout;
    enum AVSampleFormat fmt;
} AudioParams;

typedef struct MyAVPacketList {
    AVPacket pkt;
    struct MyAVPacketList *next;
    int serial;
} MyAVPacketList;

typedef struct PacketQueue {
    MyAVPacketList *first_pkt, *last_pkt;
    int nb_packets;
    int size;
    int abort_request;
    int serial;
    SDL_mutex *mutex;
    SDL_cond *cond;
} PacketQueue;

typedef struct SubPicture {
    double pts; /* presentation time stamp for this picture */
    AVSubtitle sub;
} SubPicture;

typedef struct VideoPicture {
    double pts;             // presentation timestamp for this picture
    int64_t pos;            // byte position in file
    SDL_Overlay *bmp;
    int width, height; /* source height & width */
    AVRational sample_aspect_ratio;
    int allocated;
    int reallocate;
    int serial;
} VideoPicture;

typedef struct VideoState {
    SDL_Thread *read_tid;
    SDL_Thread *video_tid;
    AVInputFormat *iformat;
    int no_background;
    int abort_request;
    int force_refresh;
    int paused;
    int last_paused;
    int queue_attachments_req;
    int seek_req;
    int seek_flags;
    int64_t seek_pos;
    int64_t seek_rel;
    int read_pause_return;
    AVFormatContext *ic;
    int realtime;

    int audio_stream;

    int av_sync_type;
    double external_clock;                   ///< external clock base
    double external_clock_drift;             ///< external clock base - time (av_gettime) at which we updated external_clock
    int64_t external_clock_time;             ///< last reference time
    double external_clock_speed;             ///< speed of the external clock

    double audio_clock;
    int audio_clock_serial;
    double audio_diff_cum; /* used for AV difference average computation */
    double audio_diff_avg_coef;
    double audio_diff_threshold;
    int audio_diff_avg_count;
    AVStream *audio_st;
    PacketQueue audioq;
    int audio_hw_buf_size;
    uint8_t silence_buf[SDL_AUDIO_BUFFER_SIZE];
    uint8_t *audio_buf;
    uint8_t *audio_buf1;
    unsigned int audio_buf_size; /* in bytes */
    unsigned int audio_buf1_size;
    int audio_buf_index; /* in bytes */
    int audio_write_buf_size;
    AVPacket audio_pkt_temp;
    AVPacket audio_pkt;
    int audio_pkt_temp_serial;
    struct AudioParams audio_src;
    struct AudioParams audio_tgt;
    struct SwrContext *swr_ctx;
    double audio_current_pts;
    double audio_current_pts_drift;
    int frame_drops_early;
    int frame_drops_late;
    AVFrame *frame;

    enum ShowMode {
        SHOW_MODE_NONE = -1, SHOW_MODE_VIDEO = 0, SHOW_MODE_WAVES, SHOW_MODE_RDFT, SHOW_MODE_NB
    } show_mode;
    int16_t sample_array[SAMPLE_ARRAY_SIZE];
    int sample_array_index;
    int last_i_start;
    RDFTContext *rdft;
    int rdft_bits;
    FFTSample *rdft_data;
    int xpos;
    double last_vis_time;

    SDL_Thread *subtitle_tid;
    int subtitle_stream;
    int subtitle_stream_changed;
    AVStream *subtitle_st;
    PacketQueue subtitleq;
    SubPicture subpq[SUBPICTURE_QUEUE_SIZE];
    int subpq_size, subpq_rindex, subpq_windex;
    SDL_mutex *subpq_mutex;
    SDL_cond *subpq_cond;

    double frame_timer;
    double frame_last_pts;
    double frame_last_duration;
    double frame_last_dropped_pts;
    double frame_last_returned_time;
    double frame_last_filter_delay;
    int64_t frame_last_dropped_pos;
    int video_stream;
    AVStream *video_st;
    PacketQueue videoq;
    double video_current_pts;       // current displayed pts
    double video_current_pts_drift; // video_current_pts - time (av_gettime) at which we updated video_current_pts - used to have running video pts
    int64_t video_current_pos;      // current displayed file pos
    double max_frame_duration;      // maximum duration of a frame - above this, we consider the jump a timestamp discontinuity
    int video_clock_serial;
    VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
    int pictq_size, pictq_rindex, pictq_windex;
    SDL_mutex *pictq_mutex;
    SDL_cond *pictq_cond;
    struct SwsContext *img_convert_ctx;
    SDL_Rect last_display_rect;

    char filename[1024];
    int width, height, xleft, ytop;
    int step;

    int last_video_stream, last_audio_stream, last_subtitle_stream;

    SDL_cond *continue_read_thread;
} VideoState;

extern const char *input_filename;
extern SDL_Surface *screen;
extern int display_disable;
extern int audio_disable;
extern int video_disable;
extern AVPacket flush_pkt;
extern AVInputFormat *file_iformat;
extern int wanted_stream[AVMEDIA_TYPE_NB];

extern void sigterm_handler(int sig);
extern int lockmgr(void **mtx, enum AVLockOp op);
extern void do_exit(VideoState *is);

extern VideoState *stream_open(const char *filename, AVInputFormat *iformat);
extern void ffplay_event_loop(VideoState *cur_stream);
extern VideoState *ffplay_init(const char *resource_file);

extern void stream_cycle_channel(VideoState *is, int codec_type);
extern void stream_component_close(VideoState *is, int stream_index);
extern int stream_component_open(VideoState *is, int stream_index);

extern void stream_seek(VideoState *is, int64_t pos, int64_t rel, 
		int seek_by_bytes);
extern int video_open(VideoState *is, int force_set_video_mode, 
		VideoPicture *vp);

extern void frame_modify_hook(SDL_Overlay *overlay);
